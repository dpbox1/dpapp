#include "dpapp/dpasc.h"
#include "dpapp/dpdef.h"
#include "dpapp/dpevp.h"
#include "dpapp/dplog.h"
#include "dpapp/dprbt.h"
#include "dpapp/dpret.h"
#include "dpapp/os/dpevp_pri.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>

#define DPELE_TYPE_WEAK INT8_MAX

typedef struct dpele_tmrspec
{
    RB_ENTRY(dpele);
    int64_t expire;
    double after;
} dpele_tmrspec_t;

struct dpele
{
    const dpele_type_t* type; // User type
    uint16_t able_events : 5;
    uint16_t want_events : 5;
    uint16_t poll_events : 5;

    // All ele flags
    uint8_t is_timing : 1; // Is timing
    uint8_t is_close  : 1; // Close fd when dpele_del, default 1
    uint8_t is_detach : 1; // Is detach, default 0.
    uint8_t _p1       : 4; // Padding

    uint8_t is_reto : 1; // For ctc.
    uint8_t _p2     : 7;

    dpret_t ret; // Error code

    union
    {
        RB_ENTRY(dpele) * _tmr_rbt_spec;
        dpele_tmrspec_t* tmr_spec;
    };

    atomic_int refc; // Control the life cycle

    union
    {
        int fd;
        struct
        {
            uint8_t toid;      // Assign to worker id
            uint8_t fromid;    // Which worker add ctc
            atomic_uchar stat; // CTC status
        };
    };

    struct dpele* prev; // Unified deque prev
    struct dpele* next; // Unified deque next

    dpv64_t cop; // Coroutine pointer

    const dpasc_t* asc_type;
    void* asc_data; // alloc arg_size + sizeof(uint32_t) as size;

    char aux_data[]; // type-bound auxiliary data
};

void* _dpele_asc_data(dpele_t* ele)
{
    if (ele->asc_type == NULL || ele->asc_type->datasz == 0) {
        return NULL;
    }

    uint32_t data_size = ele->asc_type->datasz;
    uint32_t* data = (uint32_t*)ele->asc_data;

    if (data && (data[0] >= data_size)) {
        memset(data + 1, 0, data_size);
        return data + 1; // offset 1 uint32_t
    } else {
        data = realloc(data, data_size + sizeof(uint32_t));
        if (data == NULL) {
            errno = DPE_NOMEM;
            return NULL;
        } else {
            data[0] = data_size;
            ele->asc_data = data;
            memset(data + 1, 0, data_size);
            return data + 1; // offset 1 uint32_t
        }
    }
}

void* dpele_asc_data(dpele_t* ele)
{
    uint32_t* data = (uint32_t*)ele->asc_data;
    return data ? (void*)(data + 1) : NULL;
}

const dpasc_t* _dpele_asc_type(dpele_t* ele)
{
    return ele->asc_type;
}

static dpret_t _dpevp_unwch_efd(dpefd_t* efd);
static dpret_t _dpevp_watch_efd(dpefd_t* efd);

dpele_t* dpele_get_by_uptr(void* uptr)
{
    return (dpele_t*)((char*)uptr - sizeof(struct dpele));
}

static dpele_type_t _dpele_wake_only_type = {
    .type = (dpele_type_e)DPELE_TYPE_WEAK,
    .events = DPEVT_ALL,
};

struct dpele_wake
{
    const dpele_type_t* type; // User type
    int fd;
    int id;
};

static inline dpret_t _dpele_wake_init(struct dpele_wake* self, int id)
{
    self->type = &_dpele_wake_only_type;
    self->id = id;
    self->fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    return self->fd;
}

static inline void _dpele_wake_fini(struct dpele_wake* self)
{
    close(self->fd);
}

/// timerset based on rbtree
typedef struct dptmrset
{
    RB_HEAD(, dpele);
} dptmrset_t;

static inline int64_t _dpele_tmrspec_compare(dpele_t* o1, dpele_t* o2)
{
    int64_t c = o1->tmr_spec->expire - o2->tmr_spec->expire;
    if (c == 0) {
        c = (int64_t)o1 - (int64_t)o2;
    }
    return c;
}

RB_GENERATE(dptmrset, dpele, _tmr_rbt_spec, _dpele_tmrspec_compare, ->);

void dptmrset_init(dptmrset_t* self)
{
    RB_INIT(self);
}

void dptmrset_free(dptmrset_t* self)
{
    dpele_t *item = NULL, *temp = NULL;
    RB_FOREACH_SAFE(item, dptmrset, self, temp)
    {
        RB_REMOVE(dptmrset, self, item);
    }
    RB_INIT(self);
}

static dpret_t _dptmrset_upsert(dptmrset_t* self, dpele_t* ele)
{
    dpele_tmrspec_t* tmr_spec = ele->tmr_spec;
    if (tmr_spec == NULL || tmr_spec->after < 0) {
        return DPE_INVAL;
    }

    tmr_spec->expire = dplog_millis() + (double)tmr_spec->after * 1000.0;

    if (ele->is_timing) {
        RB_REINSERT(dptmrset, self, ele);
    } else {
        if (RB_INSERT(dptmrset, self, ele) != NULL) {
            return DPE_INTERNAL_ERROR;
        }
        ele->is_timing = 1;
    }

    return DPE_OK;
}

static inline void _dptmrset_remove(dptmrset_t* self, dpele_t* ele)
{
    if (ele && ele->is_timing) {
        RB_REMOVE(dptmrset, self, ele);
        ele->is_timing = 0;
    }
}

dpele_t* dptmrset_expire(dptmrset_t* self, int* waitms)
{
    (*waitms) = -1;

    dpele_t* mintmr = RB_MIN(dptmrset, self);
    if (mintmr == NULL) {
        return NULL;
    }

    int64_t currts = dplog_millis();
    if (mintmr->tmr_spec->expire > currts) {
        int64_t nextms = mintmr->tmr_spec->expire - currts - 10;
        (*waitms) = 1 < nextms ? nextms : 1;
        return NULL;
    }

    RB_REMOVE(dptmrset, self, mintmr);
    (*waitms) = 0;

    mintmr->is_timing = 0;
    return mintmr;
}

static void _dpctc_init(dpele_t* self);
static void _dpefd_init(dpele_t* self);

dpele_t* _dpele_new(const dpele_type_t* type)
{
    dpele_t* ele = (dpele_t*)calloc(1, sizeof(dpele_t) + (type ? type->size : 0));
    if (ele == NULL) {
        errno = DPE_NOMEM;
        return NULL;
    }

    atomic_init(&ele->refc, 1);
    ele->type = type;
    dpret_t ret = DPE_OK;
    switch (type->type) {
    case DPELE_TYPE_EFD: {
        _dpefd_init(ele);
        break;
    }
    case DPELE_TYPE_USD: {
        break;
    }
    case DPELE_TYPE_TMR: {
        break;
    }
    case DPELE_TYPE_CTC: {
        _dpctc_init(ele);
        break;
    }
    default: {
        ret = DPE_INVAL;
        break;
    }
    }

    if (dpret_iserr(ret)) {
        free(ele);
        ele = NULL;
    }

    return ele;
}

void _dpele_del(dpele_t* self)
{
    if (self->type->type == DPELE_TYPE_EFD && self->fd >= 0) {
        dpret_t ret = _dpevp_unwch_efd(self);
        if (dpret_iserr(ret)) {
            errno = ret;
        }

        if (self->is_close) {
            close(self->fd);
            self->fd = -1;
        }
    }

    if (self->type && self->type->fini) {
        self->type->fini(self->aux_data);
    }

    DP_FREE(self->tmr_spec);
    DP_FREE(self->asc_data);
    free(self);
}

dpele_t* dpele_new(const dpele_type_t* type, ...)
{
    va_list args;
    va_start(args, type);
    dpele_t* ele = dpele_newv(type, args);
    va_end(args);
    return ele;
}

dpele_t* dpele_newv(const dpele_type_t* type, va_list args)
{
    if (type == NULL) {
        errno = DPE_INVAL;
        return NULL;
    }
    dpele_t* ele = _dpele_new(type);
    if (ele == NULL) {
        return NULL;
    }

    if (type->init) {
        dpret_t r = type->init(ele->aux_data, args);
        if (dpret_isok(r)) {
            if (type->type == DPELE_TYPE_EFD && type->events > 0) {
                ele->fd = r;
                r = _dpevp_watch_efd(ele);
            }
        }
        if (dpret_iserr(r)) {
            _dpele_del(ele);
            ele = NULL;
        }
    }

    return ele;
}

dpele_t* dpele_ref(dpele_t* self)
{
    atomic_fetch_add_explicit(&self->refc, 1, memory_order_acquire);
    return self;
}

#define _dpele_unref(self_)                                                         \
    atomic_fetch_sub_explicit(&self_->refc, 1, memory_order_acq_rel)

#define _dpele_refc(self_) atomic_load_explicit(&self_->refc, memory_order_acquire)

uint32_t dpele_refc(dpele_t* self)
{
    return _dpele_refc(self);
}

void dpele_del(dpele_t* self)
{
    if (self && _dpele_unref(self) == 1) {
        if (self->asc_type) {
            return; // will del by loop
        }
        _dpele_del(self);
    }
}

dpele_t* dpele_dup(dpele_t* self, bool unuse_)
{
    if (self->type->type == DPELE_TYPE_EFD && self->fd < 0) {
        errno = DPE_PERM;
        return NULL;
    }

    if (dpele_is_doing(self)) {
        errno = DPE_PERM;
        return NULL;
    }

    dpele_t* ele = _dpele_new(self->type);
    if (ele == NULL) {
        return NULL;
    }

    dpret_t r = DPE_OK;
    if (self->type && self->type->size) {
        if (self->type->copy) {
            r = self->type->copy(ele->aux_data, self->aux_data);
            if (dpret_iserr(r)) {
                goto ERR;
            }
        } else
            memcpy(ele->aux_data, self->aux_data, self->type->size);
    }

    if (ele->fd >= 0) {
        // Dup fd and split events
        ele->fd = dup(self->fd);
        if (ele->fd < 0) {
            r = -errno;
            goto ERR;
        }
        ele->poll_events = (self->poll_events & (~DPEVT_AIN));
        r = _dpevp_watch_efd(ele);
        if (dpret_iserr(r)) {
            goto ERR;
        }

        self->poll_events &= (~DPEVT_OUT);
        _dpevp_watch_efd(self);

        ele->want_events = 0;
        ele->able_events = (self->able_events & (~DPEVT_AIN));
    }

    return ele;

ERR:
    errno = r;
    free(ele);
    return NULL;
}

dpret_t dpele_ret(dpele_t* self)
{
    return self->ret;
}

void dpele_set_ret(dpele_t* self, dpret_t ret)
{
    self->ret = ret;
}

dpret_t dpele_set_detach(dpele_t* self, bool detach)
{
    if (dpele_is_doing(self)) {
        return DPE_BUSY;
    }

    self->is_detach = detach ? 1 : 0;
    return DPE_OK;
}

bool dpele_is_detach(dpele_t* self)
{
    return self->is_detach;
}

dpret_t dpele_set_timeout(dpele_t* self, double sec)
{
    if (self->type->type == DPELE_TYPE_CTC) {
        return DPE_INVAL;
    }

    dpret_t ret = (sec < 0.0f) ? DPE_BEINITED : DPE_OK;
    if (self->tmr_spec == NULL) {
        if (ret != DPE_OK) {
            return ret;
        }
        self->tmr_spec = calloc(1, sizeof(dpele_tmrspec_t));
        if (self->tmr_spec == NULL) {
            return DPE_NOMEM;
        }
    }
    self->tmr_spec->after = sec;
    return ret;
}

double dpele_timeout(dpele_t* self)
{
    return self->tmr_spec ? self->tmr_spec->after : -1;
}

dpv64_t dpele_cop(dpele_t* self)
{
    return self->cop;
}

bool dpele_wait(dpele_t* self, dpv64_t cop)
{
    if (dpele_is_doing(self) && self->is_detach == 0) {
        self->cop = cop;
        return true;
    } else {
        return false;
    }
}

void _dpele_set_cop(dpele_t* self, dpv64_t cop)
{
    self->cop = cop;
}

const dpele_type_t* dpele_type(dpele_t* self)
{
    return self ? self->type : NULL;
}

void* dpele_aux_data(dpele_t* self)
{
    return self ? self->aux_data : NULL;
}

bool dpele_is_doing(dpele_t* self)
{
    return (self->asc_type != NULL);
}

static dpret_t _dpefd_init_uinit(void* udata, va_list vlist)
{
    int fd = va_arg(vlist, int);
    if (fd < 0) {
        return DPE_INVAL;
    }
    return fd;
}

const dpele_type_t* dpefd_init_type()
{
    static dpele_type_t _type = {
        .name = "efd",
        .type = DPELE_TYPE_EFD,
        .iotype = DPAIO_TYPE_GFD,
        .events = DPEVT_AIO,
        .init = _dpefd_init_uinit,
    };
    return &_type;
}

void _dpefd_init(dpele_t* self)
{
    self->is_close = 1;
    self->fd = -1;
    self->poll_events = self->type->events;
}

int dpefd_fd(dpele_t* self)
{
    return self->fd;
}

void dpefd_set_close(dpele_t* self, bool cl)
{
    self->is_close = cl;
}

dpret_t _dpefd_wake(dpele_t* self, dpasc_out_t* out)
{
    dpret_t ret = DPEVT_GET_ERR(self->able_events);
    if (dpret_iserr(ret)) {
        self->want_events = 0;
        return ret;
    }

    if (out->inva_events) {
        self->able_events &= (~out->inva_events);
        out->inva_events = 0;
    }

    self->want_events = out->want_events;
    ret = DPE_WAIT;
    while (self->able_events & self->want_events) {
        out->able_events = self->able_events;
        ret = self->asc_type->post(self, out);
        if (out->inva_events) {
            self->able_events &= (~out->inva_events);
            out->inva_events = 0;
        }
        if (ret == DPE_WAIT) {
            if (out->want_events > 0) {
                self->want_events = out->want_events;
            }
        } else {
            self->want_events = 0;
            break;
        }
    }
    return ret;
}

bool _dpefd_feed(dpele_t* self, uint32_t evs)
{
    self->able_events |= evs;
    if (self->want_events == 0 || !dpele_is_doing(self)) {
        return false;
    }

    dpasc_out_t out = {
        .want_events = self->want_events,
        .inva_events = 0,
        .able_events = self->able_events,
        .data = dpele_asc_data(self),
    };
    dpret_t ret = _dpefd_wake(self, &out);
    if (ret == DPE_WAIT) {
        self->ret = DPE_INPROGRESS;
        return false;
    } else {
        self->ret = ret;
        return true;
    }
}

#define DPCTC_OVER (1U << 21)
#define DPCTC_RETO (1U << 22)

typedef enum
{
    DPCTC_INITIAL,
    DPCTC_COMMIT,
    DPCTC_CANCEL,
    DPCTC_RUNNING,
    DPCTC_FINISH,
} dpctc_stat_e;

void _dpctc_init(dpctc_t* self)
{
    self->toid = -1;
    self->fromid = -1;
    atomic_init(&self->stat, DPCTC_INITIAL);
    self->ret = 0;
    self->is_reto = 0;
    self->is_detach = 0;
}

dpret_t dpctc_set_toid(dpctc_t* self, int toid)
{
    if (self == NULL || self->type->type != DPELE_TYPE_CTC) {
        return DPE_INVAL;
    }

    toid = _dpctc_resolve_toid(self, toid);
    if (dpret_iserr(toid)) {
        return toid;
    }

    self->toid = (uint8_t)toid;
    return DPE_OK;
}

static inline bool _dpctc_del(dpctc_t* self)
{
    if (dpele_refc(self) <= 0) {
        _dpele_del(self);
        return true;
    }
    return self->is_detach != 0;
}

int dpctc_fromid(dpctc_t* self)
{
    return self->fromid;
}

int dpctc_toid(dpctc_t* self)
{
    return self->toid;
}

static inline dpctc_stat_e _dpctc_stat(dpctc_t* self)
{
    return (dpctc_stat_e)atomic_load_explicit(&self->stat, memory_order_acquire);
}

static inline void _dpctc_set_stat(dpctc_t* self, uint8_t stat)
{
    atomic_store_explicit(&self->stat, (int8_t)stat, memory_order_release);
}

static inline bool _dpctc_ces_stat(dpctc_t* self, uint8_t new_stat, uint8_t old_stat)
{
    return atomic_compare_exchange_strong_explicit(&self->stat, &old_stat, new_stat,
        memory_order_release, memory_order_acquire);
}

typedef struct dpele_deque
{
    dpele_t* head;
    dpele_t* tail;
} dpele_deque_t;

static inline void _dpele_deque_init(dpele_deque_t* dq)
{
    dq->head = NULL;
    dq->tail = NULL;
}

static void _dpele_deque_clear(dpele_deque_t* dq)
{
    dpele_t* ele = dq->head;
    dpele_t* temp;
    while (ele) {
        temp = ele;
        ele = ele->next;
        dpele_del(temp);
    }
    dq->head = NULL;
    dq->tail = NULL;
}

static inline void _dpele_deque_push_back(dpele_deque_t* dq, dpele_t* ele)
{
    if (ele->prev != NULL || ele->next != NULL || dq->head == ele) {
        // 已经在队列中，不能重复插入
        return;
    }
    ele->next = NULL;
    ele->prev = dq->tail;
    if (dq->tail) {
        dq->tail->next = ele;
    } else {
        dq->head = ele;
    }
    dq->tail = ele;
}

static inline dpele_t* _dpele_deque_pop_front(dpele_deque_t* dq)
{
    dpele_t* ele = dq->head;
    if (ele) {
        dq->head = ele->next;
        if (dq->head) {
            dq->head->prev = NULL;
        } else {
            dq->tail = NULL;
        }
        ele->prev = NULL;
        ele->next = NULL;
    }
    return ele;
}

static inline dpele_t* _dpele_deque_remove(dpele_deque_t* dq, dpele_t* ele)
{
    if (ele->prev) {
        ele->prev->next = ele->next;
    } else {
        dq->head = ele->next;
    }
    if (ele->next) {
        ele->next->prev = ele->prev;
    } else {
        dq->tail = ele->prev;
    }
    ele->prev = NULL;
    ele->next = NULL;
    return ele;
}

static void _dpele_deque_pushq(dpele_deque_t* dst, dpele_deque_t* src)
{
    if (src->head == NULL) {
        return;
    }
    src->head->prev = dst->tail;
    if (dst->tail) {
        dst->tail->next = src->head;
        dst->tail = src->tail;
    } else {
        dst->head = src->head;
        dst->tail = src->tail;
    }
    src->head = NULL;
    src->tail = NULL;
}

static inline void _dpele_deque_move(dpele_deque_t* dst, dpele_deque_t* src)
{
    *dst = *src;
    src->head = NULL;
    src->tail = NULL;
}

typedef struct _dpevp
{
    int id;   // worker id
    int type; // worker type

    int epoll_fd;
    int ev_capacity;
    struct epoll_event* evs;
    dpele_deque_t enter_ctcs;

    void (*commit_ctc_f)(struct _dpevp*, dpctc_t*);
    void (*finish_ctc_f)(struct _dpevp*, dpctc_t*);

    dpele_t* (*reap_funcs[4])(struct _dpevp*);

    dptmrset_t timer; // manage timers
    int timer_waitms;

    int ev_index;
    int ev_count;

    int pop_limit;
    dpele_deque_t ready;
} dpevp_t;

static __thread dpevp_t* _gevp = NULL; // peer thread has a node instance

dpevp_t* worker_new(int type, int id);
dpevp_t* master_new(int type, int id);
void worker_del(dpevp_t*);
void master_del(dpevp_t*);
bool master_start(dpevp_t* self);
bool worker_start(dpevp_t* self);

dpevp_t* dpevp_new(int type, int id)
{
    if (id == 0) {
        return master_new(type, id);
    } else {
        return worker_new(type, id);
    }
}

void dpevp_del(dpevp_t* self)
{
    if (self->id == 0) {
        master_del(self);
    } else {
        worker_del(self);
    }
}

void dpevp_start(dpevp_t* self, void (*start_fun)(void*), void* start_arg)
{
    _gevp = self;

    if (self->id == 0) {
        master_start(self);
    } else {
        worker_start(self);
    }

    start_fun(start_arg);
}

int dpevp_id()
{
    dpevp_t* self = _gevp;
    return self ? self->id : -1;
}

int dpevp_type()
{
    dpevp_t* self = _gevp;
    return self ? self->type : -1;
}

static inline dpele_t* _dpevp_reap_usd(dpevp_t* self)
{
    return _dpele_deque_pop_front(&self->ready);
}

static inline dpele_t* _dpevp_reap_tmr(dpevp_t* self)
{
    dpele_t* ele = NULL;
    while ((ele = dptmrset_expire(&self->timer, &self->timer_waitms)) != NULL) {
        switch (ele->type->type) {
        case DPELE_TYPE_EFD: {
            ele->ret = DPE_TIME;
            return ele;
        }
        case DPELE_TYPE_USD: {
            ele->ret = DPE_TIME;
            return ele;
        }
        case DPELE_TYPE_CTC: {
            if (_dpctc_ces_stat(ele, DPCTC_CANCEL, DPCTC_COMMIT)) {
                ele->ret = DPE_TIME;
            }
            break;
        }
        case DPELE_TYPE_TMR: {
            ele->ret = DPE_TIME;
            return ele;
        }
        }
    }
    return NULL;
}

static void _dpevp_free(dpevp_t* self)
{
    if (self->epoll_fd > 0)
        close(self->epoll_fd);
    free(self->evs);
    _dpele_deque_clear(&self->enter_ctcs);
    dptmrset_free(&self->timer);
    memset(self, 0, sizeof(dpevp_t));
}

static bool _dpevp_init(dpevp_t* self, int type, int id)
{
    memset(self, 0, sizeof(dpevp_t));
    self->id = id;
    self->type = type;

    self->epoll_fd = epoll_create(1);
    if (self->epoll_fd < 0) {
        dplog_error("dpevp", "epoll_create failed for node %d: %s", id,
            strerror(errno));
        _dpevp_free(self);
        return false;
    }
    self->evs = malloc(sizeof(struct epoll_event) * 256);
    if (self->evs == NULL) {
        dplog_error("dpevp", "Failed to allocate epoll events for node %d", id);
        _dpevp_free(self);
        return false;
    }
    self->ev_capacity = 256;

    _dpele_deque_init(&self->enter_ctcs);

    dptmrset_init(&self->timer);

    self->reap_funcs[DPELE_TYPE_TMR] = _dpevp_reap_tmr;
    self->reap_funcs[DPELE_TYPE_USD] = _dpevp_reap_usd;
    return true;
}

dpret_t _dpevp_unwch_efd(dpefd_t* efd)
{
    if (efd->fd >= 0) {
        if (epoll_ctl(_gevp->epoll_fd, EPOLL_CTL_DEL, efd->fd, NULL) == 0) {
            return DPE_OK;
        } else {
            return (errno == ENOENT) ? DPE_OK : -errno;
        }
    } else {
        return DPE_INVAL;
    }
}

dpret_t _dpevp_watch_efd(dpefd_t* efd)
{
    uint32_t evs = efd->poll_events;
    dpret_t r = DPE_OK;
    struct epoll_event evt;
    evt.data.ptr = efd;
    evt.events = EPOLLRDHUP | EPOLLET;
    evt.events |= (evs & DPEVT_AIO);

    int nbf = fcntl(efd->fd, F_GETFL, 0);
    if (nbf != -1 && !(nbf & O_NONBLOCK)) {
        fcntl(efd->fd, F_SETFL, nbf | O_NONBLOCK);
    }

    r = epoll_ctl(_gevp->epoll_fd, EPOLL_CTL_ADD, efd->fd, &evt);
    if (r != 0) {
        if (errno == EEXIST) {
            r = epoll_ctl(_gevp->epoll_fd, EPOLL_CTL_MOD, efd->fd, &evt);
            if (r != 0) {
                r = -errno;
            }
        } else {
            r = -errno;
        }
    }

    return r;
}

typedef struct
{
    dpevp_t base;

    /// For ctc
    struct dpele_wake commit_wake;
    struct dpele_wake accept_wake;

    dpele_deque_t accepting_ctcs;
    dpele_deque_t commiting_ctcs;
    dpele_deque_t accepted_ctcs;
    dpele_deque_t commited_ctcs;

    bool is_request_ctc;
    bool is_respond_ctc;
} worker_t;

static const uint64_t _CTC_INCR_FLAG = 0x0000000000000001;
static const uint64_t _CTC_TAKE_FLAG = 0x0000000100000000;
static const uint64_t _CTC_TAKE_MASK = 0xffffffff00000000;
static const uint64_t _CTC_PUSH_MASK = 0x00000000ffffffff;

static inline void _worker_move_accept_ctc(worker_t* self)
{
    _dpele_deque_move(&self->accepted_ctcs, &self->accepting_ctcs);
    if (self->accepted_ctcs.head) {
        (void)!write(self->accept_wake.fd, &_CTC_INCR_FLAG, 8);
    } else {
        self->is_respond_ctc = true;
    }
}

static inline void _worker_move_commit_ctc(worker_t* self)
{
    _dpele_deque_move(&self->commited_ctcs, &self->commiting_ctcs);
    if (self->commited_ctcs.head) {
        (void)!write(self->commit_wake.fd, &_CTC_INCR_FLAG, 8);
    } else {
        self->is_request_ctc = true;
    }
}

static inline void _worker_push_commit_ctc(worker_t* self, dpctc_t* ctc)
{
    if (self->is_request_ctc) {
        self->is_request_ctc = false;
        _dpele_deque_push_back(&self->commited_ctcs, ctc);
        (void)!write(self->commit_wake.fd, &_CTC_INCR_FLAG, 8);
    } else {
        _dpele_deque_push_back(&self->commiting_ctcs, ctc);
    }
}

static inline void _worker_push_accept_ctc(worker_t* self, dpctc_t* ctc)
{
    if (self->is_respond_ctc) {
        self->is_respond_ctc = false;
        _dpele_deque_push_back(&self->accepted_ctcs, ctc);
        (void)!write(self->accept_wake.fd, &_CTC_INCR_FLAG, 8);
    } else {
        _dpele_deque_push_back(&self->accepting_ctcs, ctc);
    }
}

static inline void _worker_take_commit_ctc(worker_t* self, dpele_deque_t* ctcs)
{
    _dpele_deque_pushq(ctcs, &self->commited_ctcs);
    (void)!write(self->accept_wake.fd, &_CTC_TAKE_FLAG, 8);
}

static inline void _worker_take_accept_ctc(worker_t* self, dpele_deque_t* ctcs)
{
    _dpele_deque_pushq(ctcs, &self->accepted_ctcs);
    (void)!write(self->commit_wake.fd, &_CTC_TAKE_FLAG, 8);
}

static void _worker_finish_ctc(dpevp_t* self, dpctc_t* ctc)
{
    if (ctc->fromid == self->id) {
        _dpele_deque_push_back(&self->enter_ctcs, ctc);
    } else {
        _worker_push_commit_ctc((worker_t*)self, ctc);
    }
}

static void _worker_commit_ctc(dpevp_t* self, dpctc_t* ctc)
{
    if (ctc->toid == self->id) {
        _dpele_deque_push_back(&self->enter_ctcs, ctc);
    } else {
        _worker_push_commit_ctc((worker_t*)self, ctc);
    }
}

static dpctc_t* _worker_reap_ctc(dpevp_t* node)
{
    worker_t* self = (worker_t*)node;
    dpctc_t* ctc = NULL;
    while ((ctc = _dpele_deque_pop_front(&self->base.enter_ctcs)) != NULL) {
        switch (_dpctc_stat(ctc)) {
        case DPCTC_COMMIT: {
            if (_dpctc_ces_stat(ctc, DPCTC_RUNNING, DPCTC_COMMIT)) {
                return ctc;
            } // Otherwise jump to DPTASK_S_CANCEL
        }
        case DPCTC_CANCEL: {
            if (_dpctc_ces_stat(ctc, DPCTC_FINISH, DPCTC_CANCEL)) {
                if (!_dpctc_del(ctc)) {
                    _worker_finish_ctc(&self->base, ctc);
                }
                break;
            } // Otherwise jump to default
        }
        case DPCTC_RUNNING: {
            if (ctc->asc_type && ctc->asc_type->post) {
                dpasc_out_t out = {.data = dpele_asc_data(ctc)};
                ctc->ret = ctc->asc_type->post(ctc, &out);
                ctc->asc_type = NULL;
                _dpctc_set_stat(ctc, DPCTC_FINISH);
                _worker_finish_ctc(&self->base, ctc);
                break;
            }
            return ctc;
        }
        default: {
            _dptmrset_remove(&node->timer, ctc);
            if (!_dpctc_del(ctc)) {
                return ctc;
            }
            break;
        }
        }
    }
    return NULL;
}

static dpefd_t* _worker_reap_efd(dpevp_t* node)
{
    if (node->ev_index >= node->ev_count) {
        return NULL;
    }

    worker_t* self = (worker_t*)node;
    struct epoll_event* evd = NULL;
    dpefd_t* efd = NULL;
    uint64_t ctc_ev = 0;

    while (node->ev_index < node->ev_count) {
        evd = &node->evs[node->ev_index];
        // EPOLL基本事件和DPEVT基本一致，不需要额外转换，EPOLLRDHUP为特有的事件
        if (evd->events & EPOLLRDHUP) {
            evd->events |= DPEVT_HUP;
        }

        node->ev_index++;
        efd = (dpefd_t*)evd->data.ptr;
        if (efd->type->type == DPELE_TYPE_WEAK) {
            (void)!read(((struct dpele_wake*)evd->data.ptr)->fd, &ctc_ev,
                sizeof(ctc_ev));
            if (ctc_ev & _CTC_TAKE_MASK) {
                _worker_move_commit_ctc(self);
            }
            if (ctc_ev & _CTC_PUSH_MASK) {
                _worker_take_accept_ctc(self, &self->base.enter_ctcs);
            }
        } else {
            if (_dpefd_feed(efd, evd->events)) {
                _dptmrset_remove(&node->timer, efd);
                return efd;
            }
        }
    }
    return NULL;
}

dpevp_t* worker_new(int type, int id)
{
    worker_t* self = (worker_t*)calloc(1, sizeof(worker_t));
    if (self == NULL) {
        dplog_error("dpevp", "Failed to allocate worker %d", id);
        return NULL;
    }
    if (!_dpevp_init(&self->base, type, id)) {
        free(self);
        return NULL;
    }

    self->base.commit_ctc_f = _worker_commit_ctc;
    self->base.finish_ctc_f = _worker_finish_ctc;

    _dpele_deque_init(&self->accepting_ctcs);
    _dpele_deque_init(&self->commiting_ctcs);
    _dpele_deque_init(&self->commited_ctcs);
    _dpele_deque_init(&self->accepted_ctcs);

    self->is_respond_ctc = self->is_request_ctc = true;

    _dpele_wake_init(&self->accept_wake, id);
    _dpele_wake_init(&self->commit_wake, id);

    self->base.reap_funcs[DPELE_TYPE_EFD] = _worker_reap_efd;
    self->base.reap_funcs[DPELE_TYPE_CTC] = _worker_reap_ctc;

    return (dpevp_t*)self;
}

void worker_del(dpevp_t* evp)
{
    worker_t* self = (worker_t*)evp;
    _dpevp_free(&self->base);

    _dpele_deque_clear(&self->accepting_ctcs);
    _dpele_deque_clear(&self->commiting_ctcs);
    _dpele_deque_clear(&self->accepted_ctcs);
    _dpele_deque_clear(&self->commited_ctcs);

    _dpele_wake_fini(&self->accept_wake);
    _dpele_wake_fini(&self->commit_wake);
    free(self);
}

bool worker_start(dpevp_t* self)
{
    worker_t* worker = (worker_t*)self;
    struct epoll_event evt;
    evt.data.ptr = &worker->accept_wake;
    evt.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    return epoll_ctl(worker->base.epoll_fd, EPOLL_CTL_ADD, worker->accept_wake.fd,
               &evt)
        == 0;
}

typedef struct _dpevp master_t;

static void _master_finish_ctc(dpevp_t* self, dpctc_t* ctc)
{
    if (ctc->fromid == self->id) {
        _dpele_deque_push_back(&self->enter_ctcs, ctc);
    } else {
        _worker_push_accept_ctc((worker_t*)_gevps.evps[ctc->fromid], ctc);
    }
}

static void _master_commit_ctc(dpevp_t* self, dpctc_t* ctc)
{
    if (ctc->toid == self->id) {
        _dpele_deque_push_back(&self->enter_ctcs, ctc);
    } else {
        _worker_push_accept_ctc((worker_t*)_gevps.evps[ctc->toid], ctc);
    }
}

static dpctc_t* _master_reap_ctc(dpevp_t* node)
{
    master_t* self = (master_t*)node;
    dpctc_t* ctc = NULL;

    // Execute at most DPCTC_STEP_LENGTH ctcs each time
    dpele_deque_t* que = &self->enter_ctcs;
    while ((ctc = _dpele_deque_pop_front(que)) != NULL) {
        switch (_dpctc_stat(ctc)) {
        case DPCTC_COMMIT: {
            if (_dpctc_ces_stat(ctc, DPCTC_RUNNING, DPCTC_COMMIT)) {
                if (ctc->toid == 0) {
                    return ctc;
                } else {
                    _worker_push_accept_ctc((worker_t*)_gevps.evps[ctc->toid], ctc);
                }
                break;
            } // Otherwise jump to DPTASK_S_CANCEL
        }
        case DPCTC_CANCEL: {
            if (_dpctc_ces_stat(ctc, DPCTC_FINISH, DPCTC_CANCEL)) {
                if (!_dpctc_del(ctc)) {
                    _master_finish_ctc(node, ctc);
                }
                break;
            } // Otherwise jump to default
        }
        // ignore DPCTC_RUNNING
        default: {
            if (ctc->fromid == 0) {
                _dptmrset_remove(&node->timer, ctc);
                if (!_dpctc_del(ctc)) {
                    return ctc;
                }
            } else {
                _worker_push_accept_ctc((worker_t*)_gevps.evps[ctc->fromid], ctc);
            }
            break;
        }
        }
    }
    return NULL;
}

static dpefd_t* _master_reap_efd(dpevp_t* node)
{
    if (node->ev_index >= node->ev_count) {
        return NULL;
    }

    master_t* self = (master_t*)node;
    struct epoll_event* evd = NULL;
    dpefd_t* efd = NULL;
    uint64_t ctc_ev = 0;

    while (node->ev_index < node->ev_count) {
        evd = &node->evs[node->ev_index];
        // EPOLL基本事件和DPEVT基本一致，不需要额外转换，EPOLLRDHUP为特有的事件
        if (evd->events & EPOLLRDHUP) {
            evd->events |= DPEVT_HUP;
        }

        node->ev_index++;
        efd = (dpefd_t*)evd->data.ptr;
        if (efd->type->type == DPELE_TYPE_WEAK) {
            struct dpele_wake* wfd = (struct dpele_wake*)evd->data.ptr;
            worker_t* worker = (worker_t*)_gevps.evps[wfd->id];

            (void)!read(wfd->fd, &ctc_ev, sizeof(ctc_ev));
            if (ctc_ev & _CTC_TAKE_MASK) {
                _worker_move_accept_ctc(worker);
            }

            if (ctc_ev & _CTC_PUSH_MASK) {
                _worker_take_commit_ctc(worker, &self->enter_ctcs);
            }
        } else {
            if (_dpefd_feed(efd, evd->events)) {
                _dptmrset_remove(&node->timer, efd);
                return efd;
            }
        }
    }
    return NULL;
}

dpevp_t* master_new(int type, int id)
{
    master_t* self = (master_t*)calloc(1, sizeof(master_t));
    if (self == NULL) {
        dplog_error("dpevp", "Failed to allocate master event loop");
        return NULL;
    }
    if (!_dpevp_init(self, 0, 0)) {
        free(self);
        return NULL;
    }

    self->commit_ctc_f = _master_commit_ctc;
    self->finish_ctc_f = _master_finish_ctc;
    self->reap_funcs[DPELE_TYPE_EFD] = _master_reap_efd;
    self->reap_funcs[DPELE_TYPE_CTC] = _master_reap_ctc;

    return self;
}

void master_del(dpevp_t* self)
{
    _dpevp_free(self);
    free(self);
}

bool master_start(dpevp_t* self)
{
    worker_t* worker = NULL;
    for (size_t i = 1; i < _gevps.count; i++) {
        worker = (worker_t*)_gevps.evps[i];
        struct epoll_event evt;
        evt.data.ptr = &worker->commit_wake;
        evt.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
        if (epoll_ctl(self->epoll_fd, EPOLL_CTL_ADD, worker->commit_wake.fd, &evt)
            != 0) {
            return false;
        }
    }
    return true;
}

bool _dpele_asc_over(dpele_t* ele)
{
    // 已经执行结束的AIO：不需要再执行post
    if (ele->type->type == DPELE_TYPE_EFD || ele->type->iotype != DPAIO_TYPE_NAN) {
        ele->want_events = 0;
        ele->asc_type = NULL;
        goto OVER;
    }

    // 正在运行的 CTC 回调派发：post==NULL 时 pop 给绑定层
    if (ele->type->type == DPELE_TYPE_CTC && _dpctc_stat(ele) == DPCTC_RUNNING
        && ele->asc_type != NULL && ele->asc_type->post == NULL) {
        return true;
    }

    // 执行post
    const dpasc_t* asct = ele->asc_type;
    ele->want_events = 0;
    ele->asc_type = NULL;

    if (asct && asct->post) {
        dpasc_out_t out = {.data = dpele_asc_data(ele)};
        dpret_t ret = asct->post(ele, &out);
        ele->ret = ret;
    }

OVER:
    if (dpele_refc(ele) <= 0) {
        _dpele_del(ele);
        return false;
    }

    return !dpele_is_detach(ele);
}

#define DPEVP_REAP_ELE(self__, func__, rbody__)                                     \
    {                                                                               \
        while ((ele = (func__)(self__)) != NULL) {                                  \
            if (_dpele_asc_over(ele)) {                                             \
                rbody__;                                                            \
                return ele;                                                         \
            }                                                                       \
        }                                                                           \
    }

#define DPEVP_REAP_LIMIT 32

dpele_t* dpevp_pop(int timeout_ms)
{
    dpevp_t* self = _gevp;
    dpele_t* ele = NULL;

    DPEVP_REAP_ELE(self, self->reap_funcs[DPELE_TYPE_EFD], {})

    // 限制连续处理非EFD类型的个数，避免饿死epoll_wait
    for (int i = 1; i < 4 && self->pop_limit < DPEVP_REAP_LIMIT; i++) {
        DPEVP_REAP_ELE(self, self->reap_funcs[i], self->pop_limit++)
    }

    if (self->pop_limit >= DPEVP_REAP_LIMIT || self->ready.head) {
        timeout_ms = 0;
    } else if (self->timer_waitms >= 0
        && (timeout_ms < 0 || self->timer_waitms < timeout_ms)) {
        timeout_ms = self->timer_waitms;
    }
    self->pop_limit = 0;

    self->ev_index = 0;
    self->ev_count = epoll_wait(self->epoll_fd, self->evs, self->ev_capacity,
        timeout_ms);

    return NULL;
}

dpret_t dpevp_add(dpele_t* ele, const dpasc_t* asc, ...)
{
    va_list vargs;
    va_start(vargs, asc);
    dpret_t ret = dpevp_addv(ele, asc, vargs);
    va_end(vargs);
    return ret;
}

dpret_t dpevp_addv(dpele_t* ele, const dpasc_t* asc, va_list vargs)
{
    if (ele == NULL || asc == NULL || (asc->types & (1 << ele->type->type)) == 0
        || (asc->iotypes != 0 && (asc->iotypes & (1U << ele->type->iotype)) == 0)) {
        return DPE_INVAL;
    }

    if (dpele_is_doing(ele) && (asc->flags & DPASC_FLAG_ALLOW_DOING) == 0) {
        return DPE_PERM;
    }

    ele->asc_type = asc;
    dpasc_out_t out = {
        .want_events = 0,
        .inva_events = 0,
        .able_events = ele->able_events,
        .data = _dpele_asc_data(ele),
    };
    dpret_t ret = asc->prep(ele, vargs, &out);
    if (ret != DPE_CONTINUE) {
        goto FINISH;
    }

    switch (ele->type->type) {
    case DPELE_TYPE_EFD: {
        ret = _dpefd_wake(ele, &out);
        break;
    }
    case DPELE_TYPE_CTC: {
        dpevp_t* self = _gevp;
        _dpctc_set_stat(ele, DPCTC_COMMIT);
        ele->next = NULL;
        ele->fromid = self->id;
        ele->is_reto = 0;
        ele->cop.ptr = NULL;
        self->commit_ctc_f(self, ele);
        ret = DPE_WAIT;
        break;
    }
    default: {
        // 只有EFD会直接调用post消耗事件,其他类型应该在后续触发后调用
        ret = DPE_WAIT;
        break;
    }
    }

FINISH:
    ele->ret = ret;
    if (dpret_isok(ele->ret)) { // 模拟在事件循环中过一遍
        _dpele_deque_push_back(&_gevp->ready, ele);
        return ele->is_detach ? ret : DPE_WAIT;
    } else if (ele->ret == DPE_WAIT) {
        ele->ret = DPE_INPROGRESS;
        _dptmrset_upsert(&_gevp->timer, ele); // 提供超时
        return ele->is_detach ? DPE_OK : DPE_WAIT;
    } else {
        ele->asc_type = NULL;
        return ele->ret;
    }
}

dpret_t dpevp_end(dpele_t* ele, dpret_t ret)
{
    dpevp_t* self = _gevp;
    if (!dpele_is_doing(ele)) {
        return DPE_OK;
    }

    if (ele->type->type == DPELE_TYPE_CTC && ele->fromid != self->id) {
        return DPE_PERM;
    }

    _dptmrset_remove(&self->timer, ele);

    switch (ele->type->type) {
    case DPELE_TYPE_EFD: {
        ele->want_events = DPEVT_NAN;
        ele->ret = ret;
        _dpele_deque_push_back(&self->ready, ele);
        break;
    }
    case DPELE_TYPE_USD: {
        ele->ret = ret;
        _dpele_deque_push_back(&self->ready, ele);
        break;
    }
    case DPELE_TYPE_CTC: {
        if (_dpctc_ces_stat(ele, DPCTC_CANCEL, DPCTC_COMMIT)) {
            ele->ret = ret;
        }
        break;
    }
    case DPELE_TYPE_TMR: {
        ele->ret = ret;
        _dpele_deque_push_back(&self->ready, ele);
        break;
    }
    }

    return DPE_OK;
}

dpret_t dpctc_reto(dpctc_t* self, int toid)
{
    if (self == NULL || self->type->type != DPELE_TYPE_CTC) {
        return DPE_INVAL;
    }

    toid = _dpctc_resolve_toid(self, toid);
    if (dpret_iserr(toid)) {
        return toid;
    }

    if (_dpctc_stat(self) != DPCTC_RUNNING) {
        return DPE_PERM;
    }

    self->toid = (uint8_t)toid;
    self->is_reto = 1;
    return DPE_OK;
}

static dpret_t _dpctc_submit_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    if (ctc->type->type != DPELE_TYPE_CTC) {
        return DPE_INVAL;
    }

    dpctc_stat_e stat = _dpctc_stat(ctc);
    if (stat > DPCTC_INITIAL && stat < DPCTC_FINISH) {
        return DPE_REPEAT;
    }

    dpv64_t* data = (dpv64_t*)out->data;
    if (data == NULL) {
        return DPE_INVAL;
    }
    data[0] = va_arg(arg, dpv64_t);
    data[1] = va_arg(arg, dpv64_t);

    return DPE_CONTINUE;
}

DPASC_CTC_FUNCTION(submit, _dpctc_submit_prep, NULL, sizeof(dpv64_t) * 2, 0)

dpret_t dpevp_end_ctc_(dpctc_t* ctc, dpret_t err)
{
    if (ctc == NULL || ctc->type->type != DPELE_TYPE_CTC || !dpele_is_doing(ctc)) {
        return DPE_INVAL;
    }

    dpevp_t* self = _gevp;
    ctc->ret = err;

    if (ctc->is_reto) {
        ctc->is_reto = false;
        _dpctc_set_stat(ctc, DPCTC_COMMIT);
        self->commit_ctc_f(self, ctc);
    } else {
        _dpctc_set_stat(ctc, DPCTC_FINISH);
        if (ctc->is_detach) {
            if (dpele_refc(ctc) <= 0) {
                _dpele_del(ctc);
            }
            return DPE_OK;
        }
        self->finish_ctc_f(self, ctc);
    }
    return DPE_OK;
}
