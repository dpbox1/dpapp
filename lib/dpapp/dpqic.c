#include "dpapp/dpqic.h"
#include "dpapp/dpasc.h"
#include "dpapp/dpbuf.h"
#include "dpapp/dpdef.h"
#include "dpapp/dpefd.h"
#include "dpapp/dpevp.h"
#include "dpapp/dplog.h"
#include "dpapp/dprbt.h"
#include "dpapp/dpret.h"
#include "dpapp/dpssl.h"
#include "dpapp/os/dpevp_pri.h"
#include "dpudp_pri.h"
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// ** 注意：确保空间足够容纳大包，这非常重要 !!!!!!!!!!! **
#define MTU_SIZE         2048
#define CTL_SIZE         256
#define MAX_PACKET_COUNT 64

#define _DPQIC_ALPNS_LENGTH 256

#if !DPAPP_LSQUIC_ENABLE
bool dpqic_enable()
{
    return false;
}

dpret_t dpqic_thrd_init()
{
    return DPE_UNSUPPORT;
}

void dpqic_thrd_exit()
{
    return;
}

#else // DPAPP_LSQUIC_ENABLE

#include "lsquic/lsquic.h"
#include "lsquic/lsquic_types.h"
#include "lsquic/lsxpack_header.h"
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/tls1.h>

bool dpqic_enable()
{
    return (dpssl_enable() && true);
}

// lsquic 流与连接回调
static lsquic_conn_ctx_t* _dpqic_cb_on_new_conn(void* ctx, lsquic_conn_t* conn);
static void _dpqic_cb_on_conn_closed(lsquic_conn_t* conn);
static lsquic_stream_ctx_t* _dpqic_cb_on_new_stream(void* ctx,
    lsquic_stream_t* stream);
static void _dpqic_cb_on_read(lsquic_stream_t* stream, lsquic_stream_ctx_t* st_h);
static void _dpqic_cb_on_write(lsquic_stream_t* stream, lsquic_stream_ctx_t* st_h);
static void _dpqic_cb_on_close(lsquic_stream_t* stream, lsquic_stream_ctx_t* st_h);
static void _dpqic_cb_on_hsk_done(lsquic_conn_t* conn,
    enum lsquic_hsk_status status);

// lsquic 发包回调
static int _dpqic_cb_send_packets(void* ctx, const struct lsquic_out_spec* specs,
    unsigned count);

// lsquic SSL 回调
static struct ssl_ctx_st* _dpqic_cb_get_ssl_ctx(void* peer_ctx,
    const struct sockaddr* unused);
static struct ssl_ctx_st* _dpqic_cb_lookup_cert(void* cert_lu_ctx,
    const struct sockaddr*, const char* sni);

// lsquic HTTP 头回调
void* _dpqic_cb_hsi_create(void* hsi_ctx, lsquic_stream_t* stream,
    int is_push_promise);
struct lsxpack_header* _dpqic_cb_hsi_predec(void* hdr_set,
    struct lsxpack_header* hdr, size_t space);
int _dpqic_cb_hsi_append(void* hdr_set, struct lsxpack_header* hdr);
void _dpqic_cb_hsi_delete(void* hdr_set);

// dpapp 事件循环回调
struct _dpqic_ioevent_data
{
    dpele_t* qele;
};

static dpret_t _dpqic_ioevent_prep(dpefd_t* efd, va_list arg, dpasc_out_t* out);
static dpret_t _dpqic_ioevent_post(dpefd_t* efd, dpasc_out_t* out);
DPASC_SKT_FUNCTION(qic_ioevent, _dpqic_ioevent_prep, _dpqic_ioevent_post,
    sizeof(struct _dpqic_ioevent_data), 0)

typedef struct dpqic_engine
{
    dprole_e role;
    uint32_t engine_flag;
    char group[32];
    struct dpqic_engine* next;

    // SLQUIC engine
    lsquic_engine_t* engine;
    struct lsquic_engine_settings engine_settings;
    struct lsquic_engine_api engine_api;
    struct lsquic_stream_if stream_if;
    struct lsquic_hset_if header_if;

    // 所有监听器或客户端计数
    uint64_t count;

    dptmr_t* timer;
} dpqic_engine_t;

struct dpqic_thrd_ctx
{
    // 引擎链表头，每个引擎有唯一的 group
    dpqic_engine_t* engine_head;
    char alpns[_DPQIC_ALPNS_LENGTH];

    // 各客户端/服务端 recvmmsg 读缓冲
    struct mmsghdr in_data[MAX_PACKET_COUNT];
    struct _dpqic_in_storage
    {
        struct iovec iov;
        struct sockaddr_storage peer_addr;
        struct sockaddr_storage local_addr;
        socklen_t peer_addr_len;
        uint8_t ctl_data[CTL_SIZE];
        uint8_t msg_data[MTU_SIZE];
        int ecn;
    } in_storage[MAX_PACKET_COUNT];

    // 各客户端/服务端 sendmmsg 写缓冲
    struct mmsghdr out_data[MAX_PACKET_COUNT];
    struct _dpqic_out_storage
    {
        uint8_t ctl_data[CTL_SIZE];
    } out_storage[MAX_PACKET_COUNT];
};

static __thread struct dpqic_thrd_ctx* _qic_thenv = NULL;
static pthread_mutex_t _qic_thmux = PTHREAD_MUTEX_INITIALIZER;
static int _qic_thcount = 0;

static int _dpqic_log_buf(void* logger_ctx, const char* buf, size_t len)
{
    char* lbeg = strchr(buf, '[');
    char* lend = strchr(lbeg, ']');
    char lname[16] = "";
    memcpy(lname, lbeg + 1, lend - lbeg - 1);

    lbeg = strstr(lend, "[QUIC:");
    if (lbeg) {
        lend = strchr(lbeg, ']');
    }

    dplog_level_e l = dplog_namel(lname);
    dplog_write(l, "dpqic", "%.*s", (int)(len - (lend - buf + 3)), lend + 2);
    return 0;
}

struct lsquic_logger_if qic_logger_if = {.log_buf = _dpqic_log_buf};

static void _dpqic_reset_in_storage(struct msghdr* msg,
    struct _dpqic_in_storage* item);

dpret_t dpqic_thrd_init()
{
    if (_qic_thenv == NULL) {
        _qic_thenv = calloc(1, sizeof(struct dpqic_thrd_ctx));
        if (_qic_thenv == NULL) {
            dplog_error("dpqic", "Failed to allocate thread context");
            return DPE_NOMEM;
        }

        for (int i = 0; i < MAX_PACKET_COUNT; i++) {
            _dpqic_reset_in_storage(&_qic_thenv->in_data[i].msg_hdr,
                &_qic_thenv->in_storage[i]);
        }
    } else {
        return DPE_OK;
    }

    // 仅初始化一次
    dpret_t ret = DPE_OK;
    pthread_mutex_lock(&_qic_thmux);
    if (_qic_thcount == 0) {
        if (lsquic_global_init(LSQUIC_GLOBAL_CLIENT | LSQUIC_GLOBAL_SERVER) != 0) {
            dplog_error("dpqic", "lsquic_global_init failed");
            ret = DPE_UNINIT;
            goto FINAL;
        }

        dplog_info("dpqic", "lsquic global environment initialized");

        lsquic_logger_init(&qic_logger_if, NULL, LLTS_NONE);
        lsquic_set_log_level(dplog_curlname());
    }
    _qic_thcount++;
FINAL:
    pthread_mutex_unlock(&_qic_thmux);
    return ret;
}

void dpqic_thrd_exit()
{
    if (_qic_thenv) {
        dpret_t ret = dpqic_del_engine(NULL);
        if (_qic_thenv->engine_head) {
            dplog_error("dpqic", "Free engine(%d), and some engines are not freed.",
                ret);
        } else {
            free(_qic_thenv);
            _qic_thenv = NULL;
        }
    } else {
        return;
    }

    pthread_mutex_lock(&_qic_thmux);
    _qic_thcount--;
    if (_qic_thcount == 0) {
        lsquic_global_cleanup();
    }
    pthread_mutex_unlock(&_qic_thmux);
}

static inline dpele_t* _dpqic_ready_next(dpele_t* ele)
{
    return (dpele_t*)(((void**)(dpele_aux_data(ele)))[0]);
}

static inline void _dpqic_set_ready_next(dpele_t* ele, dpele_t* next)
{
    ((void**)(dpele_aux_data(ele)))[0] = next;
}

static dpqic_engine_t* _dpqic_get_engine(const char* group,
    dpqic_engine_settings_t* settings_)
{
    if (group == NULL || group[0] == '\0') {
        dplog_error("dpqic", "engine group name is required");
        errno = DPE_INVAL;
        return NULL;
    }

    const char* alpn = dpssl_get_alpn(group, 0);
    if (alpn == NULL) {
        dplog_error("dpqic", "ssl group '%s' has no ALPN configured", group);
        errno = DPE_INVAL;
        return NULL;
    }

    dpret_t ver_ret = dpssl_has_version(group, TLS1_3_VERSION, TLS1_3_VERSION);
    if (ver_ret != DPE_OK) {
        dplog_error("dpqic", "ssl group '%s' must support TLS 1.3 (ret=%d)", group,
            ver_ret);
        errno = ver_ret == DPE_NOTEXISTS ? DPE_UNSUPPORT : ver_ret;
        return NULL;
    }

    dprole_e role = dpssl_role(group);
    struct dpqic_thrd_ctx* thctx = _qic_thenv;
    dpqic_engine_t* tail = NULL;
    for (dpqic_engine_t* curr = thctx->engine_head; curr != NULL;
         curr = curr->next) {
        if (strcmp(curr->group, group) == 0) {
            return curr;
        } else {
            tail = curr;
        }
    }

    int flag = (role == DPROLE_SERVER) ? LSENG_SERVER : 0;
    if (strcmp(alpn, "h3") == 0 || strncmp(alpn, "h3-", 3) == 0) {
        flag |= LSENG_HTTP;
    }

    if (settings_) {
        char err_buf[1024] = "";
        if (lsquic_engine_check_settings(settings_, flag, err_buf, 1024) != 0) {
            dplog_error("dpqic", "Failed to check settings: %s", err_buf);
            errno = DPE_INVAL;
            return NULL;
        }
    }

    dpqic_engine_t* engine = calloc(1, sizeof(dpqic_engine_t));
    if (engine == NULL) {
        dplog_error("dpqic", "Failed to allocate engine for group '%s'", group);
        errno = DPE_NOMEM;
        return NULL;
    }
    strncpy(engine->group, group, 31);
    engine->role = role;
    engine->timer = dpele_new(dptmr_init_type());
    if (engine->timer == NULL) {
        dplog_error("dpqic", "Failed to create timer for group '%s'", group);
        free(engine);
        errno = DPE_NOMEM;
        return NULL;
    }
    dpele_set_detach(engine->timer, true);

    struct lsquic_engine_settings* settings = &engine->engine_settings;
    if (settings_) {
        memcpy(settings, settings_, sizeof(dpqic_engine_settings_t));
    } else {
        lsquic_engine_init_settings(settings, flag);
#ifdef DPAPP_DEBUG
        if (role == DPROLE_CLIENT) {
            settings->es_idle_timeout = 120;           // 120s
            settings->es_handshake_to = 120 * 1000000; // 120s (in microseconds)
        }
#endif
    }

    settings->es_ecn = 1; // 强制启用 ECN

    struct lsquic_stream_if* stream_if = &engine->stream_if;
    stream_if->on_new_conn = _dpqic_cb_on_new_conn;
    stream_if->on_conn_closed = _dpqic_cb_on_conn_closed;
    stream_if->on_new_stream = _dpqic_cb_on_new_stream;
    stream_if->on_read = _dpqic_cb_on_read;
    stream_if->on_write = _dpqic_cb_on_write;
    stream_if->on_close = _dpqic_cb_on_close;
    stream_if->on_hsk_done = _dpqic_cb_on_hsk_done;

    struct lsquic_engine_api* engine_api = &engine->engine_api;
    engine_api->ea_settings = &engine->engine_settings;
    engine_api->ea_stream_if = stream_if;
    engine_api->ea_stream_if_ctx = engine;
    engine_api->ea_packets_out = _dpqic_cb_send_packets;
    engine_api->ea_packets_out_ctx = engine;
    engine_api->ea_get_ssl_ctx = _dpqic_cb_get_ssl_ctx;
    engine_api->ea_lookup_cert = _dpqic_cb_lookup_cert;
    engine_api->ea_cert_lu_ctx = engine;
    engine_api->ea_alpn = alpn;

    if (flag & LSENG_HTTP) {
        struct lsquic_hset_if* header_if = &engine->header_if;
        header_if->hsi_create_header_set = _dpqic_cb_hsi_create;
        header_if->hsi_prepare_decode = _dpqic_cb_hsi_predec;
        header_if->hsi_process_header = _dpqic_cb_hsi_append;
        header_if->hsi_discard_header_set = _dpqic_cb_hsi_delete;
        engine_api->ea_hsi_if = header_if;
        engine_api->ea_hsi_ctx = engine;
    }

    engine->engine = lsquic_engine_new(flag, engine_api);
    if (engine->engine == NULL) {
        dplog_error("dpqic", "Failed to create lsquic engine (role=%d, group=%s)",
            role, group);
        dpele_del(engine->timer);
        free(engine);
        errno = DPE_UNINIT;
        return NULL;
    }

    engine->engine_flag = flag;

    if (thctx->engine_head == NULL) {
        thctx->engine_head = engine;
    } else {
        tail->next = engine;
    }

    if (flag & LSENG_HTTP) {
        /* lsquic h3_alpns 以 NULL 结尾；LSQVER_I002 对应占位 ""，非 TLS ALPN */
        const char* const* alpns = lsquic_get_h3_alpns(settings->es_versions);
        for (; alpns && *alpns != NULL; ++alpns) {
            if ((*alpns)[0] == '\0')
                continue;
            dpret_t alpn_ret = dpssl_add_alpn(group, *alpns);
            if (dpret_iserr(alpn_ret)) {
                dplog_warn("dpqic", "Failed to add ALPN '%s': %d", *alpns, alpn_ret);
            }
        }
    }

    dplog_info("dpqic", "Engine ready: group='%s' role=%s alpn='%s' http=%d", group,
        role == DPROLE_SERVER ? "server" : "client", alpn, (flag & LSENG_HTTP) != 0);
    return engine;
}

dpret_t dpqic_add_engine(const char* group, dpqic_engine_settings_t* settings_)
{
    dpqic_engine_t* engine = _dpqic_get_engine(group, settings_);
    return engine ? DPE_OK : errno;
}

dpret_t dpqic_del_engine(const char* group)
{
    struct dpqic_thrd_ctx* thctx = _qic_thenv;
    dpqic_engine_t* prev = NULL;
    dpqic_engine_t* curr = thctx->engine_head;
    dpqic_engine_t* temp = NULL;
    dpret_t count = 0; // 已删除引擎数
    while (curr != NULL) {
        if (curr->count == 0 && (group == NULL || strcmp(curr->group, group) == 0)) {
            // 从链表移除
            if (prev == NULL) {
                thctx->engine_head = curr->next;
            } else {
                prev->next = curr->next;
            }

            lsquic_engine_destroy(curr->engine);
            temp = curr;
            curr = curr->next;
            free(temp);
            count++;
        } else {
            prev = curr;
            curr = curr->next;
        }
    }

    return count;
}

void _dpqic_cb_timeout(dptmr_t* tmr, dpv64_t arg);

void _dpqic_start_process(dpqic_engine_t* engine)
{
    static double zero_timeout = 0.0;
    dpevp_add(engine->timer, dptmr_callback(), zero_timeout, _dpqic_cb_timeout,
        engine);
}

void _dpqic_process_conns(dpqic_engine_t* engine)
{
    lsquic_engine_process_conns(engine->engine);

    int diff = 0;
    if (lsquic_engine_earliest_adv_tick(engine->engine, &diff)) {
        double after = diff < 0 ? 0.0 : (double)diff / 1000000.0;
        // 插入或更新定时器
        dpret_t ret = dpevp_add(engine->timer, dptmr_callback(), after,
            _dpqic_cb_timeout, engine);
        if (dpret_iserr(ret)) {
            dplog_error("dpqic", "Failed to run timer: %d", ret);
        }
    }
}

void _dpqic_cb_timeout(dptmr_t* tmr, dpv64_t arg)
{
    dpqic_engine_t* engine = (dpqic_engine_t*)arg.ptr;
    _dpqic_process_conns(engine);
}

typedef struct
{
#define _DPQIC_EFD_HEAD_FIELDS                                                      \
    dpefd_t* udp_efd;                                                               \
    dpqic_engine_t* engine;                                                         \
    bool writable;

    _DPQIC_EFD_HEAD_FIELDS
    dpele_t** out_conn;
    dpele_t* ready_conn_head;
    dpele_t* ready_conn_tail;
} _dpqic_listen_t;

static dpret_t _dpqic_listen_vopen(void* udata, va_list varg)
{
    _dpqic_listen_t* ulsn = (_dpqic_listen_t*)udata;

    const char* group = va_arg(varg, const char*);
    dpqic_engine_t* engine = _dpqic_get_engine(group, NULL);
    if (engine == NULL) {
        return errno;
    }

    ulsn->udp_efd = dpele_newv(dpudp_server_type(), varg);
    DP_CHECK_RETURN(ulsn->udp_efd == NULL, DPE_INVAL);

    dpret_t ret = dpqic_server_setopt(ulsn->udp_efd);
    if (dpret_iserr(ret)) {
        dpele_del(ulsn->udp_efd);
        return ret;
    }

    ret = dpefd_set_block(ulsn->udp_efd, false);
    if (dpret_iserr(ret)) {
        dpele_del(ulsn->udp_efd);
        return ret;
    }

    dpele_set_detach(ulsn->udp_efd, true);
    ret = dpevp_add(ulsn->udp_efd, dpskt_qic_ioevent(),
        DPV64_PTR(dpele_get_by_uptr(udata)));
    if (dpret_iserr(ret)) {
        dpele_del(ulsn->udp_efd);
        return ret;
    }

    ulsn->engine = engine;
    engine->count++;
    return DPE_OK;
}

static void _dpqic_listen_clean(void* udata)
{
    _dpqic_listen_t* ulsn = (_dpqic_listen_t*)udata;

    if (ulsn->engine)
        ulsn->engine->count--;

    dpele_t* conn = ulsn->ready_conn_head;
    dpele_t* tmp = NULL;
    while (conn) {
        tmp = _dpqic_ready_next(conn);
        dpele_del(conn);
        conn = tmp;
    }

    dpele_del(ulsn->udp_efd);
}

const dpele_type_t* dpqic_listen_type()
{
    static dpele_type_t type = {
        .name = "qic_listen",
        .type = DPELE_TYPE_USD,
        .size = sizeof(_dpqic_listen_t),
        .iotype = DPAIO_TYPE_QIC,
        .init = _dpqic_listen_vopen,
        .fini = _dpqic_listen_clean,
    };
    return &type;
}

typedef struct
{
    _DPQIC_EFD_HEAD_FIELDS
} _dpqic_client_t;

static dpret_t _dpqic_gen_token(_dpqic_client_t* ucet, const char* token_str,
    uint8_t** out)
{
    static const unsigned char c2b[0x100] = {
        [(int)'0'] = 0,
        [(int)'1'] = 1,
        [(int)'2'] = 2,
        [(int)'3'] = 3,
        [(int)'4'] = 4,
        [(int)'5'] = 5,
        [(int)'6'] = 6,
        [(int)'7'] = 7,
        [(int)'8'] = 8,
        [(int)'9'] = 9,
        [(int)'A'] = 0xA,
        [(int)'B'] = 0xB,
        [(int)'C'] = 0xC,
        [(int)'D'] = 0xD,
        [(int)'E'] = 0xE,
        [(int)'F'] = 0xF,
        [(int)'a'] = 0xA,
        [(int)'b'] = 0xB,
        [(int)'c'] = 0xC,
        [(int)'d'] = 0xD,
        [(int)'e'] = 0xE,
        [(int)'f'] = 0xF,
    };

    unsigned char* token;
    int len, i;

    len = strlen(token_str);
    token = malloc(len / 2);
    if (!token) {
        return DPE_NOMEM;
    }

    for (i = 0; i < len / 2; ++i) {
        token[i] = (c2b[(int)token_str[i * 2]] << 4)
            | c2b[(int)token_str[i * 2 + 1]];
    }

    *out = token;
    return len / 2;
}

static dpret_t _dpqic_client_vopen(void* udata, va_list varg)
{
    _dpqic_client_t* ucet = (_dpqic_client_t*)udata;

    const char* group = va_arg(varg, const char*);
    dpqic_engine_t* engine = _dpqic_get_engine(group, NULL);
    if (engine == NULL) {
        return DPE_INVAL;
    }

    ucet->udp_efd = dpele_newv(dpudp_client_type(), varg);
    DP_CHECK_RETURN(ucet->udp_efd == NULL, DPE_INVAL);

    dpret_t ret = dpqic_client_setopt(ucet->udp_efd);
    if (dpret_iserr(ret)) {
        dpele_del(ucet->udp_efd);
        return ret;
    }

    ret = dpefd_set_block(ucet->udp_efd, false);
    if (dpret_iserr(ret)) {
        dpele_del(ucet->udp_efd);
        return ret;
    }

    dpele_set_detach(ucet->udp_efd, true);
    ret = dpevp_add(ucet->udp_efd, dpskt_qic_ioevent(),
        DPV64_PTR(dpele_get_by_uptr(udata)));

    if (dpret_iserr(ret)) {
        dpele_del(ucet->udp_efd);
        return ret;
    }

    ucet->engine = engine;
    engine->count++;
    return ret;
}

static void _dpqic_client_clean(void* udata)
{
    _dpqic_client_t* ucet = (_dpqic_client_t*)udata;
    ucet->engine->count--;
    dpele_del(ucet->udp_efd);
}

const dpele_type_t* dpqic_client_type()
{
    static dpele_type_t type = {
        .name = "qic_client",
        .type = DPELE_TYPE_USD,
        .size = sizeof(_dpqic_client_t),
        .iotype = DPAIO_TYPE_QIC,
        .init = _dpqic_client_vopen,
        .fini = _dpqic_client_clean,
    };
    return &type;
}

typedef struct
{
    dpele_t* ready_next;
    lsquic_conn_t* conn;
    dpele_t** out_stm;
    dpele_t* ready_stm_head;
    dpele_t* ready_stm_tail;
} _dpqic_conect_t;

static inline dpqic_engine_t* _dpqic_conect_engine(lsquic_conn_t* conn)
{
    return ((_dpqic_listen_t*)dpele_aux_data(lsquic_conn_get_peer_ctx(conn, NULL)))
        ->engine;
}

static dpret_t _dpqic_conect_vinit(void* udata, va_list vlist)
{
    _dpqic_conect_t* uconn = (_dpqic_conect_t*)udata;
    uconn->conn = va_arg(vlist, lsquic_conn_t*);
    return DPE_OK;
}

static void _dpqic_conect_clean(void* udata)
{
    _dpqic_conect_t* uconn = (_dpqic_conect_t*)udata;
    if (uconn->conn) {
        lsquic_conn_set_ctx(uconn->conn, NULL);
        lsquic_conn_close(uconn->conn);
        uconn->conn = NULL;
    }

    dpele_t* ele = uconn->ready_stm_head;
    dpele_t* tmp = NULL;
    while (ele) {
        tmp = _dpqic_ready_next(ele);
        dpele_del(ele);
        ele = tmp;
    }
}

const dpele_type_t* dpqic_conect_type()
{
    static dpele_type_t type = {
        .name = "qic_conect",
        .type = DPELE_TYPE_USD,
        .size = sizeof(_dpqic_conect_t),
        .iotype = DPAIO_TYPE_QIC,
        .init = _dpqic_conect_vinit,
        .fini = _dpqic_conect_clean,
    };
    return &type;
}

typedef struct _dpqic_stream
{
    dpele_t* ready_next;
    lsquic_stream_t* stream;
    dpele_t* avatar;
    uint8_t is_owner : 1; // 是否为流的拥有者，默认 1
} _dpqic_stream_t;

static dpret_t _dpqic_stream_vinit(void* udata, va_list vlist)
{
    _dpqic_stream_t* ustm = (_dpqic_stream_t*)udata;
    ustm->stream = va_arg(vlist, lsquic_stream_t*);
    ustm->is_owner = 1;
    return DPE_OK;
}

static dpret_t _dpqic_stream_copy(void* dst, const void* src)
{
    // 允许同一流上并发的待定读写操作
    _dpqic_stream_t* usrcstm = (_dpqic_stream_t*)src;
    _dpqic_stream_t* udststm = (_dpqic_stream_t*)dst;

    if (usrcstm->avatar) {
        return DPE_PERM;
    }

    usrcstm->avatar = dpele_get_by_uptr(dst);
    udststm->avatar = dpele_get_by_uptr((void*)src);
    udststm->stream = usrcstm->stream;
    udststm->is_owner = 0;
    return DPE_OK;
}

static void _dpqic_stream_clean(void* udata)
{
    _dpqic_stream_t* ustm = (_dpqic_stream_t*)udata;

    if (ustm->avatar) {
        _dpqic_stream_t* uavatar = (_dpqic_stream_t*)dpele_aux_data(ustm->avatar);
        uavatar->is_owner = 1;
        uavatar->avatar = NULL;
    } else {
        if (ustm->stream) {
            lsquic_stream_set_ctx(ustm->stream, NULL);
            lsquic_stream_close(ustm->stream);
        }
    }
}

const dpele_type_t* dpqic_stream_type()
{
    static dpele_type_t _dpqic_stream_type = {
        .name = "qic_stream",
        .type = DPELE_TYPE_USD,
        .size = sizeof(_dpqic_stream_t),
        .iotype = DPAIO_TYPE_QIC,
        .init = _dpqic_stream_vinit,
        .copy = _dpqic_stream_copy,
        .fini = _dpqic_stream_clean,
    };
    return &_dpqic_stream_type;
}

static dpret_t _dpqic_connect_prep(dpele_t* ele, va_list arg, dpasc_out_t* out)
{
    const char* sni_ = va_arg(arg, const char*);
    const char* token_ = va_arg(arg, const char*);
    dpele_t** conn = va_arg(arg, dpele_t**);

    *conn = NULL;
    _dpqic_client_t* ucet = (_dpqic_client_t*)dpele_aux_data(ele);
    dpudp_client_t* uudp = (dpudp_client_t*)dpele_aux_data(ucet->udp_efd);
    dpqic_engine_t* engine = ucet->engine;

    dpret_t ret = DPE_OK;
    uint8_t* token_out = NULL;
    size_t token_len = 0;
    if (token_) {
        ret = _dpqic_gen_token(ucet, token_, &token_out);
        if (ret <= 0) {
            dpele_del(ucet->udp_efd);
            return ret;
        }
        token_len = ret;
    }

    lsquic_conn_t* qconn = lsquic_engine_connect(engine->engine, N_LSQVER,
        (struct sockaddr*)&uudp->local_addr.addr,
        (struct sockaddr*)&uudp->peer_addr.addr, ele, NULL, sni_, 0, NULL, 0,
        token_out, token_len);
    if (qconn == NULL) {
        free(token_out);
        return DPE_INVAL;
    }
    free(token_out);

    *conn = (dpele_t*)lsquic_conn_get_ctx(qconn);
    _dpqic_start_process(engine);
    return DPE_CONTINUE;
}

DPASC_QIC_FUNCTION(connect, _dpqic_connect_prep, NULL, 0, 0)

static dpret_t _dpqic_stream_prep(dpele_t* ele, va_list arg, dpasc_out_t* out)
{
    dpele_t** stm = va_arg(arg, dpele_t**);
    bool new_ = (bool)va_arg(arg, int);

    _dpqic_conect_t* uconn = (_dpqic_conect_t*)dpele_aux_data(ele);
    if (uconn->conn == NULL)
        return DPE_PERM;

    *stm = NULL;
    if (new_) {
        uconn->out_stm = stm;
        /*lsquic_conn_make_stream实际为同步调用（_dpqic_cb_on_new_stream）。
          在设置了*out_stm后，out_stm置NULL，然后执行dpevp_end。
          根据out_stm判断，如果返回DPE_OK，引擎会再入队（执行失败，因为已经入队,但不受影响）。*/
        lsquic_conn_make_stream(uconn->conn);
        return uconn->out_stm == NULL ? DPE_OK : DPE_CONTINUE;
    } else {
        if (uconn->ready_stm_head == NULL) {
            uconn->out_stm = stm;
            return DPE_CONTINUE;
        } else {
            *stm = uconn->ready_stm_head;
            uconn->ready_stm_head = _dpqic_ready_next(uconn->ready_stm_head);
            if (uconn->ready_stm_head == NULL)
                uconn->ready_stm_tail = NULL;
            return DPE_OK;
        }
    }
}

DPASC_QIC_FUNCTION(stream, _dpqic_stream_prep, NULL, 0, 0)

static dpret_t _dpqic_accept_prep(dpele_t* ele, va_list arg, dpasc_out_t* out)
{
    dpele_t** conn = va_arg(arg, dpele_t**);

    _dpqic_listen_t* lsnctx = (_dpqic_listen_t*)dpele_aux_data(ele);
    if (lsnctx->ready_conn_head == NULL) {
        lsnctx->out_conn = conn;
        return DPE_CONTINUE;
    } else {
        *conn = lsnctx->ready_conn_head;
        lsnctx->ready_conn_head = _dpqic_ready_next(lsnctx->ready_conn_head);
        if (lsnctx->ready_conn_head == NULL)
            lsnctx->ready_conn_tail = NULL;
        return DPE_OK;
    }
}

DPASC_QIC_FUNCTION(accept, _dpqic_accept_prep, NULL, 0, 0)

#define DPQIC_PREP_CHECK_RECV_STREAM(ele__)                                         \
    _dpqic_stream_t* ustm = (_dpqic_stream_t*)dpele_aux_data(ele);                     \
    if (ustm->stream == NULL)                                                       \
        return DPE_CLOSED;                                                          \
    if (ustm->is_owner != 1)                                                        \
        return DPE_PERM;

#define DPQIC_PREP_CHECK_SEND_STREAM(ele__)                                         \
    _dpqic_stream_t* ustm = (_dpqic_stream_t*)dpele_aux_data(ele);                     \
    if (ustm->stream == NULL)                                                       \
        return DPE_CLOSED;                                                          \
    if (ustm->is_owner == 1 && ustm->avatar != NULL)                                \
        return DPE_PERM;

static dpret_t _dpqic_recv_prep(dpele_t* ele, va_list arg, dpasc_out_t* out)
{
    dpaio_arg_t* io = (dpaio_arg_t*)out->data;
    io->buf = va_arg(arg, void*);
    io->len = va_arg(arg, int);

    if (io->buf == NULL || io->len <= 0)
        return DPE_INVAL;

    DPQIC_PREP_CHECK_RECV_STREAM(ele)

    lsquic_stream_wantread(ustm->stream, 1);
    _dpqic_start_process(_dpqic_conect_engine(lsquic_stream_conn(ustm->stream)));
    return DPE_CONTINUE;
}

static dpret_t _dpqic_recv_post(dpele_t* ele, dpasc_out_t* out)
{
    dpaio_arg_t* io = (dpaio_arg_t*)out->data;
    _dpqic_stream_t* ustm = (_dpqic_stream_t*)dpele_aux_data(ele);
    ssize_t ret = lsquic_stream_read(ustm->stream, io->buf, io->len);
    if (ret < 0)
        return -errno;
    if (ret == 0)
        return DPE_EOF;
    return ret;
}

DPASC_QIC_FUNCTION(recv, _dpqic_recv_prep, _dpqic_recv_post, sizeof(dpaio_arg_t), 0)

static dpret_t _dpqic_send_prep(dpele_t* ele, va_list arg, dpasc_out_t* out)
{
    dpaio_arg_t* io = (dpaio_arg_t*)out->data;
    io->buf = (void*)va_arg(arg, const void*);
    io->len = va_arg(arg, int);

    if (io->buf == NULL || io->len <= 0)
        return DPE_INVAL;

    DPQIC_PREP_CHECK_SEND_STREAM(ele)

    lsquic_stream_wantwrite(ustm->stream, 1);
    _dpqic_start_process(_dpqic_conect_engine(lsquic_stream_conn(ustm->stream)));
    return DPE_CONTINUE;
}

static dpret_t _dpqic_send_post(dpele_t* ele, dpasc_out_t* out)
{
    dpaio_arg_t* io = (dpaio_arg_t*)out->data;
    _dpqic_stream_t* ustm = (_dpqic_stream_t*)dpele_aux_data(ele);
    ssize_t ret = lsquic_stream_write(ustm->stream, io->buf, io->len);
    if (ret > 0) {
        lsquic_stream_flush(ustm->stream);
        return ret;
    }
    if (ret == 0)
        return DPE_WAIT;
    return DPE_BADFD;
}

DPASC_QIC_FUNCTION(send, _dpqic_send_prep, _dpqic_send_post, sizeof(dpaio_arg_t), 0)

static dpret_t _dpqic_recvv_prep(dpele_t* ele, va_list arg, dpasc_out_t* out)
{
    dpaio_arg_t* io = (dpaio_arg_t*)out->data;
    io->iov = (struct iovec*)va_arg(arg, const struct iovec*);
    io->len = va_arg(arg, int);

    if (io->iov == NULL || io->len <= 0)
        return DPE_INVAL;

    DPQIC_PREP_CHECK_RECV_STREAM(ele)

    lsquic_stream_wantread(ustm->stream, 1);
    _dpqic_start_process(_dpqic_conect_engine(lsquic_stream_conn(ustm->stream)));
    return DPE_CONTINUE;
}

static dpret_t _dpqic_recvv_post(dpele_t* ele, dpasc_out_t* out)
{
    dpaio_arg_t* io = (dpaio_arg_t*)out->data;
    _dpqic_stream_t* ustm = (_dpqic_stream_t*)dpele_aux_data(ele);
    ssize_t ret = lsquic_stream_readv(ustm->stream, io->iov, io->len);
    if (ret < 0)
        return -errno;
    if (ret == 0)
        return DPE_EOF;
    return ret;
}

DPASC_QIC_FUNCTION(recvv, _dpqic_recvv_prep, _dpqic_recvv_post, sizeof(dpaio_arg_t),
    0)

static dpret_t _dpqic_sendv_prep(dpele_t* ele, va_list arg, dpasc_out_t* out)
{
    dpaio_arg_t* io = (dpaio_arg_t*)out->data;
    io->iov = (struct iovec*)va_arg(arg, const struct iovec*);
    io->len = va_arg(arg, int);

    if (io->iov == NULL || io->len <= 0)
        return DPE_INVAL;

    DPQIC_PREP_CHECK_SEND_STREAM(ele)

    lsquic_stream_wantwrite(ustm->stream, 1);
    _dpqic_start_process(_dpqic_conect_engine(lsquic_stream_conn(ustm->stream)));
    return DPE_CONTINUE;
}

static dpret_t _dpqic_sendv_post(dpele_t* ele, dpasc_out_t* out)
{
    dpaio_arg_t* io = (dpaio_arg_t*)out->data;
    _dpqic_stream_t* ustm = (_dpqic_stream_t*)dpele_aux_data(ele);
    ssize_t ret = lsquic_stream_writev(ustm->stream, io->iov, io->len);
    if (ret > 0) {
        lsquic_stream_flush(ustm->stream);
        return ret;
    }
    if (ret == 0)
        return DPE_WAIT;
    return DPE_BADFD;
}

DPASC_QIC_FUNCTION(sendv, _dpqic_sendv_prep, _dpqic_sendv_post, sizeof(dpaio_arg_t),
    0)

/* 使用向量结构存储所有头部，不使用 hash 等快速查找结构。
 * 这样 lsquic 读写头部时无需重建 lsquic_http_headers 结构。 */
struct dpqic_hdrset
{
    struct lsquic_http_headers headers;
    int real_count;
};

static dpret_t _dpqic_recv_hdrset_prep(dpele_t* ele, va_list arg, dpasc_out_t* out)
{
    dpaio_arg_t* io = (dpaio_arg_t*)out->data;
    io->ptr = (char*)va_arg(arg, dpqic_hdrset_t**);

    if (io->ptr == NULL)
        return DPE_INVAL;

    DPQIC_PREP_CHECK_RECV_STREAM(ele)

    dpqic_engine_t* engine = _dpqic_conect_engine(lsquic_stream_conn(ustm->stream));
    if ((engine->engine_flag & LSENG_HTTP) == 0)
        return DPE_PERM;

    lsquic_stream_wantread(ustm->stream, 1);
    _dpqic_start_process(engine);
    return DPE_CONTINUE;
}

static dpret_t _dpqic_recv_hdrset_post(dpele_t* ele, dpasc_out_t* out)
{
    dpaio_arg_t* io = (dpaio_arg_t*)out->data;
    dpqic_hdrset_t* hdrset = lsquic_stream_get_hset(
        ((_dpqic_stream_t*)dpele_aux_data(ele))->stream);
    if (hdrset) {
        *(dpqic_hdrset_t**)io->ptr = hdrset;
        return DPE_OK;
    }
    return -errno;
}

DPASC_QIC_FUNCTION(recv_hdrset, _dpqic_recv_hdrset_prep, _dpqic_recv_hdrset_post,
    sizeof(dpaio_arg_t), 0)

static dpret_t _dpqic_send_hdrset_prep(dpele_t* ele, va_list arg, dpasc_out_t* out)
{
    dpaio_arg_t* io = (dpaio_arg_t*)out->data;
    io->ptr = (void*)va_arg(arg, const dpqic_hdrset_t*);

    if (io->ptr == NULL)
        return DPE_INVAL;

    DPQIC_PREP_CHECK_SEND_STREAM(ele)

    dpqic_engine_t* engine = _dpqic_conect_engine(lsquic_stream_conn(ustm->stream));
    if ((engine->engine_flag & LSENG_HTTP) == 0)
        return DPE_PERM;

    lsquic_stream_wantwrite(ustm->stream, 1);
    _dpqic_start_process(engine);
    return DPE_CONTINUE;
}

static dpret_t _dpqic_send_hdrset_post(dpele_t* ele, dpasc_out_t* out)
{
    dpaio_arg_t* io = (dpaio_arg_t*)out->data;
    int r = lsquic_stream_send_headers(((_dpqic_stream_t*)dpele_aux_data(ele))->stream,
        &((const dpqic_hdrset_t*)io->ptr)->headers, 0);
    return r == 0 ? DPE_OK : -errno;
}

DPASC_QIC_FUNCTION(send_hdrset, _dpqic_send_hdrset_prep, _dpqic_send_hdrset_post,
    sizeof(dpaio_arg_t), 0)

// 所有回调函数
void _dpqic_reset_in_storage(struct msghdr* msg, struct _dpqic_in_storage* item)
{
    item->iov.iov_base = item->msg_data;
    item->iov.iov_len = MTU_SIZE;
    item->ecn = 0;

    msg->msg_iov = &item->iov;
    msg->msg_iovlen = 1;
    msg->msg_name = &item->peer_addr;
    msg->msg_control = item->ctl_data;
    msg->msg_namelen = sizeof(struct sockaddr_storage);
    msg->msg_controllen = CTL_SIZE;
    msg->msg_flags = 0;
}

static dpret_t _dpqic_cb_recv_packets(dpele_t* qele)
{
    struct dpqic_thrd_ctx* thctx = _qic_thenv;

    // _dpqic_client_t 与 _dpqic_listen_t 头部相同
    _dpqic_client_t* ucet = (_dpqic_client_t*)dpele_aux_data(qele);
    dpefd_t* udp_efd = ucet->udp_efd;

    // dpudp_server_t 与 dpudp_client_t 的 local_addr 头部相同
    dpudp_server_t* uudp = (dpudp_server_t*)dpele_aux_data(udp_efd);

    dpqic_engine_t* engine = ucet->engine;

    struct mmsghdr* in_data = thctx->in_data;
    struct _dpqic_in_storage* in_storage = thctx->in_storage;
    dpret_t ret = DPE_WAIT;
    int count = 0;
    uint32_t dropped = 0;
    struct _dpqic_in_storage* item = NULL;

    for (;;) {
        int s = recvmmsg(dpefd_fd(udp_efd), in_data, MAX_PACKET_COUNT, 0, NULL);
        if (s < 0) {
            ret = -errno;
            break;
        } else if (s == 0) {
            ret = DPE_WAIT;
            break;
        }

        for (int n = 0; n < s; n++) {
            item = &in_storage[n];

            memcpy(&item->local_addr, &uudp->local_addr.addr,
                sizeof(item->local_addr));
            _parse_contorl_msg(&in_data[n].msg_hdr, &item->local_addr, &dropped,
                &item->ecn);

            if (lsquic_engine_packet_in(engine->engine, item->msg_data,
                    in_data[n].msg_len, (struct sockaddr*)&item->local_addr,
                    (struct sockaddr*)&item->peer_addr, qele, item->ecn)
                < 0) {
                dplog_warn("dpqic", "packet_in failed after %d packets (dropped=%u)",
                    count, dropped);
                ret = DPE_UNKNOWN;
                _dpqic_reset_in_storage(&in_data[n].msg_hdr, item);
                break;
            }

            count++;
            _dpqic_reset_in_storage(&in_data[n].msg_hdr, item);
        }
    }

    if (count) {
        _dpqic_process_conns(engine);
    }

    return ret;
}

int _dpqic_cb_send_packets(void* ctx, const struct lsquic_out_spec* specs,
    unsigned count)
{
    struct dpqic_thrd_ctx* thctx = _qic_thenv;
    dpqic_engine_t* engine = (dpqic_engine_t*)ctx;
    struct mmsghdr* out_data = thctx->out_data;
    struct _dpqic_out_storage* out_storage = thctx->out_storage;
    struct mmsghdr* mmsg = NULL;
    dpele_t* ele = NULL;
    dpefd_t* udp_efd = NULL;
    int n = 0, m = 0;
    const struct lsquic_out_spec* spec = NULL;
    enum ctl_what cw = 0;
    uintptr_t ancil_key = 0, prev_ancil_key = 0;

    while (n < count) {
        spec = specs + n;
        ele = spec->peer_ctx;
        udp_efd = ((_dpqic_client_t*)dpele_aux_data(ele))->udp_efd;
        prev_ancil_key = 0;

        for (m = 0; n < count && m < MAX_PACKET_COUNT && ele == spec->peer_ctx;
             n++, m++, spec++) {
            mmsg = out_data + m;
            mmsg->msg_len = 0;
            mmsg->msg_hdr.msg_name = (void*)spec->dest_sa;
            mmsg->msg_hdr.msg_namelen = (AF_INET == spec->dest_sa->sa_family
                    ? sizeof(struct sockaddr_in)
                    : sizeof(struct sockaddr_in6));
            mmsg->msg_hdr.msg_iov = spec->iov;
            mmsg->msg_hdr.msg_iovlen = spec->iovlen;
            mmsg->msg_hdr.msg_flags = 0;

            if ((engine->role == DPROLE_SERVER) && spec->local_sa->sa_family) {
                cw = CW_SENDADDR;
                ancil_key = (uintptr_t)spec->local_sa;
            } else {
                cw = 0;
                ancil_key = 0;
            }

            if (spec->ecn) {
                cw |= CW_ECN;
                ancil_key |= spec->ecn;
            }

            if (cw && prev_ancil_key == ancil_key && m > 0) {
                /* 复用前一个辅助消息避免 memset 开销 */
                mmsg->msg_hdr.msg_control = out_data[m - 1].msg_hdr.msg_control;
                mmsg->msg_hdr.msg_controllen = out_data[m - 1]
                                                   .msg_hdr.msg_controllen;
            } else if (cw) {
                prev_ancil_key = ancil_key;
                mmsg->msg_hdr.msg_control = out_storage[m].ctl_data;
                mmsg->msg_hdr.msg_controllen = CTL_SIZE;

                /* 因 CMSG_NXTHDR 的一个疑似 bug 需要清零缓冲区，参见
                 * https://stackoverflow.com/questions/27601849/cmsg-nxthdr-returns-null-even-though-there-are-more-cmsghdr-objects
                 */
                memset(mmsg->msg_hdr.msg_control, 0, CTL_SIZE);
                _setup_control_msg(&mmsg->msg_hdr, cw,
                    (struct sockaddr*)spec->local_sa,
                    (struct sockaddr*)spec->dest_sa, spec->ecn);
            } else {
                prev_ancil_key = 0;
                mmsg->msg_hdr.msg_control = NULL;
                mmsg->msg_hdr.msg_controllen = 0;
            }
        }

        int s = sendmmsg(dpefd_fd(udp_efd), out_data, m, 0);
        if (s >= 0) {
            if (s == m) {
                continue;
            } else {
                n -= (m - s);
                errno = EAGAIN;
                break;
            }
        } else {
            n -= m;
            break;
        }
    }

    return n;
}

dpret_t _dpqic_ioevent_prep(dpefd_t* efd, va_list arg, dpasc_out_t* out)
{
    out->want_events = DPEVT_AIO;
    struct _dpqic_ioevent_data* data = (struct _dpqic_ioevent_data*)out->data;
    data->qele = va_arg(arg, dpele_t*);
    return DPE_CONTINUE;
}

dpret_t _dpqic_ioevent_post(dpefd_t* efd, dpasc_out_t* out)
{
    int evs = out->able_events;
    dpele_t* qele = ((struct _dpqic_ioevent_data*)out->data)->qele;

    dpret_t ret = DPE_WAIT;
    if (evs & DPEVT_IN) {
        ret = _dpqic_cb_recv_packets(qele);
        out->inva_events |= DPEVT_IN;
    }

    _dpqic_client_t* ucet = (_dpqic_client_t*)dpele_aux_data(qele);
    ucet->writable = ((evs & DPEVT_OUT) > 0 || ucet->writable);

    if (ucet->writable) {
        lsquic_engine_t* engine = ucet->engine->engine;
        if (lsquic_engine_has_unsent_packets(engine)) {
            lsquic_engine_send_unsent_packets(engine);
        }
        ucet->writable = !lsquic_engine_has_unsent_packets(engine);
        out->inva_events |= DPEVT_OUT;
    }

    if (dpele_is_doing(qele)) {
        if (dpret_iserr(ret) && ret != DPE_WAIT)
            dpevp_end(qele, ret);
    }

    return ret;
}

lsquic_conn_ctx_t* _dpqic_cb_on_new_conn(void* ctx, lsquic_conn_t* conn)
{
    dpele_t* econn = dpele_new(dpqic_conect_type(), conn);
    if (econn == NULL) {
        dplog_error("dpqic", "dpele_new failed(%d) for connection %p, closing",
            errno, (void*)conn);
        lsquic_conn_close(conn);
        return NULL;
    }

    if (((dpqic_engine_t*)ctx)->role == DPROLE_CLIENT) {
        return (lsquic_conn_ctx_t*)econn;
    }

    dpele_t* efd = (dpele_t*)lsquic_conn_get_peer_ctx(conn, NULL);
    if (efd == NULL) {
        lsquic_conn_close(conn);
        return NULL;
    }

    _dpqic_listen_t* ulsn = (_dpqic_listen_t*)dpele_aux_data(efd);
    if (ulsn->out_conn) {
        *ulsn->out_conn = econn;
        ulsn->out_conn = NULL;
        dpevp_end(efd, DPE_OK);
    } else {
        if (ulsn->ready_conn_head) {
            _dpqic_set_ready_next(ulsn->ready_conn_tail, econn);
            ulsn->ready_conn_tail = econn;
        } else {
            // 将新建的连接元素入队
            ulsn->ready_conn_head = ulsn->ready_conn_tail = econn;
        }
    }
    return (lsquic_conn_ctx_t*)econn;
}

void _dpqic_cb_on_hsk_done(lsquic_conn_t* conn, enum lsquic_hsk_status status)
{
    // 客户端握手完成回调
    dpele_t* efd = (dpele_t*)lsquic_conn_get_peer_ctx(conn, NULL);

    switch (status) {
    case LSQ_HSK_FAIL: {
        dpevp_end(efd, DPE_CONNREFUSED);
        break;
    }
    case LSQ_HSK_OK: {
        dpevp_end(efd, DPE_OK);
        break;
    }
    default: {
        break;
    }
    }
}

void _dpqic_cb_on_conn_closed(lsquic_conn_t* conn)
{
    dpele_t* econn = (dpele_t*)lsquic_conn_get_ctx(conn);
    if (econn == NULL) {
        return;
    }

    _dpqic_conect_t* uconn = (_dpqic_conect_t*)dpele_aux_data(econn);
    uconn->out_stm = NULL;
    uconn->conn = NULL;
    if (dpele_is_doing(econn)) {
        dpevp_end(econn, DPE_CLOSED);
    }
    lsquic_conn_set_ctx(conn, NULL);
}

lsquic_stream_ctx_t* _dpqic_cb_on_new_stream(void* ctx, lsquic_stream_t* stream)
{
    if (lsquic_stream_is_pushed(stream)) {
        lsquic_stream_refuse_push(stream); // 暂不支持 push 流
        return NULL;
    }

    dpele_t* estm = dpele_new(dpqic_stream_type(), stream);
    if (estm == NULL) {
        dplog_error("dpqic", "dpele_new failed(%d) for stream %p, closing", errno,
            (void*)stream);
        lsquic_stream_close(stream);
        return NULL;
    }

    lsquic_conn_t* conn = lsquic_stream_conn(stream);
    dpele_t* econn = (dpele_t*)lsquic_conn_get_ctx(conn);
    _dpqic_conect_t* uconn = (_dpqic_conect_t*)dpele_aux_data(econn);

    if (uconn->out_stm) {
        *uconn->out_stm = estm;
        uconn->out_stm = NULL;
        dpevp_end(econn, DPE_OK);
    } else {
        if (uconn->ready_stm_head) {
            _dpqic_set_ready_next(uconn->ready_stm_tail, estm);
            uconn->ready_stm_tail = estm;
        } else {
            uconn->ready_stm_head = uconn->ready_stm_tail = estm;
        }
    }
    return (lsquic_stream_ctx_t*)estm;
}

void _dpqic_cb_on_read(lsquic_stream_t* stream, lsquic_stream_ctx_t* st_h)
{
    dpele_t* estm = (dpele_t*)st_h;
    const dpasc_t* asc_tp = _dpele_asc_type(estm);

    if (asc_tp) {
        dpasc_out_t out = {.data = dpele_asc_data(estm)};
        dpret_t ret = asc_tp->post(estm, &out);
        if (ret == DPE_WAIT) {
            lsquic_stream_wantread(stream, 1);
        } else {
            lsquic_stream_wantread(stream, 0);
            dpevp_end(estm, ret);
        }
    } else {
        lsquic_stream_wantread(stream, 0);
    }
}

void _dpqic_cb_on_write(lsquic_stream_t* stream, lsquic_stream_ctx_t* st_h)
{
    dpele_t* estm = (dpele_t*)st_h;
    const dpasc_t* asc_tp = _dpele_asc_type(estm);
    if (asc_tp) {
        dpasc_out_t out = {.data = dpele_asc_data(estm)};
        dpret_t ret = asc_tp->post(estm, &out);
        if (ret == DPE_WAIT) {
            lsquic_stream_wantwrite(stream, 1);
        } else {
            lsquic_stream_wantwrite(stream, 0);
            dpevp_end(estm, ret);
        }
    } else {
        lsquic_stream_wantwrite(stream, 0);
    }
}

void _dpqic_cb_on_close(lsquic_stream_t* stream, lsquic_stream_ctx_t* st_h)
{
    dpele_t* estm = (dpele_t*)st_h;
    if (estm == NULL) {
        /* ctx (dpele) 可能已被删除。_dpqic_stream_clean 中
         流的 ctx 会被设为 NULL。 */
        return;
    }

    _dpqic_stream_t* ustm = (_dpqic_stream_t*)dpele_aux_data(estm);
    ustm->stream = NULL;

    if (dpele_is_doing(estm)) {
        dpevp_end(estm, DPE_CLOSED);
    }

    if (ustm->avatar) {
        _dpqic_stream_t* uavatar = (_dpqic_stream_t*)dpele_aux_data(ustm->avatar);
        uavatar->stream = NULL;
        if (dpele_is_doing(ustm->avatar)) {
            dpevp_end(ustm->avatar, DPE_CLOSED);
        }
    }
    lsquic_stream_set_ctx(stream, NULL);
}

struct ssl_ctx_st* _dpqic_cb_get_ssl_ctx(void* peer_ctx,
    const struct sockaddr* local_sa)
{
    dpqic_engine_t* engine = ((_dpqic_listen_t*)dpele_aux_data(peer_ctx))->engine;
    return dpssl_get_ctx(engine->group, NULL);
}

struct ssl_ctx_st* _dpqic_cb_lookup_cert(void* cert_lu_ctx,
    const struct sockaddr* local_sa, const char* sni)
{
    dpqic_engine_t* engine = (dpqic_engine_t*)cert_lu_ctx;
    if (sni == NULL || sni[0] == '\0') {
        return dpssl_get_ctx(engine->group, "");
    }
    return dpssl_get_ctx(engine->group, sni);
}

/// HTTP 头回调
const char* dpqic_hdrset_get(const dpqic_hdrset_t* hdrset, const char* name)
{
    DP_CHECK_RETURN(hdrset == NULL || name == NULL || name[0] == 0, NULL)

    const char* namet = NULL;
    for (int i = 0; i < hdrset->headers.count; i++) {
        namet = lsxpack_header_get_name(&hdrset->headers.headers[i]);
        if (namet && strcasecmp(namet, name) == 0) {
            return lsxpack_header_get_value(&hdrset->headers.headers[i]);
        }
    }
    return NULL;
}

struct lsxpack_header* dpqic_hdrset_add(dpqic_hdrset_t* hdrset, int nlen, int vlen)
{
    if (hdrset->headers.count >= hdrset->real_count) {
        int new_count = hdrset->real_count + 2;
        int exp_count = new_count - hdrset->real_count;
        struct lsxpack_header* new_headers = realloc(hdrset->headers.headers,
            sizeof(struct lsxpack_header) * new_count);
        if (new_headers == NULL) {
            return NULL;
        }
        memset(new_headers + hdrset->real_count, 0,
            sizeof(struct lsxpack_header) * exp_count);
        hdrset->headers.headers = new_headers;
        hdrset->real_count = new_count;
    }

    struct lsxpack_header* header = &hdrset->headers.headers[hdrset->headers.count];
    char* buf = (char*)realloc(header->buf, nlen + vlen + 2);
    if (buf == NULL) {
        return NULL;
    }
    memset(buf, 0, nlen + vlen + 2);
    lsxpack_header_set_offset2(header, buf, 0, nlen, nlen + 1, vlen);
    return header;
}

dpqic_hdrset_t* dpqic_hdrset_new(int pre_size)
{
    if (pre_size <= 0) {
        pre_size = 2;
    }

    dpqic_hdrset_t* hdrset = calloc(1, sizeof(dpqic_hdrset_t));
    if (hdrset == NULL) {
        dplog_error("dpqic", "Call calloc failed when create hdrset");
        errno = DPE_NOMEM;
        return NULL;
    }

    struct lsxpack_header* new_headers = calloc(pre_size,
        sizeof(struct lsxpack_header));
    if (new_headers == NULL) {
        free(hdrset);
        errno = DPE_NOMEM;
        return NULL;
    }

    hdrset->headers.headers = new_headers;
    hdrset->real_count = pre_size;

    return hdrset;
}

dpret_t dpqic_hdrset_set(dpqic_hdrset_t* hdrset, const char* name, const char* value)
{
    DP_CHECK_RETURN(name == NULL || name[0] == 0 || hdrset == NULL, DPE_INVAL)

    struct lsxpack_header* header = NULL;
    for (int i = 0; i < hdrset->headers.count; i++) {
        header = &hdrset->headers.headers[i];
        if (strcasecmp(lsxpack_header_get_name(header), name) == 0) {
            if (value) { // 重置值

                int vlen = strlen(value);
                int nlen = header->name_len;
                char* new_buf = (char*)realloc(header->buf, vlen + nlen + 2);
                if (new_buf == NULL) {
                    return DPE_NOMEM;
                }
                memcpy(new_buf + nlen + 1, value, vlen);
                new_buf[nlen] = '\0';
                new_buf[nlen + 1 + vlen] = '\0';
                lsxpack_header_set_offset2(header, new_buf, 0, nlen, nlen + 1, vlen);
                return DPE_OK;
            } else { // value 为 NULL 则删除此头部
                // 不是最后一个元素则交换
                if (i < hdrset->headers.count - 1) {
                    // 与最后一个元素交换
                    struct lsxpack_header*
                        last_header = &hdrset->headers
                                           .headers[hdrset->headers.count - 1];
                    struct lsxpack_header temp = *header;
                    *header = *last_header;
                    *last_header = temp;
                }

                hdrset->headers.count--;
                return DPE_OK;
            }
        }
    }

    if (value == NULL) {
        return DPE_OK;
    }

    int nlen = strlen(name);
    int vlen = strlen(value);
    header = dpqic_hdrset_add(hdrset, nlen, vlen);
    if (header == NULL) {
        return DPE_NOMEM;
    }

    hdrset->headers.count++;
    memcpy(header->buf + header->name_offset, name, nlen);
    memcpy(header->buf + header->val_offset, value, vlen);
    return DPE_OK;
}

dpret_t dpqic_hdrset_count(const dpqic_hdrset_t* hdrset)
{
    DP_CHECK_RETURN(hdrset == NULL, DPE_INVAL);
    return hdrset->headers.count;
}

const char* dpqic_hdrset_at(const dpqic_hdrset_t* hdrset, int index,
    const char** name)
{
    DP_CHECK_RETURN(hdrset == NULL || index < 0 || index >= hdrset->headers.count,
        NULL);
    lsxpack_header_t* header = &hdrset->headers.headers[index];
    if (name) {
        *name = lsxpack_header_get_name(header);
    }
    return lsxpack_header_get_value(header);
}

void dpqic_hdrset_del(dpqic_hdrset_t* hdrset)
{
    if (hdrset == NULL) {
        return;
    }
    for (int i = 0; i < hdrset->real_count; i++) {
        DP_FREE(hdrset->headers.headers[i].buf);
    }
    DP_FREE(hdrset->headers.headers);
    DP_FREE(hdrset);
}

void* _dpqic_cb_hsi_create(void* hsi_ctx, lsquic_stream_t* stream,
    int is_push_promise)
{
    dpqic_hdrset_t* hdrset = calloc(1, sizeof(dpqic_hdrset_t));
    if (hdrset == NULL) {
        dplog_error("dpqic", "Call calloc failed when create hdrset");
    }
    return hdrset;
}

struct lsxpack_header* _dpqic_cb_hsi_predec(void* hdr_set,
    struct lsxpack_header* hdr, size_t space)
{
    if (hdr) {
        char* new_buf = realloc(hdr->buf, space + 2);
        if (new_buf == NULL) {
            dplog_error("dpqic", "Call realloc failed when predec hdrset");
            return NULL;
        }
        hdr->buf = new_buf;
        hdr->val_len = space + 2;
        return hdr;
    } else {
        dpqic_hdrset_t* hdrset = (dpqic_hdrset_t*)hdr_set;
        struct lsxpack_header* header = dpqic_hdrset_add(hdrset, 0, space);
        if (header) {
            lsxpack_header_prepare_decode(header, header->buf, 0, space + 2);
        }
        return header;
    }
}

int _dpqic_cb_hsi_append(void* hdr_set, struct lsxpack_header* hdr)
{
    if (hdr) {
        char* buf = hdr->buf;
        buf[hdr->val_offset + hdr->val_len] = '\0';

        if (hdr->val_offset == hdr->name_offset + hdr->name_len) {
            // Shift the value content back 1 byte from val_offset
            int from = hdr->val_offset + hdr->val_len; // include \0
            for (; from >= hdr->val_offset; from--) {
                buf[from + 1] = buf[from];
            }
            hdr->val_offset++;
        }

        buf[hdr->name_offset + hdr->name_len] = '\0';

        dpqic_hdrset_t* hdrset = (dpqic_hdrset_t*)hdr_set;
        /* _dpqic_cb_hsi_predec 总是返回新 header，
         * 因此只需递增计数。 */
        hdrset->headers.count++;
        return 0;
    } else {
        return 0;
    }
}

void _dpqic_cb_hsi_delete(void* hdr_set)
{
    dpqic_hdrset_del((dpqic_hdrset_t*)hdr_set);
}

lsquic_stream_t* dpqic_original_stream(dpele_t* ele)
{
    return (dpele_type(ele) == dpqic_stream_type())
        ? ((_dpqic_stream_t*)dpele_aux_data(ele))->stream
        : NULL;
}

lsquic_conn_t* dpqic_original_conn(dpele_t* ele)
{
    return (dpele_type(ele) == dpqic_conect_type())
        ? ((_dpqic_conect_t*)dpele_aux_data(ele))->conn
        : NULL;
}

#endif
