#include "dpcwc/dpcwc_aux.h"
#include "dpapp/dpapp.h"
#include "dpapp/dpasc.h"
#include "dpapp/dpevp.h"
#include "dpapp/dpssl.h"
#include "dpcwc/dpcwc.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void _to_base62(uint64_t value, char* buffer)
{
    static const char base62_chars
        [] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    memset(buffer, '0', 11);
    for (int i = 10; value > 0 && i >= 0; i--) {
        buffer[i] = base62_chars[value % 62];
        value /= 62;
    }
}

static uint64_t _to_uint64(const char* b62)
{
    if (b62 == NULL) {
        return 0;
    }

    int l = strlen(b62);
    if (l > 11) {
        l = 11;
    }

    uint64_t v = 0;
    for (int i = 0, c = 0; i < l; i++) {
        c = b62[i];
        if (c >= '0' && c <= '9') {
            c = c - '0';
        } else if (c >= 'A' && c <= 'Z') {
            c = 10 + (c - 'A');
        } else if (c >= 'a' && c <= 'z') {
            c = 36 + (c - 'a');
        } else {
            v = 0;
            break;
        }
        v = v * 62 + c;
    }

    return v;
}

struct dpcwc_id_t
{
    uint64_t sequence   : 10; // 最大每秒生成 1024000 个 ID
    uint64_t thread_id  : 8;  // 最大支持 256 个线程
    uint64_t machine_id : 5;  // 最大支持 32 台机器
    uint64_t timestamp  : 41; // 约 71.49 年
};

struct dpcwc_id_ctx_t
{
    int sequence;
    uint64_t timestamp;
};
static __thread struct dpcwc_id_ctx_t _gIdctx = {0, 0};

uint64_t dpcwc_id_next(char* out_str)
{
    static int64_t _baseTS = 1735660800000; // 2025-01-01 00:00:00
    static int _max_sequence = (1 << 10) - 1;
    struct dpcwc_id_ctx_t* ctx = &_gIdctx;

    struct dpcwc_id_t id;
    id.machine_id = dpapp_info()->machine_id;
    id.thread_id = dpevp_id();
    id.sequence = ctx->sequence++;
    id.timestamp = dplog_millis() - _baseTS;

    if (id.sequence == _max_sequence) {
        ctx->sequence = 0;
        while (id.timestamp == ctx->timestamp) {
            dpcwc_sleep(0.005, DPV64_NULL);
            id.timestamp = dplog_millis() - _baseTS;
        }
    }
    ctx->timestamp = id.timestamp;

    uint64_t ret = *(uint64_t*)&id;

    if (out_str) {
        _to_base62(ret, out_str);
    }
    return ret;
}

uint64_t dpcwc_id_2u64(const char* str)
{
    return _to_uint64(str);
}

void dpcwc_id_2str(uint64_t id, char* out_str)
{
    _to_base62(id, out_str);
}

// --- TCP / UDS 连接 ---
dpret_t dpcwc_tcp_client(const char* addr, int port, dpefd_t** client)
{
    dpefd_t* cet = dpele_new(dptcp_client_type(), addr, port);
    if (cet == NULL) {
        return errno;
    }
    const dpsockaddr_t* ios = (const dpsockaddr_t*)dpele_aux_data(cet);
    dpret_t ret = dpcwc_aexec(cet, dpskt_connect(), ios);
    if (dpret_isok(ret)) {
        *client = cet;
        return DPE_OK;
    }
    dpele_del(cet);
    return ret;
}

dpret_t dpcwc_tcp_listen(const char* addr, int port, int backlog, dpefd_t** listener)
{
    dpefd_t* lsn = dpele_new(dptcp_listen_type(), addr, port, backlog);
    if (lsn == NULL) {
        return errno;
    }
    *listener = lsn;
    return DPE_OK;
}

dpret_t dpcwc_tcp_accept(dpefd_t* listener, dpefd_t** server)
{
    dpsockaddr_t ios;
    dpret_t ret = dpcwc_aexec(listener, dpskt_accept(), &ios);
    if (dpret_iserr(ret)) {
        return ret;
    }

    dpefd_t* svr = dpele_new(dptcp_server_type(), ret, &ios);
    if (svr == NULL) {
        return errno;
    }
    *server = svr;
    return DPE_OK;
}

dpret_t dpcwc_uds_client(const char* path, dpefd_t** client)
{
    dpefd_t* cet = dpele_new(dpuds_client_type(), path);
    if (cet == NULL) {
        return errno;
    }
    const dpsockaddr_t* ios = (const dpsockaddr_t*)dpele_aux_data(cet);
    dpret_t ret = dpcwc_aexec(cet, dpskt_connect(), ios);
    if (dpret_isok(ret)) {
        *client = cet;
        return DPE_OK;
    }
    dpele_del(cet);
    return ret;
}

dpret_t dpcwc_uds_listen(const char* path, dpefd_t** listener)
{
    dpefd_t* lsn = dpele_new(dpuds_listen_type(), path);
    if (lsn == NULL) {
        return errno;
    }
    *listener = lsn;
    return DPE_OK;
}

dpret_t dpcwc_uds_accept(dpefd_t* listener, dpefd_t** server)
{
    dpsockaddr_t ios;
    dpret_t ret = dpcwc_aexec(listener, dpskt_accept(), &ios);
    if (dpret_iserr(ret)) {
        return ret;
    }

    dpefd_t* svr = dpele_new(dpuds_server_type(), ret, &ios);
    if (svr == NULL) {
        return errno;
    }
    *server = svr;
    return DPE_OK;
}

#if DPAPP_HAS_SSL
dpret_t dpcwc_ssl_client(const char* host, int port, const char* group,
    const char* sni, dpefd_t** client)
{
    dpefd_t* tcp_client = NULL;
    dpret_t ret = dpcwc_tcp_client(host, port, &tcp_client);
    if (dpret_iserr(ret)) {
        return ret;
    }

    dpefd_t* ssl_client = dpele_new(dpssl_client_type(), tcp_client, group, sni);
    if (ssl_client == NULL) {
        dpele_del(tcp_client);
        return errno;
    } else {
        *client = ssl_client;
        return DPE_OK;
    }
}

dpret_t dpcwc_ssl_accept(dpefd_t* listener, const char* group, dpefd_t** server)
{
    dpefd_t* tcp_server = NULL;
    dpret_t ret = dpcwc_tcp_accept(listener, &tcp_server);
    if (dpret_iserr(ret)) {
        return ret;
    }

    dpefd_t* ssl_server = dpele_new(dpssl_server_type(), tcp_server, group);
    if (ssl_server == NULL) {
        dpele_del(tcp_server);
        return errno;
    } else {
        *server = ssl_server;
        return DPE_OK;
    }
}

#endif

static dpv64_t _tcp_accept_loop_run(dpv64_t v)
{
    dpv64_t* argv = (dpv64_t*)v.ptr;
    dpefd_t* listener = (dpefd_t*)argv[0].ptr;
    dpcwc_server_param_t* param = (dpcwc_server_param_t*)argv[1].ptr;

    dpret_t err = DPE_OK;
    while (true) {
        dpefd_t* server = NULL;
#if DPAPP_HAS_SSL
        if (param->ssl) {
            err = dpcwc_ssl_accept(listener, param->ssl, &server);
        } else
#endif
        {
            err = dpcwc_tcp_accept(listener, &server);
        }
        if (dpret_iserr(err)) {
            break;
        }

        dpcwc_server_start_args_t task_args = {
            .peer = server,
            .args = param->start_args,
        };
        // `task_args` 位于当前栈帧；start 协程须在 await/yield
        // 前将其拷贝到局部变量。
        dpaco_wrap(param->start, DPV64_PTR(&task_args), 0);
    }

    dpele_del(listener);
    dplog_warn("dpaux", "TCP listener quit with error code: %d", err);
    return DPV64_NULL;
}

static dpv64_t _uds_accept_loop_run(dpv64_t v)
{
    dpv64_t* argv = (dpv64_t*)v.ptr;
    dpefd_t* listener = (dpefd_t*)argv[0].ptr;
    dpaco_fun_t start = (dpaco_fun_t)argv[1].ptr;
    dpv64_t start_args = argv[2];

    dpret_t err = DPE_OK;
    while (true) {
        dpefd_t* server = NULL;
        err = dpcwc_uds_accept(listener, &server);
        if (dpret_iserr(err)) {
            break;
        }
        dpcwc_server_start_args_t task_args = {
            .peer = server,
            .args = start_args,
        };
        dpaco_wrap(start, DPV64_PTR(&task_args), 0);
    }
    dpele_del(listener);
    dplog_warn("dpaux", "UDS listener quit with error code: %d", err);
    return DPV64_NULL;
}

#if DPAPP_LSQUIC_ENABLE
static dpv64_t _qic_accept_loop_run(dpv64_t v)
{
    dpv64_t* argv = (dpv64_t*)v.ptr;
    dpefd_t* listener = (dpefd_t*)argv[0].ptr;
    dpaco_fun_t start = (dpaco_fun_t)argv[1].ptr;
    dpv64_t start_args = argv[2];

    dpret_t err = DPE_OK;
    while (true) {
        dpele_t* conn = NULL;
        err = dpcwc_aexec(listener, dpqic_accept(), &conn);
        if (dpret_iserr(err)) {
            break;
        }

        dpcwc_server_start_args_t task_args = {
            .peer = conn,
            .args = start_args,
        };
        dpaco_wrap(start, DPV64_PTR(&task_args), 0);
    }
    dpele_del(listener);
    dplog_warn("dpaux", "QUIC listener quit with error code: %d", err);
    return DPV64_NULL;
}
#endif

static dpret_t _start_tcp(const dpcwc_server_param_t* p)
{
    if (!dpssl_enable() && p->ssl) {
        dplog_error("dpaux", "ssl is disabled at build time");
        return DPE_UNSUPPORT;
    }

    dpefd_t* listener = NULL;
    dpret_t ret = dpcwc_tcp_listen(p->host, p->port, SOMAXCONN, &listener);
    if (dpret_iserr(ret) || listener == NULL) {
        dplog_error("dpaux", "failed to listen on %s:%d", p->host, p->port);
        return ret;
    }

    dpv64_t argv[2];
    argv[0] = DPV64_PTR(listener);
    argv[1] = DPV64_PTR(p);
    dpaco_wrap(_tcp_accept_loop_run, DPV64_PTR(argv), 0);
    dplog_notice("dpaux", "tcp server(ssl: %s) started on %s:%d",
        p->ssl ? "true" : "false", p->host, p->port);
    return DPE_OK;
}

static dpret_t _start_uds(const dpcwc_server_param_t* p)
{
    dpefd_t* listener = NULL;
    dpret_t ret = dpcwc_uds_listen(p->host, &listener);
    if (dpret_iserr(ret) || listener == NULL) {
        dplog_error("dpaux", "failed to listen on %s", p->host);
        return ret;
    }
    dpv64_t argv[3];
    argv[0] = DPV64_PTR(listener);
    argv[1] = DPV64_PTR(p->start);
    argv[2] = p->start_args;
    dpaco_wrap(_uds_accept_loop_run, DPV64_PTR(argv), 0);
    dplog_notice("dpaux", "uds server started on %s", p->host);
    return DPE_OK;
}

#if DPAPP_LSQUIC_ENABLE
static dpret_t _start_qic(const dpcwc_server_param_t* p)
{
    if (!p->ssl) {
        dplog_error("dpaux", "qic listener requires ssl group");
        return DPE_INVAL;
    }

    dpret_t ret = dpqic_add_engine(p->ssl, NULL);
    if (dpret_iserr(ret)) {
        dplog_error("dpaux", "failed to add quic engine: %s, ret: %d", p->ssl, ret);
        return ret;
    }

    dpefd_t* listener = dpele_new(dpqic_listen_type(), p->ssl, p->host, p->port);
    if (listener == NULL) {
        dplog_error("dpaux", "failed to listen on %s:%d", p->host, p->port);
        return errno;
    }

    dpv64_t argv[3];
    argv[0] = DPV64_PTR(listener);
    argv[1] = DPV64_PTR(p->start);
    argv[2] = p->start_args;
    dpaco_wrap(_qic_accept_loop_run, DPV64_PTR(argv), 0);
    dplog_notice("dpaux", "quic server started on %s:%d", p->host, p->port);
    return DPE_OK;
}
#endif

dpret_t dpcwc_server_start(const dpcwc_server_param_t* params)
{
    if (params == NULL) {
        return DPE_INVAL;
    }

    int started = 0;
    int index = 0;
    const dpcwc_server_param_t* p = params;
    while (p->type != NULL) {
        if (p->start == NULL || p->host == NULL) {
            dplog_error("dpaux", "invalid server param at index %d", index);
            p++;
            index++;
            continue;
        }

        dpret_t ret = DPE_INVAL;
        if (strcmp(p->type, "tcp") == 0) {
            ret = _start_tcp(p);
        } else if (strcmp(p->type, "uds") == 0) {
            ret = _start_uds(p);
#if DPAPP_LSQUIC_ENABLE
        } else if (strcmp(p->type, "qic") == 0) {
            ret = _start_qic(p);
#endif
        } else {
            dplog_error("dpaux", "unknown server type: %s", p->type);
        }

        if (dpret_isok(ret)) {
            started++;
        }
        p++;
        index++;
    }

    dplog_info("dpaux", "started %d servers", started);
    return started;
}

dpret_t dpcwc_server_start_with_task(dpctc_t* task, dpv64_t arg)
{
    (void)task;
    const dpcwc_server_param_t* list = (const dpcwc_server_param_t*)arg.ptr;
    if (list == NULL) {
        return DPE_INVAL;
    }
    return dpcwc_server_start(list);
}

dpret_t dpcwc_ctc_once(int toid, dpcwc_call_f func, dpv64_t ro_req)
{
    dpele_t* ctc = dpele_new(dpctc_init_type(), toid, 0);
    if (ctc == NULL) {
        return errno;
    }

    dpret_t ret = dpcwc_aexec(ctc, dpctc_submit(), DPV64_PTR(func), ro_req);
    dpele_del(ctc);
    return ret;
}

dpret_t dpcwc_ctc_detach(int toid, dpcwc_call_f func, dpv64_t req)
{
    dpele_t* ctc = dpele_new(dpctc_init_type(), toid, 1);
    if (ctc == NULL) {
        return errno;
    }

    dpret_t ret = dpevp_add(ctc, dpctc_submit(), DPV64_PTR(func), req);
    dpele_del(ctc);
    return ret;
}

dpret_t dpcwc_ctc_reto(dpctc_t* task, int toid, dpcwc_call_f func, dpv64_t arg)
{
    if (task == NULL || func == NULL) {
        return DPE_INVAL;
    }

    dpv64_t* data = (dpv64_t*)dpele_asc_data(task);
    data[0].ptr = func;
    data[1] = arg;
    return dpctc_reto(task, toid);
}

dpret_t dpcwc_ctc_each(int totype, dpcwc_call_f func, dpv64_t ro_req)
{
    const dpapp_info_t* info = dpapp_info();
    totype = abs(totype);
    if (totype >= info->type_count) {
        return DPE_INVAL;
    }

    int tcount = info->each_count[totype];
    int* toids = info->each_ids[totype];
    if (tcount == 0) {
        return DPE_INVAL;
    }

    dpctc_t** ctc_list = calloc(tcount, sizeof(dpctc_t*));
    if (ctc_list == NULL) {
        return DPE_NOMEM;
    }

    dpret_t last_err = DPE_OK;
    int num = 0;
    for (; num < tcount; num++) {
        dpele_t* ctc = dpele_new(dpctc_init_type(), toids[num], 0);
        if (ctc == NULL) {
            last_err = errno;
            break;
        }

        last_err = dpevp_add(ctc, dpctc_submit(), DPV64_PTR(func), ro_req);
        if (last_err == DPE_WAIT) {
            ctc_list[num] = ctc;
        } else {
            dpele_del(ctc);
            break;
        }
    }

    for (int i = 0; i < num; i++) {
        last_err = dpcwc_await(ctc_list[i], DPV64_NULL);
        dpele_del(ctc_list[i]);
    }

    free(ctc_list);
    return last_err;
}
