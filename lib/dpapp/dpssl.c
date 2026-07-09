#include "dpapp/dpssl.h"
#include "dpapp/dpasc.h"
#include "dpapp/dpbuf.h"
#include "dpapp/dpdef.h"
#include "dpapp/dpevp.h"
#include "dpapp/dplog.h"
#include "dpapp/dprbt.h"
#include "dpapp/dpret.h"
#include <errno.h>

#include <openssl/ssl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

#if !DPAPP_HAS_SSL

bool dpssl_enable()
{
    return false;
}

dpret_t dpssl_thrd_init()
{
    return DPE_UNSUPPORT;
}

void dpssl_thrd_exit()
{
    return;
}

#else
#include "openssl/bio.h"
#include "openssl/dtls1.h"
#include "openssl/err.h"
#include "openssl/ssl.h"
#include "openssl/tls1.h"

#define _DPSSL_DEFAULT_SNI "@default.sni.dpssl"

bool dpssl_enable()
{
    return true;
}

#define dpssl_log_error(fmt, ...)                                                   \
    do {                                                                            \
        long c = ERR_get_error();                                                   \
        dplog_error("dpssl", "(%ld)%s: " fmt, c, ERR_error_string(c, NULL),         \
            ##__VA_ARGS__);                                                         \
    } while (0)

struct dpssl_group;

typedef struct dpssl_alpn
{
    struct dpssl_alpn* next;
    char alpn[];
} dpssl_alpn_t;

typedef struct dpssl_ctx
{
    struct dpssl_ctx* next;
    dpssl_ori_ctx_t* ctx;
    char* sni; // name 唯一键查找
} dpssl_ctx_t;

typedef struct dpssl_group
{
    struct dpssl_group* next;
    dprole_e role;
    uint16_t min_version;
    uint16_t max_version;
    char name[32];            ///< group 唯一标识
    dpssl_alpn_t* alpns_head; ///< ALPN 链表头
    dpssl_alpn_t* alpns_tail; ///< ALPN 链表尾，后插
    dpbuf_t alpns_wire;       ///< RFC7301 wire，alpns 变更后懒同步
    bool alpns_wire_dirty;
    dpssl_ctx_t* ctx_head;        ///< 仅用于服务端
    dpssl_ori_ctx_t* default_ctx; ///< dpssl_add 创建的默认 ctx
} dpssl_group_t;

typedef struct dpssl_thrd_mgr
{
    dpssl_group_t* groups; ///< 以 name 为唯一键查找
} dpssl_thrd_mgr_t;

static __thread struct dpssl_thrd_mgr _dpssl_thrd_mgr = {NULL};

static void _dpssl_ctx_free(dpssl_ctx_t* ctx);
static void _dpssl_group_del_all_ctx(dpssl_group_t* grp);
static void _dpssl_group_free(dpssl_group_t* grp);

dpret_t dpssl_thrd_init()
{
    return DPE_OK;
}

void dpssl_thrd_exit()
{
    dpssl_thrd_mgr_t* mgr = &_dpssl_thrd_mgr;
    while (mgr->groups != NULL) {
        dpssl_group_t* grp = mgr->groups;
        mgr->groups = grp->next;
        _dpssl_group_free(grp);
    }
}

static dpret_t _dpssl_group_find(const char* group, dpssl_group_t** out)
{
    if (group == NULL || group[0] == '\0' || strlen(group) > 31) {
        return DPE_INVAL;
    }

    dpssl_thrd_mgr_t* mgr = &_dpssl_thrd_mgr;
    dpssl_group_t* grp = NULL;
    for (dpssl_group_t* cur = mgr->groups; cur != NULL; cur = cur->next) {
        if (strcmp(cur->name, group) == 0) {
            grp = cur;
            break;
        }
    }

    if (out) {
        *out = grp;
    }
    return grp ? DPE_OK : DPE_NOTEXISTS;
}

/* --- ALPN --- */

static const char* _dpssl_alpn_at(const dpssl_group_t* grp, int idx)
{
    if (grp == NULL || idx < 0) {
        return NULL;
    }

    dpssl_alpn_t* node = grp->alpns_head;
    int cur = 0;
    while (node != NULL) {
        if (cur == idx) {
            return node->alpn;
        }
        node = node->next;
        cur++;
    }
    return NULL;
}

static bool _dpssl_alpn_contains(const dpssl_group_t* grp, const char* sub_alpn)
{
    if (sub_alpn == NULL || sub_alpn[0] == '\0') {
        return false;
    }

    for (dpssl_alpn_t* node = grp->alpns_head; node != NULL; node = node->next) {
        if (strcmp(node->alpn, sub_alpn) == 0) {
            return true;
        }
    }
    return false;
}

static dpret_t _dpssl_alpns_wire_sync(dpssl_group_t* grp)
{
    if (!grp->alpns_wire_dirty) {
        return DPE_OK;
    }

    dpbuf_reset(&grp->alpns_wire, DPBUF_INIT_W);

    for (dpssl_alpn_t* node = grp->alpns_head; node != NULL; node = node->next) {
        size_t n = strlen(node->alpn);
        if (n > 255) {
            return DPE_NAMETOOLONG;
        }
        uint8_t len_byte = (uint8_t)n;
        if (dpbuf_wdata(&grp->alpns_wire, &len_byte, 1) != 1) {
            return DPE_NOMEM;
        }
        if (dpbuf_wdata(&grp->alpns_wire, node->alpn, (int)n) != (int)n) {
            return DPE_NOMEM;
        }
    }

    dpbuf_eseek(&grp->alpns_wire, 0, SEEK_END);
    grp->alpns_wire_dirty = false;
    return DPE_OK;
}

static dpret_t _dpssl_add_alpn(dpssl_group_t* grp, const char* sub_alpn)
{
    if (sub_alpn == NULL || sub_alpn[0] == '\0') {
        return DPE_INVAL;
    }

    if (_dpssl_alpn_contains(grp, sub_alpn)) {
        return DPE_OK;
    }

    size_t alpn_len = strlen(sub_alpn);
    if (alpn_len > 255) {
        return DPE_NAMETOOLONG;
    }

    dpssl_alpn_t* node = malloc(sizeof(dpssl_alpn_t) + alpn_len + 1);
    if (node == NULL) {
        return DPE_NOMEM;
    }
    node->next = NULL;
    memcpy(node->alpn, sub_alpn, alpn_len + 1);

    if (grp->alpns_tail != NULL) {
        grp->alpns_tail->next = node;
    } else {
        grp->alpns_head = node;
    }
    grp->alpns_tail = node;

    grp->alpns_wire_dirty = true;
    return DPE_OK;
}

static void _dpssl_alpns_free(dpssl_group_t* grp)
{
    dpssl_alpn_t* node = grp->alpns_head;
    while (node != NULL) {
        dpssl_alpn_t* next = node->next;
        free(node);
        node = next;
    }
    grp->alpns_head = NULL;
    grp->alpns_tail = NULL;
}

static int _dpssl_alpn_select(SSL* ssl, const unsigned char** out,
    unsigned char* outlen, const unsigned char* in, unsigned int inlen, void* arg)
{
    (void)ssl;
    dpssl_group_t* grp = (dpssl_group_t*)arg;
    _dpssl_alpns_wire_sync(grp);

    int r = SSL_select_next_proto((unsigned char**)out, outlen, in, inlen,
        (unsigned char*)dpbuf_crdata(&grp->alpns_wire),
        (unsigned int)dpbuf_crsize(&grp->alpns_wire));
    if (r == OPENSSL_NPN_NEGOTIATED) {
        return SSL_TLSEXT_ERR_OK;
    }

    dplog_warn("dpssl", "no supported protocol can be selected from %.*s",
        (int)inlen, (char*)in);
    return SSL_TLSEXT_ERR_ALERT_FATAL;
}

dpret_t dpssl_add_alpn(const char* group, const char* alpn)
{
    if (alpn == NULL) {
        return DPE_INVAL;
    }

    dpssl_group_t* grp = NULL;
    dpret_t ret = _dpssl_group_find(group, &grp);
    if (ret != DPE_OK) {
        dplog_error("dpssl", "add_alpn: group '%s' not found (%d)",
            group ? group : "(null)", ret);
        return ret;
    }

    return _dpssl_add_alpn(grp, alpn);
}

dpret_t dpssl_has_alpn(const char* group, const char* alpn)
{
    if (alpn == NULL || alpn[0] == '\0') {
        return DPE_INVAL;
    }

    dpssl_group_t* grp = NULL;
    dpret_t ret = _dpssl_group_find(group, &grp);
    if (ret != DPE_OK) {
        return ret;
    }

    return _dpssl_alpn_contains(grp, alpn) ? DPE_OK : DPE_NOTEXISTS;
}

dpret_t dpssl_has_version(const char* group, uint16_t min_version,
    uint16_t max_version)
{
    dpssl_group_t* grp = NULL;
    dpret_t ret = _dpssl_group_find(group, &grp);
    if (ret != DPE_OK) {
        return ret;
    }

    uint16_t minv = grp->min_version;
    uint16_t maxv = grp->max_version;

    if (min_version && (minv > min_version || min_version > maxv)) {
        return DPE_NOTEXISTS;
    }

    if (max_version && (maxv < max_version || max_version < minv)) {
        return DPE_NOTEXISTS;
    }

    return DPE_OK;
}

const char* dpssl_get_alpn(const char* group, int idx)
{
    if (idx < 0) {
        return NULL;
    }

    dpssl_group_t* grp = NULL;
    if (_dpssl_group_find(group, &grp) != DPE_OK) {
        return NULL;
    }

    return _dpssl_alpn_at(grp, idx);
}

/* --- group --- */

static dpret_t _dpssl_group_add(const char* group, dprole_e role,
    uint16_t min_version, uint16_t max_version, dpssl_group_t** out_grp)
{
    if (group == NULL || group[0] == '\0' || strlen(group) > 31
        || (role != DPROLE_CLIENT && role != DPROLE_SERVER)) {
        return DPE_INVAL;
    }

    dpssl_group_t* grp = calloc(1, sizeof(dpssl_group_t));
    if (grp == NULL) {
        return DPE_NOMEM;
    }
    grp->role = role;
    grp->min_version = min_version;
    grp->max_version = max_version;
    strncpy(grp->name, group, 31);
    grp->name[31] = '\0';

    if (!dpbuf_init(&grp->alpns_wire, DPBUF_S_SIZE)) {
        free(grp);
        return DPE_NOMEM;
    }

    dpssl_thrd_mgr_t* mgr = &_dpssl_thrd_mgr;
    if (mgr->groups == NULL) {
        mgr->groups = grp;
    } else {
        grp->next = mgr->groups;
        mgr->groups = grp;
    }
    if (out_grp) {
        *out_grp = grp;
    }
    return DPE_OK;
}

static dpret_t _dpssl_ctx_find(dpssl_group_t* grp, const char* sni,
    dpssl_ctx_t** out_ctx)
{
    if (sni == NULL || sni[0] == '\0' || grp == NULL) {
        return DPE_INVAL;
    }

    dpssl_ctx_t* ctx = NULL;
    for (dpssl_ctx_t* cur = grp->ctx_head; cur != NULL; cur = cur->next) {
        if (cur->sni && strcmp(cur->sni, sni) == 0) {
            ctx = cur;
            break;
        }
    }

    if (out_ctx) {
        *out_ctx = ctx;
    }
    return ctx ? DPE_OK : DPE_NOTEXISTS;
}

static dpret_t _dpssl_ctx_add(dpssl_group_t* grp, const char* sni,
    dpssl_ori_ctx_t* ori_ctx, dpssl_ctx_t** out_ctx)
{
    if (sni == NULL || sni[0] == '\0' || grp == NULL || ori_ctx == NULL) {
        return DPE_INVAL;
    }

    dpssl_ctx_t* ctx = calloc(1, sizeof(dpssl_ctx_t));
    if (ctx == NULL) {
        return DPE_NOMEM;
    }
    ctx->sni = strdup(sni);
    ctx->ctx = ori_ctx;

    if (grp->ctx_head == NULL) {
        grp->ctx_head = ctx;
    } else {
        ctx->next = grp->ctx_head;
        grp->ctx_head = ctx;
    }

    if (out_ctx) {
        *out_ctx = ctx;
    }
    return DPE_OK;
}

static void _dpssl_ctx_free(dpssl_ctx_t* ctx)
{
    if (ctx == NULL) {
        return;
    }
    if (ctx->ctx) {
        SSL_CTX_free(ctx->ctx);
    }
    free(ctx->sni);
    free(ctx);
}

static void _dpssl_group_del_all_ctx(dpssl_group_t* grp)
{
    while (grp->ctx_head != NULL) {
        dpssl_ctx_t* ctx = grp->ctx_head;
        grp->ctx_head = ctx->next;
        _dpssl_ctx_free(ctx);
    }
}

static void _dpssl_group_free(dpssl_group_t* grp)
{
    _dpssl_group_del_all_ctx(grp);
    if (grp->default_ctx) {
        SSL_CTX_free(grp->default_ctx);
        grp->default_ctx = NULL;
    }
    _dpssl_alpns_free(grp);
    dpbuf_fini(&grp->alpns_wire);
    free(grp);
}

static dpret_t _dpssl_group_unlink(const char* group, dpssl_group_t** out_grp)
{
    if (group == NULL || group[0] == '\0' || strlen(group) > 31) {
        return DPE_INVAL;
    }

    dpssl_thrd_mgr_t* mgr = &_dpssl_thrd_mgr;
    dpssl_group_t* prev = NULL;
    for (dpssl_group_t* cur = mgr->groups; cur != NULL; cur = cur->next) {
        if (strcmp(cur->name, group) == 0) {
            if (prev) {
                prev->next = cur->next;
            } else {
                mgr->groups = cur->next;
            }
            if (out_grp) {
                *out_grp = cur;
            }
            return DPE_OK;
        }
        prev = cur;
    }
    return DPE_NOTEXISTS;
}

static dpret_t _dpssl_ctx_unlink(dpssl_group_t* grp, const char* sni,
    dpssl_ctx_t** out_ctx)
{
    if (sni == NULL || sni[0] == '\0' || grp == NULL) {
        return DPE_INVAL;
    }

    dpssl_ctx_t* prev = NULL;
    for (dpssl_ctx_t* cur = grp->ctx_head; cur != NULL; cur = cur->next) {
        if (cur->sni && strcmp(cur->sni, sni) == 0) {
            if (prev) {
                prev->next = cur->next;
            } else {
                grp->ctx_head = cur->next;
            }
            if (out_ctx) {
                *out_ctx = cur;
            }
            return DPE_OK;
        }
        prev = cur;
    }
    return DPE_NOTEXISTS;
}

int _dpssl_sni_select(SSL* ssl, int* out_alert, void* arg)
{
    dpssl_group_t* grp = (dpssl_group_t*)arg;
    const char* name = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);

    bool not_default = true;
    if (name == NULL || name[0] == '\0') {
        name = _DPSSL_DEFAULT_SNI;
        not_default = false;
    }

    dpssl_ctx_t* ctx = NULL;
    if (_dpssl_ctx_find(grp, name, &ctx) != DPE_OK && not_default) {
        _dpssl_ctx_find(grp, _DPSSL_DEFAULT_SNI, &ctx);
    }
    if (ctx == NULL) {
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }
    SSL_set_SSL_CTX(ssl, ctx->ctx);
    return SSL_TLSEXT_ERR_OK;
}

dpret_t _dpssl_use_crt_key(SSL_CTX* ssl_ctx, const char* crt, const char* key)
{
    if (crt == NULL || key == NULL) {
        return DPE_INVAL;
    }

    if (SSL_CTX_use_certificate_file(ssl_ctx, crt, SSL_FILETYPE_PEM) <= 0) {
        dpssl_log_error("Failed to load server certificate");
        return DPE_INTERNAL_ERROR;
    }
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key, SSL_FILETYPE_PEM) <= 0) {
        dpssl_log_error("Failed to load server private key");
        return DPE_INTERNAL_ERROR;
    }
    if (SSL_CTX_check_private_key(ssl_ctx) != 1) {
        dpssl_log_error("Private key does not match certificate");
        return DPE_INTERNAL_ERROR;
    }

    return DPE_OK;
}

dpret_t _dpssl_new_ori_ctx(uint16_t min_version, uint16_t max_version,
    dpssl_ori_ctx_t** out_ctx)
{
    if (out_ctx == NULL) {
        return DPE_INVAL;
    }

    const SSL_METHOD* method = NULL;
    if (min_version >> 8 == SSL3_VERSION_MAJOR) {
        method = TLS_method();
    } else if (min_version >> 8 == DTLS1_VERSION_MAJOR) {
        method = DTLS_method();
    } else {
        method = TLS_method();
        min_version = TLS1_3_VERSION;
        max_version = TLS1_3_VERSION;
    }

    SSL_CTX* ssl_ctx = SSL_CTX_new(method);
    if (!ssl_ctx) {
        dpssl_log_error("Failed to create SSL context");
        return DPE_INTERNAL_ERROR;
    }

    if (min_version) {
        SSL_CTX_set_min_proto_version(ssl_ctx, min_version);
    }
    if (max_version) {
        SSL_CTX_set_max_proto_version(ssl_ctx, max_version);
    }

    if (!SSL_CTX_set_default_verify_paths(ssl_ctx)) {
        dpssl_log_error("Failed to load default CA certificates");
        SSL_CTX_free(ssl_ctx);
        return DPE_INTERNAL_ERROR;
    }

    *out_ctx = ssl_ctx;
    return DPE_OK;
}

dpret_t dpssl_add(const char* group, dprole_e role, uint16_t min_version,
    uint16_t max_version)
{
    dpssl_group_t* grp = NULL;
    dpret_t ret = _dpssl_group_find(group, &grp);
    if (ret == DPE_OK) {
        return DPE_OK;
    }
    if (ret != DPE_NOTEXISTS) {
        dplog_error("dpssl", "add: invalid group '%s' (%d)",
            group ? group : "(null)", ret);
        return ret;
    }

    dpssl_ori_ctx_t* ori_ctx = NULL;
    ret = _dpssl_new_ori_ctx(min_version, max_version, &ori_ctx);
    if (ret != DPE_OK) {
        return ret;
    }

    min_version = SSL_CTX_get_min_proto_version(ori_ctx);
    max_version = SSL_CTX_get_max_proto_version(ori_ctx);

    ret = _dpssl_group_add(group, role, min_version, max_version, &grp);
    if (ret != DPE_OK) {
        SSL_CTX_free(ori_ctx);
        dplog_error("dpssl", "add: failed to register group '%s' (%d)", group, ret);
        return ret;
    }

    if (role == DPROLE_SERVER) {
        SSL_CTX_set_alpn_select_cb(ori_ctx, _dpssl_alpn_select, grp);
        SSL_CTX_set_tlsext_servername_callback(ori_ctx, _dpssl_sni_select);
        SSL_CTX_set_tlsext_servername_arg(ori_ctx, grp);
    }

    grp->default_ctx = ori_ctx;
    dplog_info("dpssl", "Registered group '%s' role=%s tls=0x%04x-0x%04x", group,
        role == DPROLE_SERVER ? "server" : "client", min_version, max_version);
    return DPE_OK;
}

dpret_t dpssl_del(const char* group)
{
    dpssl_group_t* grp = NULL;
    dpret_t ret = _dpssl_group_unlink(group, &grp);
    if (ret != DPE_OK) {
        return ret;
    }

    _dpssl_group_free(grp);
    return DPE_OK;
}

dprole_e dpssl_role(const char* group)
{
    dpssl_group_t* grp = NULL;
    if (_dpssl_group_find(group, &grp) != DPE_OK) {
        return DPROLE_UNSURE;
    }
    return grp->role;
}

dpret_t dpssl_del_ctx(const char* group, const char* name)
{
    if (name == NULL) {
        return DPE_INVAL;
    }
    if (name[0] == '\0') {
        name = _DPSSL_DEFAULT_SNI;
    }

    dpssl_group_t* grp = NULL;
    dpret_t ret = _dpssl_group_find(group, &grp);
    if (ret != DPE_OK) {
        return ret;
    }

    dpssl_ctx_t* ctx = NULL;
    ret = _dpssl_ctx_unlink(grp, name, &ctx);
    if (ret != DPE_OK) {
        return ret;
    }
    _dpssl_ctx_free(ctx);
    return DPE_OK;
}

void dpssl_del_all_ctx(const char* group)
{
    dpssl_group_t* grp = NULL;
    if (_dpssl_group_find(group, &grp) != DPE_OK) {
        return;
    }
    _dpssl_group_del_all_ctx(grp);
}

dpret_t dpssl_add_ctx(const char* group, const char* sni, const char* crt,
    const char* key)
{
    if (sni == NULL) {
        return DPE_INVAL;
    }
    if (sni[0] == '\0') {
        sni = _DPSSL_DEFAULT_SNI;
    }

    dpssl_group_t* grp = NULL;
    dpret_t ret = _dpssl_group_find(group, &grp);
    if (ret != DPE_OK) {
        dplog_error("dpssl", "add_ctx: group '%s' not found (%d)", group, ret);
        return ret;
    }

    if (grp->role != DPROLE_SERVER) {
        dplog_error("dpssl", "add_ctx: group '%s' is not a server role", group);
        return DPE_PERM;
    }

    dpssl_ctx_t* ctx = NULL;
    ret = _dpssl_ctx_find(grp, sni, &ctx);
    if (ctx) {
        return DPE_OK;
    }

    dpssl_ori_ctx_t* ssl_ctx = NULL;
    ret = _dpssl_new_ori_ctx(grp->min_version, grp->max_version, &ssl_ctx);
    if (ret != DPE_OK) {
        return ret;
    }

    ret = _dpssl_use_crt_key(ssl_ctx, crt, key);
    if (ret != DPE_OK) {
        SSL_CTX_free(ssl_ctx);
        return ret;
    }
    SSL_CTX_set_alpn_select_cb(ssl_ctx, _dpssl_alpn_select, grp);

    ret = _dpssl_ctx_add(grp, sni, ssl_ctx, &ctx);
    if (ret != DPE_OK) {
        SSL_CTX_free(ssl_ctx);
        if (ret == DPE_NOMEM) {
            dplog_error("dpssl", "add_ctx: out of memory for group '%s' sni '%s'",
                group, sni);
        }
        return ret;
    }

    dplog_info("dpssl", "Added server certificate: group='%s' sni='%s'", group,
        strcmp(sni, _DPSSL_DEFAULT_SNI) == 0 ? "" : sni);
    return DPE_OK;
}

dpssl_ori_ctx_t* dpssl_get_ctx(const char* group, const char* sni)
{
    dpssl_group_t* grp = NULL;
    dpret_t ret = _dpssl_group_find(group, &grp);
    if (ret != DPE_OK) {
        return NULL;
    }

    if (sni == NULL) {
        return grp->default_ctx;
    }

    bool not_default = true;
    if (sni[0] == '\0') {
        sni = _DPSSL_DEFAULT_SNI;
        not_default = false;
    }

    dpssl_ctx_t* ctx = NULL;
    if (_dpssl_ctx_find(grp, sni, &ctx) != DPE_OK && not_default) {
        _dpssl_ctx_find(grp, _DPSSL_DEFAULT_SNI, &ctx);
    }
    return ctx ? ctx->ctx : NULL;
}

/** @brief SSL 会话元素（dpele_t）的逐元素用户数据。 */

typedef struct
{
    dpefd_t* efd;
    dpssl_ori_ssn_t* ssn;
    dprole_e role;
} dpssl_ssn_udata_t;

static dpret_t _dpssl_client_uinit(void* udata, va_list vlist)
{
    dpssl_ssn_udata_t* u = (dpssl_ssn_udata_t*)udata;
    dpefd_t* efd = va_arg(vlist, dpefd_t*);
    const char* group = va_arg(vlist, const char*);
    const char* sni = va_arg(vlist, const char*);

    if (efd == NULL || group == NULL || group[0] == '\0') {
        return DPE_INVAL;
    }

    if (dpele_is_doing(efd)) {
        return DPE_PERM;
    }

    dpssl_group_t* grp = NULL;
    dpret_t ret = _dpssl_group_find(group, &grp);
    if (ret != DPE_OK) {
        dplog_error("dpssl", "client init: group '%s' not found (%d)", group, ret);
        return ret;
    }
    if (grp->role != DPROLE_CLIENT) {
        dplog_error("dpssl", "client init: group '%s' is not a client role", group);
        return DPE_INVAL;
    }

    dpssl_ori_ssn_t* ssn = SSL_new(grp->default_ctx);
    if (!ssn) {
        dpssl_log_error("Failed to create SSL object");
        return DPE_INTERNAL_ERROR;
    }

    int fd = dpefd_fd(efd);
    BIO* bio = BIO_new_socket(fd, BIO_NOCLOSE);
    if (!bio) {
        dpssl_log_error("Failed to create BIO object");
        SSL_free(ssn);
        return DPE_INTERNAL_ERROR;
    }
    SSL_set_bio(ssn, bio, bio);

    u->efd = efd;
    u->ssn = ssn;
    u->role = DPROLE_CLIENT;
    SSL_set_connect_state(u->ssn);

    if (sni && sni[0] != '\0') {
        SSL_set_tlsext_host_name(u->ssn, sni);
    }
    if (grp->alpns_head != NULL) {
        _dpssl_alpns_wire_sync(grp);
        SSL_set_alpn_protos(u->ssn, (const uint8_t*)dpbuf_crdata(&grp->alpns_wire),
            (unsigned int)dpbuf_crsize(&grp->alpns_wire));
    }

    dpefd_set_close(efd, false);

    return fd;
}

static dpret_t _dpssl_server_uinit(void* udata, va_list vlist)
{
    dpssl_ssn_udata_t* u = (dpssl_ssn_udata_t*)udata;
    dpefd_t* efd = va_arg(vlist, dpefd_t*);
    const char* group = va_arg(vlist, const char*);

    if (efd == NULL || group == NULL || group[0] == '\0') {
        return DPE_INVAL;
    }

    if (dpele_is_doing(efd)) {
        return DPE_PERM;
    }

    dpssl_group_t* grp = NULL;
    dpret_t ret = _dpssl_group_find(group, &grp);
    if (ret != DPE_OK) {
        dplog_error("dpssl", "server init: group '%s' not found (%d)", group, ret);
        return ret;
    }
    if (grp->role != DPROLE_SERVER) {
        dplog_error("dpssl", "server init: group '%s' is not a server role", group);
        return DPE_INVAL;
    }

    dpssl_ori_ssn_t* ssn = SSL_new(grp->default_ctx);
    if (!ssn) {
        dpssl_log_error("Failed to create SSL object");
        return DPE_INTERNAL_ERROR;
    }

    int fd = dpefd_fd(efd);
    BIO* bio = BIO_new_socket(fd, BIO_NOCLOSE);
    if (!bio) {
        dpssl_log_error("Failed to create BIO object");
        SSL_free(ssn);
        return DPE_INTERNAL_ERROR;
    }
    SSL_set_bio(ssn, bio, bio);

    u->efd = efd;
    u->ssn = ssn;
    u->role = DPROLE_SERVER;
    SSL_set_accept_state(u->ssn);

    dpefd_set_close(efd, false);

    return fd;
}

static void _dpssl_ssn_ufini(void* udata)
{
    dpssl_ssn_udata_t* u = (dpssl_ssn_udata_t*)udata;
    SSL_free(u->ssn);
    dpele_del(u->efd);
}

const dpele_type_t* dpssl_server_type()
{
    static const dpele_type_t type = {
        .name = "ssl_server",
        .type = DPELE_TYPE_EFD,
        .size = sizeof(dpssl_ssn_udata_t),
        .iotype = DPAIO_TYPE_SSL,
        .events = DPEVT_AIO,
        .init = _dpssl_server_uinit,
        .fini = _dpssl_ssn_ufini,
    };
    return &type;
}

const dpele_type_t* dpssl_client_type()
{
    static const dpele_type_t type = {
        .name = "ssl_client",
        .type = DPELE_TYPE_EFD,
        .size = sizeof(dpssl_ssn_udata_t),
        .iotype = DPAIO_TYPE_SSL,
        .events = DPEVT_AIO,
        .init = _dpssl_client_uinit,
        .fini = _dpssl_ssn_ufini,
    };
    return &type;
}

/* ── dpssl_* prep/post ──────────────────────────────────────────── */

/// SSL 错误 → prep 返回值（WANT_* 返回 DPE_CONTINUE）
static inline dpret_t _dpssl_prep_err(int ssl_err, dpasc_out_t* out)
{
    switch (ssl_err) {
    case SSL_ERROR_SSL:
#if defined(SSL_R_UNEXPECTED_EOF_WHILE_READING)
        if (ERR_GET_REASON(ERR_peek_error()) == SSL_R_UNEXPECTED_EOF_WHILE_READING)
            return DPE_EOF;
#endif
        dpssl_log_error("SSL error");
        return errno > 0 ? -errno : DPE_INTERNAL_ERROR;
    case SSL_ERROR_WANT_READ:
        out->want_events = DPEVT_IN;
        out->inva_events = 0;
        return DPE_CONTINUE;
    case SSL_ERROR_WANT_WRITE:
        out->want_events = DPEVT_OUT;
        out->inva_events = 0;
        return DPE_CONTINUE;
    case SSL_ERROR_SYSCALL:
        return DPE_EOF;
    default:
        dpssl_log_error("SSL error");
        return DPE_CONNRESET;
    }
}

/// SSL 错误 → post 返回值（WANT_* 返回 DPE_WAIT 以等待事件）
static inline dpret_t _dpssl_post_err(int ssl_err, dpasc_out_t* out)
{
    switch (ssl_err) {
    case SSL_ERROR_SSL:
#if defined(SSL_R_UNEXPECTED_EOF_WHILE_READING)
        if (ERR_GET_REASON(ERR_peek_error()) == SSL_R_UNEXPECTED_EOF_WHILE_READING)
            return DPE_EOF;
#endif
        dpssl_log_error("SSL error");
        return errno > 0 ? -errno : DPE_INTERNAL_ERROR;
    case SSL_ERROR_WANT_READ:
        out->inva_events = DPEVT_IN;
        out->want_events = DPEVT_IN;
        return DPE_WAIT;
    case SSL_ERROR_WANT_WRITE:
        out->inva_events = DPEVT_OUT;
        out->want_events = DPEVT_OUT;
        return DPE_WAIT;
    case SSL_ERROR_SYSCALL:
        return DPE_EOF;
    default:
        dpssl_log_error("SSL error");
        return DPE_CONNRESET;
    }
}

static dpret_t _dpssl_handshake_prep(dpele_t* efd, va_list arg, dpasc_out_t* out)
{
    (void)arg;
    dpssl_ssn_udata_t* u = dpele_aux_data(efd);
    if (SSL_is_init_finished(u->ssn)) {
        out->want_events = DPEVT_NAN;
        out->inva_events = 0;
        return DPE_OK;
    }
    int ret = SSL_do_handshake(u->ssn);
    if (ret == 1) {
        out->want_events = DPEVT_NAN;
        out->inva_events = 0;
        return DPE_OK;
    }
    return _dpssl_prep_err(SSL_get_error(u->ssn, ret), out);
}

static dpret_t _dpssl_handshake_post(dpele_t* efd, dpasc_out_t* out)
{
    dpssl_ssn_udata_t* u = dpele_aux_data(efd);
    int ret = SSL_do_handshake(u->ssn);
    if (ret == 1)
        return DPE_OK;
    return _dpssl_post_err(SSL_get_error(u->ssn, ret), out);
}

DPASC_SSL_FUNCTION(handshake, _dpssl_handshake_prep, _dpssl_handshake_post, 0, 0)

static dpret_t _dpssl_shutdown_prep(dpele_t* efd, va_list arg, dpasc_out_t* out)
{
    (void)arg;
    dpssl_ssn_udata_t* u = dpele_aux_data(efd);
    int ret = SSL_shutdown(u->ssn);
    if (ret == 1) {
        out->want_events = DPEVT_NAN;
        out->inva_events = 0;
        return DPE_OK;
    }
    return _dpssl_prep_err(SSL_get_error(u->ssn, ret), out);
}

static dpret_t _dpssl_shutdown_post(dpele_t* efd, dpasc_out_t* out)
{
    dpssl_ssn_udata_t* u = dpele_aux_data(efd);
    int ret = SSL_shutdown(u->ssn);
    if (ret == 1)
        return DPE_OK;
    return _dpssl_post_err(SSL_get_error(u->ssn, ret), out);
}

DPASC_SSL_FUNCTION(shutdown, _dpssl_shutdown_prep, _dpssl_shutdown_post, 0, 0)

static dpret_t _dpssl_recv_prep(dpele_t* efd, va_list arg, dpasc_out_t* out)
{
    dpaio_arg_t* io = (dpaio_arg_t*)out->data;
    io->buf = va_arg(arg, void*);
    io->len = va_arg(arg, int);
    out->want_events = DPEVT_IN;
    out->inva_events = 0;
    if (io->buf == NULL || io->len <= 0)
        return DPE_INVAL;
    return DPE_CONTINUE;
}

static dpret_t _dpssl_recv_post(dpele_t* efd, dpasc_out_t* out)
{
    dpaio_arg_t* io = (dpaio_arg_t*)out->data;
    dpssl_ssn_udata_t* u = dpele_aux_data(efd);
    int total = 0;
    for (;;) {
        int ret = SSL_read(u->ssn, (char*)io->buf + total, io->len - total);
        if (ret > 0) {
            total += ret;
            if (total == io->len)
                return total;
        } else {
            int ssl_err = SSL_get_error(u->ssn, ret);
            ret = _dpssl_post_err(ssl_err, out);
            return total > 0 ? total : ret;
        }
    }
}

DPASC_SSL_FUNCTION(recv, _dpssl_recv_prep, _dpssl_recv_post, sizeof(dpaio_arg_t), 0)

static dpret_t _dpssl_send_prep(dpele_t* efd, va_list arg, dpasc_out_t* out)
{
    dpaio_arg_t* io = (dpaio_arg_t*)out->data;
    io->buf = va_arg(arg, void*);
    io->len = va_arg(arg, int);
    out->want_events = DPEVT_OUT;
    out->inva_events = 0;
    if (io->buf == NULL || io->len <= 0)
        return DPE_INVAL;
    return DPE_CONTINUE;
}

static dpret_t _dpssl_send_post(dpele_t* efd, dpasc_out_t* out)
{
    dpaio_arg_t* io = (dpaio_arg_t*)out->data;
    dpssl_ssn_udata_t* u = dpele_aux_data(efd);
    int total = 0;
    for (;;) {
        int ret = SSL_write(u->ssn, (const char*)io->buf + total, io->len - total);
        if (ret > 0) {
            total += ret;
            if (total == io->len)
                return total;
        } else {
            int ssl_err = SSL_get_error(u->ssn, ret);
            ret = _dpssl_post_err(ssl_err, out);
            return total > 0 ? total : ret;
        }
    }
}

DPASC_SSL_FUNCTION(send, _dpssl_send_prep, _dpssl_send_post, sizeof(dpaio_arg_t), 0)

#endif /* DPAPP_HAS_SSL */
