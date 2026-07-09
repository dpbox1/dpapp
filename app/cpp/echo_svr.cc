#include "dpcpp/dpcpp.hh"
#include "dpcpp/dpcpp_asc.hh"
#include "dpcpp/dpcpp_buf.hh"
#include <memory>

#define ECHO_ALPN "echo"
#define ECHO_SNI  "echo.dpapp"

// 纯 TCP echo 处理协程
static dpcpp::aco_ret echoio_looper_tcp(dpefd_t* server, dpv64_t server_unused)
{
    dplog_notice("echo", "A new connection %p", server);

    dpret_t err = DPE_OK;
    dpcpp::buf rbuf(0);
    int64_t iobytes = 0;
    int64_t end_ts = 0;
    while (true) {
        err = co_await dpcpp::aio_read_some(server, rbuf.get());
        if (dpret_iserr(err)) {
            break;
        }

        iobytes += err;
        end_ts = dplog_millis();

        err = co_await dpcpp::aio_write_must(server, rbuf.get());
        if (dpret_iserr(err)) {
            break;
        }
    }

    dpele_del(server);
    dplog_notice("echo", "Connection %p quit: %d, io bytes: %ld, end time: %ld",
        server, err, iobytes, end_ts);
    co_return err;
}

#if DPAPP_HAS_SSL
// SSL echo：先握手
static dpcpp::aco_ret echoio_looper_ssl(dpefd_t* server, dpv64_t server_unused)
{
    dplog_notice("echo", "A new SSL connection %p", server);

    dpret_t err = co_await dpcpp::ssl_handshake(server);
    if (dpret_iserr(err)) {
        dplog_warn("echo", "SSL handshake failed %p: %d", server, err);
        dpele_del(server);
        co_return err;
    }

    dpcpp::buf rbuf(0);
    int64_t iobytes = 0;
    int64_t end_ts = 0;
    while (true) {
        err = co_await dpcpp::aio_read_some(server, rbuf.get());
        if (dpret_iserr(err)) {
            break;
        }

        iobytes += err;
        end_ts = dplog_millis();

        err = co_await dpcpp::aio_write_must(server, rbuf.get());
        if (dpret_iserr(err)) {
            break;
        }
    }

    co_await dpcpp::ssl_shutdown(server);
    dpele_del(server);
    dplog_notice("echo", "SSL connection %p quit: %d, io bytes: %ld, end time: %ld",
        server, err, iobytes, end_ts);
    co_return err;
}
#endif

#if DPAPP_LSQUIC_ENABLE
// QUIC echo：先打开流
static dpcpp::aco_ret echoio_looper_quic(dpele_t* conn, dpv64_t conn_unused)
{
    dplog_notice("echo", "A new QUIC connection %p", conn);

    dpele_t* stream = nullptr;
    dpret_t sret = co_await dpcpp::qic_stream(conn, &stream, false);
    if (dpret_iserr(sret) || !stream) {
        dplog_warn("echo", "QUIC open stream failed %p", conn);
        dpele_del(conn);
        co_return DPE_CONNRESET;
    }

    dpcpp::buf rbuf(0);
    dpret_t err = DPE_OK;
    int64_t iobytes = 0;
    int64_t end_ts = 0;
    while (true) {
        err = co_await dpcpp::aio_read_some((dpefd_t*)stream, rbuf.get());
        if (dpret_iserr(err)) {
            break;
        }

        iobytes += err;
        end_ts = dplog_millis();

        err = co_await dpcpp::aio_write_must((dpefd_t*)stream, rbuf.get());
        if (dpret_iserr(err)) {
            break;
        }
    }

    dpele_del(stream);
    dpele_del(conn);
    dplog_notice("echo", "QUIC connection %p quit: %d, io bytes: %ld, end time: %ld",
        conn, err, iobytes, end_ts);
    co_return err;
}
#endif

// cpp echo 服务端入口
extern "C" dpret_t dpcpp__cpp_echo_svr(int argc, char** argv, dpcpp::app_hdr* hdrs)
{
    static dpcpp::server::parameters params = {dpcpp::server::parameter{
        .type = "tcp",
        .host = "127.0.0.1",
        .port = 4490,
        .start = echoio_looper_tcp,
    }};

#if DPAPP_HAS_SSL
    static const char* ssl_crt_key[2];
    static bool has_ssl = false;
    if (argc >= 3) {
        ssl_crt_key[0] = argv[1];
        ssl_crt_key[1] = argv[2];
        has_ssl = true;
        params.emplace_back(dpcpp::server::parameter{
            .type = "tcp",
            .host = "127.0.0.1",
            .port = 4491,
            .ssl = "echo",
            .start = echoio_looper_ssl,
        });
#if DPAPP_LSQUIC_ENABLE
        params.emplace_back(dpcpp::server::parameter{
            .type = "qic",
            .host = "127.0.0.1",
            .port = 4492,
            .ssl = "echo",
            .start = echoio_looper_quic,
        });
#endif
    } else {
        dplog_print("Enable ssl server with pem type <cert> <key> parameters");
    }
#endif

    hdrs[1].init = [](dpv64_t arg1, dpv64_t arg2) -> dpcpp::aco_v64 {
#if DPAPP_HAS_SSL
        const char** crt_key = (const char**)arg1.ptr;
        if (crt_key) {
            dpssl_add(ECHO_ALPN, DPROLE_SERVER, 0, 0);
            dpssl_add_alpn(ECHO_ALPN, ECHO_ALPN);
            dpssl_add_ctx(ECHO_ALPN, "", crt_key[0], crt_key[1]);
            dpssl_add_ctx(ECHO_ALPN, ECHO_SNI, crt_key[0], crt_key[1]);
        }
#endif
        dpcpp::server::parameters* p = (dpcpp::server::parameters*)arg2.ptr;
        co_await dpcpp::server::start(*p);
        co_return DPV64_NULL;
    };
#if DPAPP_HAS_SSL
    hdrs[1].init_arg1 = DPV64_PTR(has_ssl ? ssl_crt_key : nullptr);
#endif
    hdrs[1].init_arg2 = DPV64_PTR(&params);
    return 2;
}
