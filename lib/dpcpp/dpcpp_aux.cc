#include "dpcpp/dpcpp_aux.hh"
#include "dpapp/dpasc.h"
#include "dpapp/dpdef.h"
#include "dpapp/dpret.h"
#include "dpcpp/dpcpp.hh"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

extern "C"
{
#include "dpapp/dpapp.h"
#include "dpapp/dpevp.h"
#include "dpapp/dplog.h"
#include "dpapp/os/dpevp_pri.h"
}

namespace dpcpp
{

namespace
{
void to_base62(uint64_t value, char* buffer)
{
    static const char base62_chars
        [] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    std::memset(buffer, '0', 11);
    for (int i = 10; value > 0 && i >= 0; i--) {
        buffer[i] = base62_chars[value % 62];
        value /= 62;
    }
}

uint64_t from_base62(const char* b62)
{
    if (!b62) {
        return 0;
    }

    int len = std::strlen(b62);
    if (len > 11) {
        len = 11;
    }

    uint64_t v = 0;
    for (int i = 0; i < len; i++) {
        char c = b62[i];
        int digit = 0;
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'A' && c <= 'Z') {
            digit = 10 + (c - 'A');
        } else if (c >= 'a' && c <= 'z') {
            digit = 36 + (c - 'a');
        } else {
            return 0;
        }
        v = v * 62 + digit;
    }

    return v;
}

struct DpId
{
    uint64_t sequence   : 10; // 最大每秒生成 1024000 个 ID
    uint64_t thread_id  : 8;  // 最大支持 256 个线程
    uint64_t machine_id : 5;  // 最大支持 32 台机器
    uint64_t timestamp  : 41; // 约 71.49 年
};

struct DpIdContext
{
    int sequence = 0;
    uint64_t timestamp = 0;
};

thread_local DpIdContext g_idctx;
static constexpr int64_t BASE_TS = 1735660800000; // 2025-01-01 00:00:00
static constexpr int MAX_SEQUENCE = (1 << 10) - 1;
} // namespace

uint64_t next_id(char* out_str)
{
    const dpapp_info_t* info = dpapp_info();
    DpIdContext* ctx = &g_idctx;

    DpId id;
    id.machine_id = info->machine_id;
    id.thread_id = dpevp_id();
    id.sequence = ctx->sequence++;
    id.timestamp = dplog_millis() - BASE_TS;

    if (id.sequence == MAX_SEQUENCE) {
        ctx->sequence = 0;
        while (id.timestamp == ctx->timestamp) {
            dpcpp::sleep(0.005);
            id.timestamp = dplog_millis() - BASE_TS;
        }
    }
    ctx->timestamp = id.timestamp;

    uint64_t ret = *(uint64_t*)&id;

    if (out_str) {
        to_base62(ret, out_str);
    }
    return ret;
}

uint64_t id_from_string(const char* str)
{
    return from_base62(str);
}

void id_to_string(uint64_t id, char* out_str)
{
    to_base62(id, out_str);
}

namespace server
{

// TCP accept 协程 —— 对应 dpcwc/dpcwc_ext.c 的 _tcp_accept_loop_run
static aco_void _tcp_accept(dpefd_t* listener, aco_callback start_func,
    const char* ssl_group, dpv64_t start_args)
{
    dpret_t err = DPE_OK;
    while (true) {
        dpefd_t* server = nullptr;
#if DPAPP_HAS_SSL
        if (ssl_group && ssl_group[0] != '\0') {
            server = co_await ssl_accept(listener, ssl_group);
        } else
#endif
        {
            server = co_await tcp_accept(listener);
        }
        if (!server) {
            err = dpele_ret(listener);
            break;
        }

        aco_start(start_func(server, start_args));
    }

    dpele_del(listener);
    dplog_warn("dpaux", "TCP listener quit with error code: %d", err);
    co_return;
}

// QUIC accept 协程
#if DPAPP_HAS_LSQUIC
static aco_void _qic_accept(dpefd_t* listener, aco_callback start_func,
    dpv64_t start_args)
{
    dpret_t err = DPE_OK;
    while (true) {
        dpele_t* conn = nullptr;
        dpret_t ret = co_await aexec(listener, dpqic_accept(), &conn);
        if (dpret_iserr(ret) || !conn) {
            err = dpele_ret(listener);
            break;
        }

        aco_start(start_func(conn, start_args));
    }

    dpele_del(listener);
    dplog_warn("dpaux", "QUIC listener quit with error code: %d", err);
    co_return;
}
#endif

// UDS accept 协程
static aco_void _uds_accept(dpefd_t* listener, aco_callback start_func,
    dpv64_t start_args)
{
    dpret_t err = DPE_OK;
    while (true) {
        dpefd_t* server = co_await uds_accept(listener);
        if (!server) {
            err = dpele_ret(listener);
            break;
        }
        aco_start(start_func(server, start_args));
    }

    dpele_del(listener);
    dplog_warn("dpaux", "UDS listener quit with error code: %d", err);
    co_return;
}

// TCP listen —— SSL group/ctx 由调用方 init 函数事先创建
static aco_void _tcp_listen(const parameter& param)
{
    if (!param.ssl.empty() && !dpssl_enable()) {
        dplog_error("dpaux", "ssl is disabled at build time");
        co_return;
    }

    dpefd_t* listener = co_await tcp_listen(param.host.c_str(), param.port,
        SOMAXCONN);
    if (!listener) {
        dplog_error("dpaux", "failed to listen on %s:%d", param.host.c_str(),
            param.port);
        co_return;
    }

    aco_start(_tcp_accept(listener, param.start,
        param.ssl.empty() ? nullptr : param.ssl.c_str(), param.start_args));

    dplog_notice("dpaux", "tcp server(ssl: %s) started on %s:%d",
        param.ssl.empty() ? "false" : "true", param.host.c_str(), param.port);
    co_return;
}

// UDS listen 协程
static aco_void _uds_listen(const parameter& param)
{
    dpefd_t* listener = co_await uds_listen(param.host.c_str());
    if (!listener) {
        dplog_error("dpaux", "failed to listen on %s", param.host.c_str());
        co_return;
    }

    aco_start(_uds_accept(listener, param.start, param.start_args));

    dplog_notice("dpaux", "uds server started on %s", param.host.c_str());
    co_return;
}

#if DPAPP_HAS_LSQUIC
// QUIC listen —— SSL group/ctx 由调用方 init 函数事先创建
static aco_void _qic_listen(const parameter& param)
{
    if (param.ssl.empty()) {
        dplog_error("dpaux", "qic listener requires ssl group");
        co_return;
    }

    dpret_t ret = dpqic_add_engine(param.ssl.c_str(), nullptr);
    if (dpret_iserr(ret)) {
        dplog_error("dpaux", "failed to add quic engine: %s, ret: %d",
            param.ssl.c_str(), ret);
        co_return;
    }

    dpefd_t* listener = dpele_new(dpqic_listen_type(), param.ssl.c_str(),
        param.host.c_str(), param.port);
    if (!listener) {
        dplog_error("dpaux", "failed to listen on %s:%d", param.host.c_str(),
            param.port);
        co_return;
    }

    aco_start(_qic_accept(listener, param.start, param.start_args));

    dplog_notice("dpaux", "quic server started on %s:%d", param.host.c_str(),
        param.port);
    co_return;
}
#endif

dpcpp::aco_ret start(const parameters& params)
{
    dpret_t ret = DPE_OK;
    int server_count = 0;
    for (auto& param : params) {
        if (param.start == nullptr) {
            dplog_error("dpaux", "start function is null: %s", param.type.c_str());
            ret = DPE_INVAL;
            continue;
        }

        if (param.type == "tcp") {
            co_await _tcp_listen(param);
            ret = DPE_OK;
#if DPAPP_HAS_LSQUIC
        } else if (param.type == "qic") {
            co_await _qic_listen(param);
            ret = DPE_OK;
#endif
        } else if (param.type == "uds") {
            co_await _uds_listen(param);
            ret = DPE_OK;
        } else {
            dplog_error("dpaux", "unknown server type: %s", param.type.c_str());
            ret = DPE_INVAL;
        }

        if (dpret_isok(ret)) {
            server_count++;
        }
    }

    dplog_info("dpaux", "started %d servers", server_count);

    co_return server_count;
}

dpcpp::aco_ret start_with_task(dpele_t* task, dpv64_t unused, dpv64_t udata)
{
    (void)unused;
    const parameters* params = (const parameters*)(((dpv64_t*)udata.ptr)[0]).ptr;
    co_return co_await start(*params);
}

} // namespace server

aco_ret ctc_once(int toid, aco_callback func, dpv64_t req)
{
    dpele_t* ctc = dpele_new(dpctc_init_type(), toid, 0);
    if (!ctc) {
        co_return errno;
    }

    dpret_t ret = co_await aexec(ctc, dpctc_submit(), DPV64_PTR(func), req);
    dpele_del(ctc);
    co_return ret;
}

aco_ret ctc_each(int totype, aco_callback func, dpv64_t req)
{
    const dpapp_info_t* info = dpapp_info();
    totype = abs(totype);
    if (totype >= info->type_count) {
        co_return DPE_INVAL;
    }

    int tcount = info->each_count[totype];
    int* toids = info->each_ids[totype];
    if (tcount == 0) {
        co_return DPE_INVAL;
    }

    std::vector<dpele_t*> ctc_list;
    ctc_list.reserve(tcount);

    dpret_t last_err = DPE_OK;
    for (int i = 0; i < tcount; i++) {
        dpele_t* ctc = dpele_new(dpctc_init_type(), toids[i], 0);
        if (!ctc) {
            last_err = errno;
            break;
        }

        last_err = dpevp_add(ctc, dpctc_submit(), DPV64_PTR(func), req);
        if (last_err == DPE_WAIT) {
            ctc_list.push_back(ctc);
        } else {
            dpele_del(ctc);
            break;
        }
    }

    for (auto* ctc : ctc_list) {
        last_err = co_await await(ctc);
        dpele_del(ctc);
    }

    co_return last_err;
}

aco_ret ctc_detach(int toid, aco_callback func, dpv64_t req)
{
    dpele_t* ctc = dpele_new(dpctc_init_type(), toid, 1);
    if (!ctc) {
        co_return errno;
    }
    dpret_t ret = dpevp_add(ctc, dpctc_submit(), DPV64_PTR(func), req);
    dpele_del(ctc);
    co_return ret;
}

aco_ret ctc_reto(dpele_t* task, int toid, aco_callback func, dpv64_t req)
{
    if (!task || !func) {
        co_return DPE_INVAL;
    }

    dpv64_t* data = (dpv64_t*)dpele_asc_data(task);
    data[0] = DPV64_PTR(func);
    data[1] = req;
    co_return dpctc_reto(task, toid);
}

} // namespace dpcpp

#include "dpapp/dpasc.h"

namespace dpcpp
{

// --- TCP / UDS 端点工厂 ---

aco_efd tcp_client(const char* addr, int port)
{
    dpefd_t* client = dpele_new(dptcp_client_type(), addr, port);
    if (!client) {
        co_return nullptr;
    }
    const dpsockaddr_t* ios = (const dpsockaddr_t*)dpele_aux_data(client);
    dpret_t ret = co_await aexec(client, dpskt_connect(), ios);
    if (dpret_isok(ret)) {
        co_return client;
    }
    dpele_del(client);
    co_return nullptr;
}

aco_efd tcp_listen(const char* addr, int port, int backlog)
{
    dpefd_t* listener = dpele_new(dptcp_listen_type(), addr, port, backlog);
    co_return listener;
}

aco_efd tcp_accept(dpefd_t* listener)
{
    dpsockaddr_t ios;
    dpret_t ret = co_await aexec(listener, dpskt_accept(), &ios);
    if (dpret_iserr(ret)) {
        co_return nullptr;
    }

    dpefd_t* server = dpele_new(dptcp_server_type(), ret, &ios);
    co_return server;
}

aco_efd uds_client(const char* path)
{
    dpefd_t* client = dpele_new(dpuds_client_type(), path);
    if (!client) {
        co_return nullptr;
    }
    const dpsockaddr_t* ios = (const dpsockaddr_t*)dpele_aux_data(client);
    dpret_t ret = co_await aexec(client, dpskt_connect(), ios);
    if (dpret_isok(ret)) {
        co_return client;
    }
    dpele_del(client);
    co_return nullptr;
}

aco_efd uds_listen(const char* path)
{
    co_return dpele_new(dpuds_listen_type(), path);
}

aco_efd uds_accept(dpefd_t* listener)
{
    dpsockaddr_t ios;
    dpret_t ret = co_await aexec(listener, dpskt_accept(), &ios);
    if (dpret_iserr(ret)) {
        co_return nullptr;
    }
    co_return dpele_new(dpuds_server_type(), ret, &ios);
}

#if DPAPP_HAS_SSL

aco_efd ssl_client(const char* host, int port, const char* group, const char* sni)
{
    dpefd_t* tcp = co_await tcp_client(host, port);
    if (!tcp) {
        co_return nullptr;
    }

    dpefd_t* client = dpele_new(dpssl_client_type(), tcp, group, sni);
    if (!client) {
        dpele_del(tcp);
        co_return nullptr;
    }
    co_return client;
}

aco_efd ssl_accept(dpefd_t* listener, const char* group)
{
    dpefd_t* tcp = co_await tcp_accept(listener);
    if (!tcp) {
        co_return nullptr;
    }

    dpefd_t* server = dpele_new(dpssl_server_type(), tcp, group);
    if (!server) {
        dpele_del(tcp);
        co_return nullptr;
    }
    co_return server;
}

#endif

} // namespace dpcpp
