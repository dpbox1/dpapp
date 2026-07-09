#include "dpapp/dpbuf.h"
#include "dpapp/dpdef.h"
#include "dpapp/dpssl.h"
#include "dpcwc/dpcwc.h"
#include "dpcwc/dpcwc_asc.h"
#include "dpcwc/dpcwc_aux.h"
#include <stdio.h>
#include <string.h>

/* HTTP/1.1 + HTTPS 监听器使用的纯文本响应体 */
static const char* k_hello_body = "Hello World";
static const char* k_hello_res = "HTTP/1.1 200 OK\r\n"
                                 "Server: dpapp-http3-demo\r\n"
                                 "Content-Type: text/plain\r\n"
                                 "Content-Length: 11\r\n"
                                 "Connection: keep-alive\r\n"
                                 "\r\n"
                                 "Hello World";

/* HTTP/1.1 请求处理（HTTP 与 HTTPS TCP 监听器共用） */
static dpv64_t http11_session(dpv64_t value)
{
    dpcwc_server_start_args_t task_args = *(dpcwc_server_start_args_t*)value.ptr;
    dpefd_t* peer = (dpefd_t*)task_args.peer;
    dpbuf_t* buf = dpbuf_new(0);
    dpret_t err = DPE_OK;

    if (buf == NULL) {
        dpele_del(peer);
        return DPV64_RES(DPE_NOMEM);
    }

#if DPAPP_HAS_SSL
    if (dpele_type(peer)->iotype == DPAIO_TYPE_SSL) {
        err = dpcwc_ssl_handshake(peer);
        if (dpret_iserr(err)) {
            dplog_error("http3", "ssl handshake failed: %d", err);
            dpbuf_del(buf);
            dpele_del(peer);
            return DPV64_RES(err);
        }
    }
#endif

    while (true) {
        /* 请求行 */
        err = dpcwc_aio_read_until(peer, "\r\n", 0, buf, 0);
        if (dpret_iserr(err)) {
            break;
        }

        /* 读取并丢弃所有头直至空行 */
        while (true) {
            dpbuf_rseek(buf, 0, SEEK_END); // 已读位置
            err = dpcwc_aio_read_until(peer, "\r\n", 0, buf, 0);
            if (dpret_iserr(err)) {
                break;
            }

            if (dpbuf_crsize(buf) == 2
                && memcmp(dpbuf_crdata(buf), "\r\n", 2) == 0) {
                break;
            }
        }
        if (dpret_iserr(err)) {
            break;
        }

        dpbuf_reset(buf, 0);
        dpbuf_wdata(buf, k_hello_res, (int)strlen(k_hello_res));
        dpbuf_eseek(buf, 0, SEEK_END);

        err = dpcwc_aio_write_must(peer, buf, 0);
        if (dpret_iserr(err)) {
            break;
        }
    }

    dplog_notice("http3", "http/https session quit: %d", err);
#if DPAPP_HAS_SSL
    if (dpele_type(peer)->iotype == DPAIO_TYPE_SSL) {
        dpcwc_ssl_shutdown(peer);
    }
#endif
    dpbuf_del(buf);
    dpele_del(peer);
    return DPV64_NULL;
}

#if DPAPP_LSQUIC_ENABLE
/* HTTP/3 会话：accept 连接、打开流、回写响应 */
static dpv64_t http3_stream(dpv64_t value)
{
    dpcwc_server_start_args_t task_args = *(dpcwc_server_start_args_t*)value.ptr;
    dpele_t* conn = (dpele_t*)task_args.peer;
    dpret_t ret = DPE_OK;
    dpqic_hdrset_t* reqhdr = NULL;
    dpqic_hdrset_t* reshdr = NULL;

    dpele_t* stream = NULL;
    ret = dpcwc_qic_stream(conn, &stream, false);
    if (dpret_iserr(ret)) {
        dplog_error("http3", "http3 open stream failed: %d", ret);
        dpele_del(conn);
        return DPV64_NULL;
    }

    ret = dpcwc_qic_recv_hdrset(stream, &reqhdr);
    if (dpret_iserr(ret)) {
        goto FINISH;
    }

    reshdr = dpqic_hdrset_new(3);
    if (reshdr == NULL) {
        ret = DPE_NOMEM;
        goto FINISH;
    }

    ret = dpqic_hdrset_set(reshdr, ":status", "200");
    if (dpret_iserr(ret)) {
        goto FINISH;
    }
    ret = dpqic_hdrset_set(reshdr, "content-type", "text/plain");
    if (dpret_iserr(ret)) {
        goto FINISH;
    }

    ret = dpcwc_qic_send_hdrset(stream, reshdr);
    if (dpret_iserr(ret)) {
        goto FINISH;
    }

    ret = dpcwc_aio_write_data(stream, k_hello_body, 11);

FINISH:
    dplog_notice("http3", "http3 stream quit: %d", ret);
    dpqic_hdrset_del(reqhdr);
    dpqic_hdrset_del(reshdr);
    dpele_del(stream);
    dpele_del(conn);
    return DPV64_NULL;
}
#endif /* DPAPP_LSQUIC_ENABLE */

/* 演示证书 CN/SAN 为 h3.dpapp。
 * sni="" 为默认证书槽：客户端 SNI 与已注册域名均不匹配时回退使用该证书；
 * 若不设置默认证书，则 SNI 必须精确匹配已注册域名，否则 TLS 握手失败。
 * HTTP_H3_SNI 供 lsquic http_client -H / curl --resolve 等显式指定主机名时使用。 */
#define HTTP_H3_SNI "h3.dpapp"

static dpv64_t init01(dpv64_t arg1, dpv64_t arg2)
{
#if DPAPP_HAS_SSL
    const char** crt_key = (const char**)arg1.ptr;
    if (crt_key) {
        dpssl_add("http/1.1", DPROLE_SERVER, 0, 0);
        dpssl_add_alpn("http/1.1", "http/1.1");
        dpssl_add_ctx("http/1.1", "", crt_key[0], crt_key[1]);
        dpssl_add_ctx("http/1.1", HTTP_H3_SNI, crt_key[0], crt_key[1]);
#if DPAPP_LSQUIC_ENABLE
        dpssl_add("h3", DPROLE_SERVER, 0, 0);
        dpssl_add_alpn("h3", "h3");
        dpssl_add_ctx("h3", "", crt_key[0], crt_key[1]);
        dpssl_add_ctx("h3", HTTP_H3_SNI, crt_key[0], crt_key[1]);
#endif
    }
#endif
    dpcwc_server_param_t* params = (dpcwc_server_param_t*)arg2.ptr;
    dpcwc_server_start(params);
    return DPV64_NULL;
}

extern dpret_t dpcwc__cwc_http_svr(int argc, char** argv, dpapp_hdr_t* hdrs)
{
    static dpcwc_server_param_t params[4];

#if DPAPP_HAS_SSL
    static const char* ssl_crt_key[2];
    const char** ssl_crt_key_ptr = NULL;
#endif

    memset(params, 0, sizeof(params));
    int n = 0;

    /* HTTP/1.1（纯 TCP） */
    params[n++] = (dpcwc_server_param_t){
        .type = "tcp",
        .host = "0.0.0.0",
        .port = 4480,
        .start = http11_session,
    };

#if DPAPP_HAS_SSL
    if (argc >= 3) {
        ssl_crt_key_ptr = ssl_crt_key;
        ssl_crt_key[0] = argv[1];
        ssl_crt_key[1] = argv[2];
        params[n++] = (dpcwc_server_param_t){
            .type = "tcp",
            .host = "0.0.0.0",
            .port = 4443,
            .ssl = "http/1.1",
            .start = http11_session,
        };

#if DPAPP_LSQUIC_ENABLE
        params[n++] = (dpcwc_server_param_t){
            .type = "qic",
            .host = "0.0.0.0",
            .port = 4443, // UDP 端口，与 TCP 4443 不冲突
            .ssl = "h3",
            .start = http3_stream,
        };
#endif
    } else {
        dplog_warn("http3",
            "Only HTTP enabled; provide <cert> <key> to enable HTTPS and HTTP/3");
    }
#endif

    hdrs[1].init = init01;
#if DPAPP_HAS_SSL
    hdrs[1].init_arg1 = DPV64_PTR(ssl_crt_key_ptr);
#endif
    hdrs[1].init_arg2 = DPV64_PTR(params);
    return 2;
}
