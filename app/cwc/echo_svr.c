#include "dpapp/dplog.h"
#include "dpapp/dpssl.h"
#include "dpapp_config.h"
#include "dpcwc/dpcwc.h"
#include "dpcwc/dpcwc_asc.h"
#include "dpcwc/dpcwc_aux.h"
#include <stdint.h>
#include <string.h>

/**
 * 读入并回写，直至读错误或发送失败；开始/结束时输出日志。
 * @param efd 传输端点
 * @return 退出时的错误码或读返回值
 */
static dpret_t _echo_io_loop(dpefd_t* efd)
{
    dplog_notice("cwc", "Connection %p io start", efd);

    dpbuf_t* rbuf = dpbuf_new(0);
    dpret_t err = DPE_OK;
    int64_t bytes = 0;
    int64_t ts = 0;
    while (true) {
        dpbuf_reset(rbuf, DPBUF_INIT_W);
        err = dpcwc_aio_read_some(efd, rbuf, 0);
        if (dpret_iserr(err) || err <= 0) {
            break;
        }

        bytes += err;
        ts = dplog_millis();
        err = dpcwc_aio_write_must(efd, rbuf, 0);
        if (dpret_iserr(err)) {
            break;
        }
    }
    dpbuf_del(rbuf);
    dplog_notice("cwc", "Connection %p quit: %d, io bytes: %ld, end time: %ld", efd,
        err, bytes, ts);
    return err;
}

/** 单连接 echo：读一行回写直至出错 */
static dpv64_t echoio_looper_tcp(dpv64_t value)
{
    dpcwc_server_start_args_t task_args = *(dpcwc_server_start_args_t*)value.ptr;
    dpefd_t* server = (dpefd_t*)task_args.peer;

    _echo_io_loop(server);

    dpele_del(server);
    return DPV64_NULL;
}

#if DPAPP_HAS_SSL
/** SSL echo：先握手，再 recv/send 循环 */
static dpv64_t echoio_looper_ssl(dpv64_t value)
{
    dpcwc_server_start_args_t task_args = *(dpcwc_server_start_args_t*)value.ptr;
    dpefd_t* server = (dpefd_t*)task_args.peer;

    dpret_t err = dpcwc_ssl_handshake(server);
    if (dpret_iserr(err)) {
        dplog_warn("cwc", "SSL handshake failed %p: %d", server, err);
        dpele_del(server);
        return DPV64_NULL;
    }

    _echo_io_loop(server);

    dpcwc_ssl_shutdown(server);
    dpele_del(server);
    return DPV64_NULL;
}
#endif

#if DPAPP_LSQUIC_ENABLE
/** QUIC echo：打开流后 recv/send 循环 */
static dpv64_t echoio_looper_quic(dpv64_t value)
{
    dpcwc_server_start_args_t task_args = *(dpcwc_server_start_args_t*)value.ptr;
    dpele_t* conn = (dpele_t*)task_args.peer;

    dpele_t* stream = NULL;
    dpret_t err = dpcwc_qic_stream(conn, &stream, false);
    if (dpret_iserr(err)) {
        dplog_warn("cwc", "QUIC open stream failed %p: %d", conn, err);
        dpele_del(conn);
        return DPV64_NULL;
    }

    _echo_io_loop((dpefd_t*)stream);

    dpele_del(stream);
    dpele_del(conn);
    return DPV64_NULL;
}
#endif

#define ECHO_ALPN "echo"
#define ECHO_SNI  "echo.dpapp"

static dpv64_t init01(dpv64_t arg1, dpv64_t arg2)
{
#if DPAPP_HAS_SSL
    const char** crt_key = (const char**)arg1.ptr;
    if (crt_key) {
        dpssl_add(ECHO_ALPN, DPROLE_SERVER, 0, 0);
        dpssl_add_alpn(ECHO_ALPN, ECHO_ALPN);
        dpssl_add_ctx(ECHO_ALPN, "", crt_key[0], crt_key[1]);
        dpssl_add_ctx(ECHO_ALPN, ECHO_SNI, crt_key[0], crt_key[1]);
    }
#endif

    dpcwc_server_param_t* params = (dpcwc_server_param_t*)arg2.ptr;
    dpcwc_server_start(params);
    return DPV64_NULL;
}

extern dpret_t dpcwc__cwc_echo_svr(int argc, char** argv, dpapp_hdr_t* hdrs)
{
    /* 最多 3 个 listener + 1 个全零哨兵 */
    static dpcwc_server_param_t params[4];

#if DPAPP_HAS_SSL
    static const char* ssl_crt_key[2];
    const char** ssl_crt_key_ptr = NULL;
#endif

    memset(params, 0, sizeof(params));
    int n = 0;

    params[n++] = (dpcwc_server_param_t){
        .type = "tcp",
        .host = "127.0.0.1",
        .port = 4490,
        .start = echoio_looper_tcp,
    };

#if DPAPP_HAS_SSL
    if (argc >= 3) {
        ssl_crt_key_ptr = ssl_crt_key;
        ssl_crt_key[0] = argv[1];
        ssl_crt_key[1] = argv[2];
        params[n++] = (dpcwc_server_param_t){
            .type = "tcp",
            .host = "127.0.0.1",
            .port = 4491,
            .ssl = ECHO_ALPN,
            .start = echoio_looper_ssl,
        };
#if DPAPP_LSQUIC_ENABLE
        params[n++] = (dpcwc_server_param_t){
            .type = "qic",
            .host = "127.0.0.1",
            .port = 4492,
            .ssl = ECHO_ALPN,
            .start = echoio_looper_quic,
        };
#endif
    }
#endif

    hdrs[1].init = init01;
#if DPAPP_HAS_SSL
    hdrs[1].init_arg1 = DPV64_PTR(ssl_crt_key_ptr);
#endif
    hdrs[1].init_arg2 = DPV64_PTR(params);

    if (argc < 3) {
        dplog_print("Enable ssl server with pem type <cert> <key> parameters");
    }

    return 2;
}
