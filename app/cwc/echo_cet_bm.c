#include "dpapp/dpbuf.h"
#include "dpapp/dpdef.h"
#include "dpapp/dpevp.h"
#include "dpapp/dplog.h"
#include "dpapp/dpret.h"
#include "dpapp/dpssl.h"
#include "dpapp_config.h"
#include "dpcwc/dpcwc.h"
#include "dpcwc/dpcwc_asc.h"
#include "dpcwc/dpcwc_aux.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct bm_local
{
    int64_t tx_bytes;
    int64_t rx_bytes;
} bm_local_t;

typedef struct
{
    int connections;
    int duration;
    const char* host;
    int port;
    const char* alpn;
    const char* sni;

    dpv64_t iofun;

    bm_local_t results[64];
} bm_ctx_t;

static dpret_t bm_tcp(dpele_t* ctc, dpv64_t arg)
{
    bm_ctx_t* ctx = (bm_ctx_t*)arg.ptr;
    int slot = dpevp_id() - 1;
    bm_local_t* result = &ctx->results[slot];

    dpefd_t* fd = NULL;
    dpret_t err = dpcwc_tcp_client(ctx->host, ctx->port, &fd);
    if (dpret_iserr(err)) {
        return err;
    }

    int64_t start_ms = dplog_millis();
    int64_t end_ms = start_ms + (int64_t)ctx->duration * 1000;

    dpbuf_t* buf = dpbuf_new(1024);
    dpbuf_wfill(buf, 1024, 'B');
    dpbuf_eseek(buf, 0, SEEK_END);

    while (dplog_millis() < end_ms) {
        err = dpcwc_aio_write_must(fd, buf, 0);
        if (dpret_iserr(err)) {
            break;
        }
        result->tx_bytes += err;

        err = dpcwc_aio_read_some(fd, buf, 0);
        if (dpret_iserr(err)) {
            break;
        }
        result->rx_bytes += err;
    }

    dpele_del(fd);
    dpbuf_del(buf);
    return DPE_OK;
}

#if DPAPP_HAS_SSL

static dpret_t bm_ssl(dpele_t* ctc, dpv64_t arg)
{
    bm_ctx_t* ctx = (bm_ctx_t*)arg.ptr;
    int slot = dpevp_id() - 1;
    bm_local_t* result = &ctx->results[slot];

    dpret_t err = dpssl_add(ctx->alpn, DPROLE_CLIENT, 0, 0);
    if (dpret_iserr(err) && err != DPE_EXIST) {
        return err;
    }
    dpssl_add_alpn(ctx->alpn, ctx->alpn);

    dpefd_t* fd = NULL;
    err = dpcwc_ssl_client(ctx->host, ctx->port, ctx->alpn, ctx->sni, &fd);
    if (dpret_iserr(err)) {
        return err;
    }

    err = dpcwc_ssl_handshake(fd);
    if (dpret_iserr(err)) {
        dpele_del(fd);
        return err;
    }

    int64_t start_ms = dplog_millis();
    int64_t end_ms = start_ms + (int64_t)ctx->duration * 1000;

    dpbuf_t* buf = dpbuf_new(1024);
    dpbuf_wfill(buf, 1024, 'B');
    dpbuf_eseek(buf, 0, SEEK_END);

    while (dplog_millis() < end_ms) {
        err = dpcwc_aio_write_must(fd, buf, 0);
        if (dpret_iserr(err)) {
            break;
        }
        result->tx_bytes += err;

        err = dpcwc_aio_read_some(fd, buf, 0);
        if (dpret_iserr(err)) {
            break;
        }
        result->rx_bytes += err;
    }

    dpcwc_ssl_shutdown(fd);
    dpele_del(fd);
    dpbuf_del(buf);
    return DPE_OK;
}
#endif

#if DPAPP_HAS_LSQUIC
static dpret_t bm_qic(dpele_t* ctc, dpv64_t arg)
{
    bm_ctx_t* ctx = (bm_ctx_t*)arg.ptr;
    int slot = dpevp_id() - 1;
    bm_local_t* result = &ctx->results[slot];

    dpret_t err = dpqic_add_engine(ctx->alpn, NULL);
    if (dpret_iserr(err)) {
        return err;
    }

    dpefd_t* cet = dpele_new(dpqic_client_type(), ctx->alpn, ctx->host, ctx->port);
    if (cet == NULL) {
        return errno;
    }

    dpele_t* conn = NULL;
    err = dpcwc_qic_connect(cet, ctx->sni, NULL, &conn);
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

    int64_t start_ms = dplog_millis();
    int64_t end_ms = start_ms + (int64_t)ctx->duration * 1000;

    dpbuf_t* buf = dpbuf_new(1024);
    dpbuf_wfill(buf, 1024, 'B');
    dpbuf_eseek(buf, 0, SEEK_END);

    while (dplog_millis() < end_ms) {
        err = dpcwc_aio_write_must((dpefd_t*)stm, buf, 0);
        if (dpret_iserr(err)) {
            break;
        }
        result->tx_bytes += err;

        err = dpcwc_aio_read_some((dpefd_t*)stm, buf, 0);
        if (dpret_iserr(err)) {
            break;
        }
        result->rx_bytes += err;
    }

    dpele_del(stm);
    dpele_del(conn);
    dpele_del(cet);
    dpbuf_del(buf);
    return DPE_OK;
}
#endif

static dpv64_t init00(dpv64_t arg1, dpv64_t arg2)
{
    (void)arg2;
    bm_ctx_t* ctx = (bm_ctx_t*)arg1.ptr;
    const dpapp_info_t* info = dpapp_info();
    int threads = info->each_count[1];
    if (threads > 64) {
        dplog_error("echo_bm", "The benchmark thread more than 64");
        return DPV64_NULL;
    }

    dplog_print("=== Start echo benchmark ===");
    dplog_print("  Threads:              %d", threads);
    dplog_print("  Connections:          %d", ctx->connections);
    dplog_print("  Client Duration:      %ds", ctx->duration);

    memset(ctx->results, 0, sizeof(ctx->results));

    int64_t start_ms = dplog_millis();

    dpele_t** ctc_list = calloc(ctx->connections, sizeof(dpele_t*));
    for (int i = 0; i < ctx->connections; i++) {
        ctc_list[i] = dpele_new(dpctc_init_type(), -1, 0);
        dpevp_add(ctc_list[i], dpctc_submit(), ctx->iofun, DPV64_PTR(ctx));
    }

    dpret_t last_err = DPE_OK;
    for (int i = 0; i < ctx->connections; i++) {
        last_err = dpcwc_await(ctc_list[i], DPV64_NULL);
        dpele_del(ctc_list[i]);
    }

    int64_t end_ms = dplog_millis();
    int64_t total_tx = 0;
    int64_t total_rx = 0;

    for (int i = 0; i < threads; i++) {
        total_tx += ctx->results[i].tx_bytes;
        total_rx += ctx->results[i].rx_bytes;
    }

    double MB = 1024 * 1024;

    double dur_sec = (double)(end_ms - start_ms) / 1000.0;
    double bm_sec = ctx->duration;

    dplog_print("");
    dplog_print("=== Echo benchmark Results ===");
    dplog_print("  Last error:     %s", dperr_detail(last_err));
    dplog_print("  Total Duration: %.2f s", dur_sec);
    dplog_print("  Total TX↑:      %.2f MiB (%ld bytes)", total_tx,
        (double)total_tx / MB);
    dplog_print("  Total RX↓:      %.2f MiB (%ld bytes)", total_rx,
        (double)total_rx / MB);
    dplog_print("  TX Rate↑:       %.2f MiB/s", (double)total_tx / MB / bm_sec);
    dplog_print("  RX Rate↓:       %.2f MiB/s", (double)total_rx / MB / bm_sec);
    dplog_print("");

    exit(0);
    return DPV64_NULL;
}

extern dpret_t dpcwc__cwc_echo_cet_bm(int argc, char** argv, dpapp_hdr_t* hdrs)
{
    static bm_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.connections = 100;
    ctx.duration = 10;
    ctx.host = "127.0.0.1";
    ctx.port = 4490;
    ctx.alpn = "echo";
    ctx.sni = "echo.dpapp";
    ctx.iofun = DPV64_PTR(bm_tcp);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            ctx.connections = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-T") == 0 && i + 1 < argc) {
            ctx.duration = atoi(argv[++i]);
            if (ctx.duration < 1) {
                dplog_error("echo_bm", "duration must be greater than 0");
                return DPE_INVAL;
            }
        } else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            ctx.host = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            ctx.port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-A") == 0 && i + 1 < argc) {
            ctx.alpn = argv[++i];
        } else if (strcmp(argv[i], "-S") == 0 && i + 1 < argc) {
            ctx.sni = argv[++i];
        } else if (strcmp(argv[i], "-P") == 0 && i + 1 < argc) {
            char* prot = argv[++i];
            if (strcasecmp(prot, "tcp") == 0) {
                ctx.iofun = DPV64_PTR(bm_tcp);
            }

#if DPAPP_HAS_SSL
            else if (strcasecmp(prot, "ssl") == 0) {
                ctx.iofun = DPV64_PTR(bm_ssl);
            }
#endif

#if DPAPP_HAS_LSQUIC
            else if (strcasecmp(prot, "qic") == 0) {
                ctx.iofun = DPV64_PTR(bm_qic);
            }
#endif

            else {
                dplog_error("echo_bm", "Unknown protocol");
                return DPE_INVAL;
            }
        }
    }

    hdrs[0].init = init00;
    hdrs[0].init_arg1 = DPV64_PTR(&ctx);

    return 2; // 含 type 1 工作线程
}
