#include "dpapp/dpdef.h"
#include "dpapp/dplog.h"
#include "dpapp/dpssl.h"
#include "dpapp_config.h"
#include "dpcwc/dpcwc.h"
#include "dpcwc/dpcwc_asc.h"
#include "dpcwc/dpcwc_aux.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

typedef enum
{
    CET_TCP = 0,
    CET_SSL = 1,
    CET_QUIC = 2,
} cet_proto_t;

typedef struct
{
    cet_proto_t proto;
    const char* host;
    int port;
    const char* alpn;
    const char* sni;
} cet_cfg_t;

static bool _parse_proto(const char* s, cet_proto_t* out)
{
    if (s == NULL || out == NULL) {
        return false;
    }
    if (strcmp(s, "tcp") == 0) {
        *out = CET_TCP;
        return true;
    }
    if (strcmp(s, "ssl") == 0 || strcmp(s, "https") == 0) {
        *out = CET_SSL;
        return true;
    }
    if (strcmp(s, "qic") == 0 || strcmp(s, "http3") == 0) {
        *out = CET_QUIC;
        return true;
    }
    return false;
}

#define ECHO_ALPN "echo"
#define ECHO_SNI  "echo.dpapp"

static void _set_proto_port(cet_cfg_t* cfg, cet_proto_t proto)
{
    cfg->proto = proto;
    if (proto == CET_TCP) {
        cfg->port = 4490;
    } else if (proto == CET_SSL) {
        cfg->port = 4491;
    } else {
        cfg->port = 4492;
    }
}

static void _set_default_cfg(cet_cfg_t* cfg)
{
    cfg->host = "127.0.0.1";
    cfg->alpn = ECHO_ALPN;
    cfg->sni = ECHO_SNI;
    _set_proto_port(cfg, CET_TCP);
}

static const char* _proto_name(cet_proto_t proto)
{
    if (proto == CET_TCP) {
        return "tcp";
    } else if (proto == CET_SSL) {
        return "ssl";
    } else {
        return "qic";
    }
}

static dpret_t _connect_stream(const cet_cfg_t* cfg, dpefd_t** stream_out,
    dpefd_t** owner_out, dpele_t** qconn_out)
{
    dpefd_t* cet = NULL;
    dpret_t err = DPE_INVAL;
    *stream_out = NULL;
    *owner_out = NULL;
    *qconn_out = NULL;

    if (cfg->proto == CET_TCP) {
        err = dpcwc_tcp_client(cfg->host, cfg->port, &cet);
        if (dpret_iserr(err)) {
            return err;
        }
        *stream_out = cet;
        return DPE_OK;
    }

#if DPAPP_HAS_SSL
    if (cfg->proto == CET_SSL) {
        return dpcwc_ssl_client(cfg->host, cfg->port, cfg->alpn, cfg->sni,
            stream_out);
    }
#endif

#if DPAPP_LSQUIC_ENABLE
    if (cfg->proto == CET_QUIC) {
        err = dpqic_add_engine(cfg->alpn, NULL);
        if (dpret_iserr(err)) {
            return err;
        }
        dpefd_t* cet = dpele_new(dpqic_client_type(), cfg->alpn, cfg->host,
            cfg->port);
        if (cet == NULL) {
            return errno;
        }
        dpele_t* conn = NULL;
        err = dpcwc_qic_connect(cet, cfg->sni, NULL, &conn);
        if (dpret_iserr(err)) {
            dpele_del(cet);
            return err;
        }
        dpele_t* stm = NULL;
        err = dpcwc_qic_stream(conn, &stm, true);
        if (dpret_iserr(err)) {
            dpele_del(conn);
            dpele_del(cet);
            return err;
        }
        *stream_out = (dpefd_t*)stm;
        *owner_out = cet;
        *qconn_out = conn;
        return DPE_OK;
    }
#endif

    return DPE_UNSUPPORT;
}

static dpv64_t init00(dpv64_t arg1, dpv64_t arg2)
{
    (void)arg2;
    const cet_cfg_t* cfg = (cet_cfg_t*)arg1.ptr;

#if DPAPP_HAS_SSL
    if (cfg->proto == CET_SSL || cfg->proto == CET_QUIC) {
        dpret_t ret = dpssl_add(ECHO_ALPN, DPROLE_CLIENT, 0, 0);
        if (dpret_iserr(ret)) {
            dplog_error("echo_cet", "Failed to add SSL group: %d", ret);
            return DPV64_NULL;
        }
        dpssl_add_alpn(ECHO_ALPN, ECHO_ALPN);
    }
#endif

    dplog_notice("echo_cet", "start mode=%s target=%s:%d", _proto_name(cfg->proto),
        cfg->host, cfg->port);

    dpefd_t* stream = NULL;
    dpefd_t* owner = NULL;
    dpele_t* qconn = NULL;
    dpret_t err = _connect_stream(cfg, &stream, &owner, &qconn);
    if (dpret_iserr(err)) {
        dplog_error("echo_cet", "connect error: %d", err);
        return DPV64_NULL;
    }

#if DPAPP_HAS_SSL
    if (cfg->proto == CET_SSL) {
        err = dpcwc_ssl_handshake(stream);
        if (dpret_iserr(err)) {
            dplog_error("echo_cet", "ssl handshake error: %d", err);
            dpele_del(stream);
            return DPV64_NULL;
        }
    }
#endif

    dpbuf_t* res = dpbuf_new(0);
    dpbuf_t* line_buf = dpbuf_new(0);
    dpefd_t* line_efd = dpele_new(dpefd_init_type(), fileno(stdin));
    dpefd_set_close(line_efd, false);

    dplog_print("start input:\n");

    while (true) {
        dpret_t line_ret = dpcwc_aio_read_until(line_efd, "\n", 0, line_buf, 0);
        if (line_ret <= 0 || dpbuf_cbegwith(line_buf, "\\q", 2, true)) {
            break;
        }

        int64_t startms = dplog_millis();
        err = dpcwc_aio_write_must(stream, line_buf, 0);
        if (dpret_iserr(err)) {
            dplog_error("echo_cet", "send error: %d", err);
            break;
        }

        dpbuf_reset(res, 0);
        err = dpcwc_aio_read_until(stream, "\n", 0, res, 0);
        if (dpret_iserr(err)) {
            dplog_error("echo_cet", "recv error: %d", err);
            break;
        }

        fwrite(dpbuf_crdata(res), dpbuf_crsize(res), 1, stdout);
        fprintf(stdout, "(time taken: %ldms)\n\n", dplog_millis() - startms);
    }

    dpbuf_del(line_buf);
    dpbuf_del(res);
    dpele_del(line_efd);

#if DPAPP_HAS_SSL
    if (cfg->proto == CET_SSL) {
        dpcwc_ssl_shutdown(stream);
    }
#endif

    dpele_del(stream);
    dpele_del(qconn);
    dpele_del(owner);

    return DPV64_NULL;
}

extern dpret_t dpcwc__cwc_echo_cet(int argc, char** argv, dpapp_hdr_t* hdrs)
{
    static cet_cfg_t cfg;
    _set_default_cfg(&cfg);

    cet_proto_t proto = CET_TCP;
    if (argc >= 2 && !_parse_proto(argv[1], &proto)) {
        dplog_print("unknown protocol: %s", argv[1]);
        dplog_print("usage: echo_cet [tcp|ssl|quic] [host] [port]");
        return 1;
    }

    _set_proto_port(&cfg, proto);

    if (argc >= 3) {
        cfg.host = argv[2];
    }
    if (argc >= 4) {
        cfg.port = atoi(argv[3]);
    }

    hdrs[0].init = init00;
    hdrs[0].init_arg1.ptr = &cfg;
    return 1;
}
