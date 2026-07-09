#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "dpapp/dpbuf.h"
#include "limits.h"
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <memory.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static inline int _iclamp(int v, int l, int h)
{
    int t = v < l ? l : v;
    return t < h ? t : h;
}

static inline int _imin(int a, int b)
{
    return a < b ? a : b;
}

static inline int _imax(int a, int b)
{
    return a < b ? b : a;
}

struct dpbuf_data
{
    char* data;
    int maxs;
    int flag;
    atomic_size_t refc;
};

static dpbuf_data_t* _dpbuf_data_new(int size)
{
    if (size < 0 || size >= INT_MAX) {
        return NULL;
    }

    dpbuf_data_t* self = (dpbuf_data_t*)malloc(sizeof(dpbuf_data_t));
    if (self == NULL) {
        return NULL;
    }

    self->data = malloc(size + 1);
    if (self->data == NULL) {
        free(self);
        return NULL;
    }

    self->data[0] = '\0';
    self->data[size] = '\0';
    self->maxs = size;
    self->flag = 0;
    atomic_init(&self->refc, 1);
    return self;
}

static dpbuf_data_t* _dpbuf_data_new_d(void* data, int size, int mode)
{
    if (data == NULL) {
        return NULL;
    }

    dpbuf_data_t* self = (dpbuf_data_t*)malloc(sizeof(dpbuf_data_t));
    if (self == NULL) {
        return NULL;
    }

    size = size < 0 ? strlen(data) : size;
    self->maxs = size;

    if (mode & DPBUF_DUP_DATA) {
        self->data = malloc(size + 1);
        if (self->data == NULL) {
            free(self);
            return NULL;
        }
        memcpy(self->data, data, size);
        self->data[size] = 0;
        self->flag = 0;
    } else {
        self->data = data;
        self->flag = mode & DPBUF_FLAGMASK;
    }
    atomic_init(&self->refc, 1);
    return self;
}

static void _dpbuf_data_del(dpbuf_data_t* self)
{
    if (atomic_fetch_sub_explicit(&self->refc, 1, memory_order_acq_rel) == 1) {
        if (!(self->flag & DPBUF_CONSTDATA) && self->data) {
            free(self->data);
        }
        free(self);
    }
}

static inline void _dpbuf_data_ref(dpbuf_data_t* self)
{
    atomic_fetch_add_explicit(&self->refc, 1, memory_order_acquire);
}

static inline int _pow_min_value(int value)
{
    if (value == 0) {
        return 1;
    }
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value++;
    return value;
}

static bool _dpbuf_data_resize(dpbuf_data_t* self, int size)
{
    if (self->maxs > size) {
        self->data[size] = '\0';
        return true;
    }

    if (self->flag & DPBUF_CONSTDATA) {
        // 不能对常量数据 realloc
        return false;
    }

    if (size < 0 || size >= INT_MAX) {
        return false;
    }

    int newsz = _pow_min_value(size + 1);
    if (newsz > DPBUF_MAX_SIZE) {
        // pow_min_value 失败（值过大）
        return false;
    }
    char* tmp = (char*)realloc(self->data, newsz);
    if (tmp == NULL) {
        // realloc 失败，但旧数据不受影响
        return false;
    }
    self->data = tmp;
    self->maxs = newsz;
    self->data[size] = '\0';
    return true;
}

bool dpbuf_init(dpbuf_t* self, int size)
{
    if (self == NULL || size < 0) {
        return false;
    }
    memset(self, 0, sizeof(dpbuf_t));
    self->data = _dpbuf_data_new(size);
    if (self->data == NULL) {
        return false;
    }
    self->size = size;
    return true;
}

bool dpbuf_init_d(dpbuf_t* self, void* data, int size, int mode)
{
    if (self == NULL) {
        return false;
    }
    memset(self, 0, sizeof(dpbuf_t));
    self->data = _dpbuf_data_new_d(data, size, mode);
    if (self->data == NULL) {
        return false;
    }
    self->size = self->data->maxs;

    if (mode & DPBUF_INIT_R) {
        self->rend = self->wbeg = self->size;
    }

    return true;
}

bool dpbuf_init_v(dpbuf_t* self, const char* fmt, va_list args)
{
    if (self == NULL || fmt == NULL) {
        return false;
    }

    va_list args2;
    va_copy(args2, args);
    int rsz = vsnprintf(NULL, 0, fmt, args2);
    va_end(args2);
    if (rsz < 0) {
        return false;
    }

    if (!dpbuf_init(self, rsz)) {
        return false;
    }

    vsnprintf(self->data->data, rsz + 1, fmt, args);
    self->rend = self->wbeg = self->size;
    return true;
}

bool dpbuf_init_f(dpbuf_t* self, const char* fmt, ...)
{
    if (self == NULL || fmt == NULL) {
        return false;
    }

    va_list args;
    va_start(args, fmt);
    bool ok = dpbuf_init_v(self, fmt, args);
    va_end(args);
    return ok;
}

bool dpbuf_init_r(dpbuf_t* self, const dpbuf_t* other)
{
    if (self == NULL) {
        return false;
    }
    if (other == NULL || other->data == NULL) {
        return dpbuf_init(self, 0);
    }

    memcpy(self, other, sizeof(dpbuf_t));
    _dpbuf_data_ref(other->data);
    self->wbeg = self->rend;
    self->size = self->rend;
    dpbuf_cpusr(other, self);
    return true;
}

bool dpbuf_from_r(dpbuf_t* self, const dpbuf_t* other)
{
    if (self == NULL) {
        return false;
    }
    if (other == NULL || dpbuf_crsize(other) == 0) {
        return dpbuf_init(self, 0);
    }

    bool ok = dpbuf_init_d(self, dpbuf_crdata(other), dpbuf_crsize(other),
        DPBUF_DUP_DATA | DPBUF_INIT_R);
    if (ok) {
        dpbuf_cpusr(other, self);
    }
    return ok;
}

bool dpbuf_from_e(dpbuf_t* self, const dpbuf_t* other)
{
    if (self == NULL) {
        return false;
    }
    if (other == NULL || dpbuf_cesize(other) == 0) {
        return dpbuf_init(self, 0);
    }

    bool ok = dpbuf_init_d(self, dpbuf_cedata(other), dpbuf_cesize(other),
        DPBUF_DUP_DATA | DPBUF_INIT_R);
    if (ok) {
        dpbuf_cpusr(other, self);
    }
    return ok;
}

dpbuf_t* dpbuf_new(int size)
{
    dpbuf_t* self = (dpbuf_t*)malloc(sizeof(dpbuf_t));
    if (!dpbuf_init(self, size)) {
        if (self) {
            free(self);
            self = NULL;
        }
    }
    return self;
}

dpbuf_t* dpbuf_new_d(void* data, int size, int mode)
{
    dpbuf_t* self = (dpbuf_t*)malloc(sizeof(dpbuf_t));
    if (!dpbuf_init_d(self, data, size, mode)) {
        if (self) {
            free(self);
            self = NULL;
        }
    }
    return self;
}

dpbuf_t* dpbuf_new_v(const char* fmt, va_list args)
{
    if (fmt == NULL) {
        return NULL;
    }

    va_list args2;
    va_copy(args2, args);
    int rsz = vsnprintf(NULL, 0, fmt, args2);
    va_end(args2);

    if (rsz >= 0) {
        dpbuf_t* self = dpbuf_new(rsz);
        if (self) {
            vsnprintf(self->data->data, rsz + 1, fmt, args);
            self->rend = self->wbeg = self->size;
            return self;
        }
    }
    return NULL;
}

dpbuf_t* dpbuf_new_f(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    dpbuf_t* self = dpbuf_new_v(fmt, args);
    va_end(args);
    return self;
}

dpbuf_t* dpbuf_new_r(const dpbuf_t* self)
{
    if (self == NULL || self->data == NULL) {
        return dpbuf_new(0);
    }

    dpbuf_t* view = (dpbuf_t*)malloc(sizeof(dpbuf_t));
    if (view == NULL) {
        return NULL;
    }
    memcpy(view, self, sizeof(dpbuf_t));
    _dpbuf_data_ref(self->data);
    view->wbeg = view->rend;
    view->size = view->rend;
    dpbuf_cpusr(self, view);
    return view;
}

dpbuf_t* dpbuf_dup_r(const dpbuf_t* self)
{
    if (self == NULL || dpbuf_crsize(self) == 0) {
        return dpbuf_new(0);
    } else {
        dpbuf_t* new_buf = dpbuf_new_d(dpbuf_crdata(self), dpbuf_crsize(self),
            DPBUF_DUP_DATA | DPBUF_INIT_R);
        dpbuf_cpusr(self, new_buf);
        return new_buf;
    }
}

dpbuf_t* dpbuf_dup_e(const dpbuf_t* self)
{
    if (self == NULL || dpbuf_cesize(self) == 0) {
        return dpbuf_new(0);
    } else {
        dpbuf_t* new_buf = dpbuf_new_d(dpbuf_cedata(self), dpbuf_cesize(self),
            DPBUF_DUP_DATA | DPBUF_INIT_R);
        dpbuf_cpusr(self, new_buf);
        return new_buf;
    }
}

void dpbuf_del(dpbuf_t* self)
{
    if (self) {
        _dpbuf_data_del(self->data);
        free(self);
    }
}

void dpbuf_fini(dpbuf_t* self)
{
    if (self && self->data) {
        _dpbuf_data_del(self->data);
    }
    memset(self, 0, sizeof(dpbuf_t));
}

void dpbuf_cpusr(const dpbuf_t* self, dpbuf_t* dst)
{
    if (self == NULL || dst == NULL) {
        return;
    }

    dst->uflag = self->uflag;
    dst->utype = self->utype;
}

size_t dpbuf_refc(dpbuf_t* self)
{
    return atomic_load_explicit(&self->data->refc, memory_order_acquire);
}

static bool _dpbuf_resize(dpbuf_t* self, int size)
{
    if (self == NULL || size <= 0) {
        return false;
    }

    bool r = _dpbuf_data_resize(self->data, size);
    if (r) {
        self->size = size;
    }

    return r;
}

static void _dpbuf_recycle_data(dpbuf_t* self)
{
    int rbeg = self->rbeg;
    if (rbeg == self->wbeg) {
        self->size = self->size - self->wbeg;
        self->rbeg = self->rend = self->wbeg = 0;
        self->data->data[self->size] = '\0';
    } else if (rbeg > DPBUF_S_SIZE && (self->data->maxs / rbeg) < 4) {
        memmove(self->data->data, self->data->data + rbeg, self->wbeg - rbeg);
        self->rbeg = 0;
        self->rend = self->rend - rbeg;
        self->wbeg = self->wbeg - rbeg;
        self->size = self->size - rbeg;
        self->data->data[self->size] = '\0';
    }
}

// 写入数据，并移动游标
static bool _dpbuf_wcheck(dpbuf_t* self, int len)
{
    int l = self->size;
    if (l - self->wbeg < len) {
        return dpbuf_resizew(self, len);
    }
    return true;
}

int dpbuf_size(const dpbuf_t* self)
{
    return self->size;
}

void* dpbuf_data(const dpbuf_t* self)
{
    return self->data->data;
}

int dpbuf_utype(const dpbuf_t* self)
{
    return self->utype;
}

void dpbuf_set_utype(dpbuf_t* self, int utype)
{
    self->utype = utype;
}

int dpbuf_uflag(const dpbuf_t* self, int uflag)
{
    return self->uflag & uflag;
}

void dpbuf_rmv_uflag(dpbuf_t* self, int uflag)
{
    self->uflag &= ~uflag;
}

void dpbuf_add_uflag(dpbuf_t* self, int uflag)
{
    self->uflag |= uflag;
}

void dpbuf_recycle(dpbuf_t* self, bool force)
{
    if (self && (force || !(self->flag & DPBUF_NORECYCLE))) {
        _dpbuf_recycle_data(self);
        _dpbuf_resize(self, 0);
    }
}

void dpbuf_set_recycle(dpbuf_t* self, bool b)
{
    int f = self->flag;
    self->flag = b ? f & (~DPBUF_NORECYCLE) : f | DPBUF_NORECYCLE;
}

bool dpbuf_resizew(dpbuf_t* self, int s)
{
    if (s >= 0) {
        if (!(self->flag & DPBUF_NORECYCLE)) {
            _dpbuf_recycle_data(self);
        }
        return _dpbuf_resize(self, self->wbeg + s);
    }
    return false;
}

void dpbuf_reset(dpbuf_t* self, int mode)
{
    if (mode & DPBUF_INIT_R) {
        self->rend = self->wbeg = self->size;
        self->rbeg = 0;
    } else if (mode & DPBUF_INIT_W) {
        self->rbeg = self->rend = self->wbeg = 0;
    } else {
        self->rbeg = self->rend = self->wbeg = self->size = 0;
    }
}

char* dpbuf_crdata(const dpbuf_t* self)
{
    return self->data->data + self->rbeg;
}

char* dpbuf_cwdata(const dpbuf_t* self)
{
    return self->data->data + self->wbeg;
}
char* dpbuf_cedata(const dpbuf_t* self)
{
    return self->data->data + self->rend;
}
// 读取数据，并移动游标
int dpbuf_crsize(const dpbuf_t* self)
{
    return self->rend - self->rbeg;
}
int dpbuf_cwsize(const dpbuf_t* self)
{
    return self->size - self->wbeg;
}
int dpbuf_cesize(const dpbuf_t* self)
{
    return self->wbeg - self->rend;
}

bool dpbuf_cempty(const dpbuf_t* self)
{
    return dpbuf_crsize(self) == 0;
}

bool dpbuf_cequalc(const dpbuf_t* self, const char* sub, int len)
{
    if (len < 0) {
        len = strlen(sub);
    }
    int s = dpbuf_crsize(self);
    if (len == 0 || s < len) {
        return false;
    }

    bool r = (memcmp(dpbuf_crdata(self), sub, len) == 0);
    return r;
}

int dpbuf_ccmp(const dpbuf_t* self, const dpbuf_t* other)
{
    if (self == other) {
        return 0;
    }
    if (self == NULL || other == NULL) {
        return self == NULL ? -1 : 1;
    }

    int l1 = dpbuf_crsize(self);
    int l2 = dpbuf_crsize(other);

    // 先按长度比较
    if (l1 != l2) {
        return l1 - l2;
    }

    // 长度相等则比较内容
    if (l1 > 0) {
        return memcmp(dpbuf_crdata(self), dpbuf_crdata(other), l1);
    }

    return 0;
}

int dpbuf_cfind(const dpbuf_t* self, const char* match, int len, int left)
{
    if (match == NULL || self == NULL) {
        return -EINVAL;
    }

    len = len < 0 ? strlen(match) : len;
    int rsz = dpbuf_crsize(self);
    if (len == 0 || rsz < len) {
        return -ENODATA;
    }

    char* d = dpbuf_crdata(self);
    char* p = (char*)memmem(d + left, rsz - left, match, len);
    if (p == NULL) {
        return -EPERM;
    }
    return p - d;
}

int dpbuf_cstrlen(const dpbuf_t* self)
{
    return strnlen(dpbuf_crdata(self), dpbuf_crsize(self));
}

bool dpbuf_cbegwith(const dpbuf_t* self, const char* sub, int len, bool skip_begws)
{
    int s = dpbuf_crsize(self);
    if (s == 0 || sub == NULL) {
        return false;
    }
    if (len < 0) {
        len = strlen(sub);
    }
    if (s < len) {
        return false;
    }
    char* d = dpbuf_crdata(self);

    if (skip_begws) {
        int n = 0;
        char c = 0;
        for (n = 0; n < s; n++) {
            c = d[n];
            if (c != ' ' && c != '\r' && c != '\n') {
                break;
            }
        }
        d += n;
        s -= n;
        if (s < len) {
            return false;
        }
    }

    return (memcmp(sub, d, len) == 0);
}

int dpbuf_rseek(dpbuf_t* self, int offset, int seek)
{
    int i = self->rbeg;
    switch (seek) {
    case SEEK_SET: {
        self->rbeg = _iclamp(0 + offset, 0, self->rend);
        break;
    }
    case SEEK_CUR: {
        self->rbeg = _iclamp(self->rbeg + offset, 0, self->rend);
        break;
    }
    case SEEK_END: {
        self->rbeg = _iclamp(self->rend + offset, 0, self->rend);
        break;
    }
    }
    return self->rbeg - i;
}

int dpbuf_wseek(dpbuf_t* self, int offset, int seek)
{
    int i = self->wbeg;
    switch (seek) {
    case SEEK_SET: {
        self->wbeg = _iclamp(self->rend + offset, self->rend, self->size);
        break;
    }
    case SEEK_CUR: {
        self->wbeg = _iclamp(self->wbeg + offset, self->rend, self->size);
        break;
    }
    case SEEK_END: {
        self->wbeg = _iclamp(self->size + offset, self->rend, self->size);
        break;
    }
    }
    return self->wbeg - i;
}

int dpbuf_wbuf(dpbuf_t* self, const dpbuf_t* buf, int len)
{
    int alen = dpbuf_crsize(buf);
    len = len <= 0 ? alen : _imin(alen, len);

    if (!_dpbuf_wcheck(self, len)) {
        return -ENOMEM;
    }
    memcpy(dpbuf_cwdata(self), dpbuf_crdata(buf), len);
    self->wbeg += len;
    return len;
}

int dpbuf_wbuf_r(dpbuf_t* self, dpbuf_t* buf, int len)
{
    int alen = dpbuf_crsize(buf);
    len = len <= 0 ? alen : _imin(alen, len);

    if (!_dpbuf_wcheck(self, len)) {
        return -ENOMEM;
    }
    memcpy(dpbuf_cwdata(self), dpbuf_crdata(buf), len);
    self->wbeg += len;
    buf->rbeg += len;
    return len;
}

int dpbuf_wfill(dpbuf_t* self, int len, int8_t v)
{
    if (!_dpbuf_wcheck(self, len)) {
        return -ENOMEM;
    }
    memset(dpbuf_cwdata(self), v, len);
    self->wbeg += len;
    return len;
}

int dpbuf_wdata(dpbuf_t* self, const void* data, int len)
{
    if (len == 0 || data == NULL) {
        return -EINVAL;
    }
    if (!_dpbuf_wcheck(self, len)) {
        return -ENOMEM;
    }
    memcpy(dpbuf_cwdata(self), data, len);
    self->wbeg += len;
    return len;
}

int dpbuf_wstrf(dpbuf_t* self, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int sz = dpbuf_wstrv(self, fmt, args);
    va_end(args);
    return sz;
}

int dpbuf_wstrv(dpbuf_t* self, const char* fmt, va_list args)
{
    if (self == NULL || fmt == NULL) {
        return -EINVAL;
    }

    va_list args2;
    va_copy(args2, args);
    int rsz = vsnprintf(NULL, 0, fmt, args2);
    va_end(args2);

    if (rsz <= 0) {
        return 0;
    }

    if (!_dpbuf_wcheck(self, rsz + 1)) {
        return -ENOMEM;
    }

    vsnprintf(dpbuf_cwdata(self), rsz + 1, fmt, args);

    self->wbeg += rsz;
    return rsz;
}

int dpbuf_eseek(dpbuf_t* self, int offset, int seek)
{
    int i = self->rend;
    switch (seek) {
    case SEEK_SET: {
        self->rend = _iclamp(self->rbeg + offset, self->rbeg, self->wbeg);
        break;
    }
    case SEEK_CUR: {
        self->rend = _iclamp(self->rend + offset, self->rbeg, self->wbeg);
        break;
    }
    case SEEK_END: {
        self->rend = _iclamp(self->wbeg + offset, self->rbeg, self->wbeg);
        break;
    }
    }
    return self->rend - i;
}

int dpbuf_rws(dpbuf_t* self)
{
    self->rbeg = self->rend;
    char* d = dpbuf_cedata(self);
    char c = 0;
    int n = 0;
    for (n = 0; n < dpbuf_cesize(self); n++) {
        c = d[n];
        if (c != ' ' && c != '\r' && c != '\n') {
            break;
        }
    }
    self->rend += n;
    return n;
}

int dpbuf_rdata(dpbuf_t* self, int len)
{
    self->rbeg = self->rend;
    int slen = dpbuf_cesize(self);
    if (len < 0) {
        len = slen;
    }
    len = _imin(len, slen);
    self->rend += len;
    return len;
}

int dpbuf_rmust(dpbuf_t* self, int len)
{
    self->rbeg = self->rend;
    int slen = dpbuf_cesize(self);
    if (len < 0 || len > slen) {
        self->rend = self->rbeg;
        return -ENODATA;
    }
    self->rend += len;
    return len;
}

int dpbuf_runtil(dpbuf_t* self, const char* until, int until_sz)
{
    self->rbeg = self->rend;
    self->rend = self->wbeg;
    int idx = dpbuf_cfind(self, until, until_sz, 0);
    if (idx < 0) {
        self->rend = self->rbeg;
        return -ENODATA;
    }

    int len = idx + until_sz;
    self->rend = self->rbeg + len;
    return len;
}

int dpbuf_rcstr(dpbuf_t* self)
{
    self->rbeg = self->rend;
    int el = self->wbeg - self->rend;
    char* ed = dpbuf_cedata(self);
    int len = strnlen(ed, el);
    if (len > 0 && len < el) {
        len++;
    }
    self->rend += len;
    return len;
}

int dpbuf_rall(dpbuf_t* self)
{
    self->rbeg = self->rend;
    self->rend = self->wbeg;
    return self->rend - self->rbeg;
}

int dpbuf_readto(dpbuf_t* self, dpbuf_t* det, int size)
{
    self->rbeg = self->rend;
    int sz = dpbuf_cesize(self);
    if (sz == 0) {
        return -ENODATA;
    }
    self->rend = self->wbeg;

    int wsz = dpbuf_wbuf(det, self, size);
    det->rend = det->wbeg;

    self->rend = self->wbeg - (sz - wsz);
    self->rbeg = self->rend;

    return wsz;
}
