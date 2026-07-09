#include "dpapp/dpbuf.h"
#include "dpapp/dpevp.h"
#include "dpapp/dpqic.h"
#include "dpapp/dpssl.h"
#include "dpcpp/dpcpp.hh"
#include "dpcpp/dpcpp_asc.hh"
#include "dpcpp/dpcpp_buf.hh"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

enum cet_proto_t
{
    CET_TCP = 0,
    CET_SSL = 1,
    CET_QUIC = 2,
};

struct cet_cfg_t
{
    cet_proto_t proto;
    const char* host;
    int port;
    const char* alpn;
    const char* sni;
};

static bool parse_proto(const char* s, cet_proto_t& out)
{
    if (s == nullptr) {
        return false;
    }
    if (std::strcmp(s, "tcp") == 0) {
        out = CET_TCP;
        return true;
    }
    if (std::strcmp(s, "ssl") == 0 || std::strcmp(s, "https") == 0) {
        out = CET_SSL;
        return true;
    }
    if (std::strcmp(s, "qic") == 0 || std::strcmp(s, "http3") == 0) {
        out = CET_QUIC;
        return true;
    }
    return false;
}

static void set_default_cfg(cet_cfg_t& cfg, cet_proto_t proto)
{
    cfg.proto = proto;
    cfg.host = "127.0.0.1";
    cfg.alpn = "echo";
    cfg.sni = "echo.dpapp";
    if (proto == CET_TCP) {
        cfg.port = 4490;
    } else if (proto == CET_SSL) {
        cfg.port = 4491;
    } else {
        cfg.port = 4492;
    }
}

static const char* proto_name(cet_proto_t proto)
{
    if (proto == CET_TCP) {
        return "tcp";
    } else if (proto == CET_SSL) {
        return "ssl";
    } else {
        return "qic";
    }
}

extern "C" dpret_t dpcpp__cpp_echo_cet(int argc, char** argv, dpcpp::app_hdr* hdrs)
{
    static cet_cfg_t cfg;
    cet_proto_t proto = CET_TCP;
    if (argc >= 2 && !parse_proto(argv[1], proto)) {
        dplog_error("cpp_echo_cet", "unknown protocol: %s", argv[1]);
        dplog_print("usage: cpp_cet [tcp|ssl|quic] [host] [port]");
        return 1;
    }
    set_default_cfg(cfg, proto);
    if (argc >= 3) {
        cfg.host = argv[2];
    }
    if (argc >= 4) {
        cfg.port = std::atoi(argv[3]);
    }

    hdrs[0].init = [](dpv64_t arg1, dpv64_t arg2) -> dpcpp::aco_v64 {
        (void)arg2;
        cet_cfg_t cfg = *(cet_cfg_t*)arg1.ptr;

        dplog_notice("cpp_echo_cet", "start mode=%s target=%s:%d",
            proto_name(cfg.proto), cfg.host, cfg.port);

        dpefd_t* stream = nullptr;
        dpefd_t* owner = nullptr;
        dpele_t* qconn = nullptr;
        dpcpp::efd client = nullptr;

#if DPAPP_HAS_SSL
        if (cfg.proto == CET_SSL || cfg.proto == CET_QUIC) {
            dpret_t ret = dpssl_add(cfg.alpn, DPROLE_CLIENT, 0, 0);
            if (ret != DPE_OK && ret != DPE_EXIST) {
                dplog_error("cpp_echo_cet", "create ssl group error: %d", ret);
                co_return DPV64_NULL;
            }
            dpssl_add_alpn(cfg.alpn, cfg.alpn);
        }
#endif

        if (cfg.proto == CET_TCP) {
            client = co_await dpcpp::tcp_client(cfg.host, cfg.port);
            if (!client) {
                dplog_error("cpp_echo_cet", "tcp connect error");
                co_return DPV64_NULL;
            }
            stream = client;
            owner = client;
        }
#if DPAPP_HAS_SSL
        else if (cfg.proto == CET_SSL) {
            dpele_t* ssn_ele = co_await dpcpp::ssl_client(cfg.host, cfg.port,
                cfg.alpn, cfg.sni);
            if (!ssn_ele) {
                dplog_error("cpp_echo_cet", "ssl client create error");
                co_return DPV64_NULL;
            }
            dpret_t hs_err = co_await dpcpp::ssl_handshake(ssn_ele);
            if (dpret_iserr(hs_err)) {
                dplog_error("cpp_echo_cet", "ssl handshake error: %d", hs_err);
                dpele_del(ssn_ele);
                co_return DPV64_NULL;
            }
            stream = ssn_ele;
            owner = ssn_ele;
        }
#endif
#if DPAPP_LSQUIC_ENABLE
        else if (cfg.proto == CET_QUIC) {
            client = dpele_new(dpqic_client_type(), cfg.alpn, cfg.host, cfg.port);
            if (!client) {
                dplog_error("cpp_echo_cet", "quic client create error");
                co_return DPV64_NULL;
            }
            qconn = nullptr;
            dpret_t qret = co_await dpcpp::qic_connect(client, cfg.sni, nullptr,
                &qconn);
            if (dpret_iserr(qret) || !qconn) {
                dpele_del(client);
                dplog_error("cpp_echo_cet", "quic connect error");
                co_return DPV64_NULL;
            }
            dpele_t* qstm = nullptr;
            qret = co_await dpcpp::qic_stream(qconn, &qstm, true);
            if (dpret_iserr(qret) || !qstm) {
                dpele_del(qconn);
                dpele_del(client);
                dplog_error("cpp_echo_cet", "quic stream create error");
                co_return DPV64_NULL;
            }
            stream = (dpefd_t*)qstm;
            owner = client;
        }
#endif
        else {
            dplog_error("cpp_echo_cet", "unsupported protocol in this build");
            co_return DPV64_NULL;
        }

        if (!stream) {
            dplog_error("cpp_echo_cet", "connect server with error");
            co_return DPV64_NULL;
        }

        dpcpp::buf res(0);
        if (!res) {
            if (cfg.proto == CET_QUIC && stream != nullptr && stream != owner) {
                dpele_del(stream);
            }
            if (qconn) {
                dpele_del(qconn);
            }
            if (owner) {
                dpele_del(owner);
            }
            co_return DPV64_NULL;
        }

        dpcpp::efd line_efd(dpefd_init_type(), fileno(stdin));
        if (!line_efd) {
            if (owner) {
                dpele_del(owner);
            }
            co_return DPV64_NULL;
        }
        line_efd.set_close(false);

        dpcpp::buf line_buf(0);

        while (true) {
            dpret_t ret = co_await dpcpp::aio_read_until(line_efd, "\n",
                line_buf.get());
            if (ret <= 0 || dpbuf_cbegwith(line_buf, "\\q", 2, true)) {
                break;
            }

            int64_t startms = dplog_millis();
            dpret_t err = co_await dpcpp::aio_write_must(stream, line_buf.get());
            if (dpret_iserr(err)) {
                dplog_error("cpp_echo_cet", "Send data with error: %d", err);
                break;
            }

            res.reset(0);
            err = co_await dpcpp::aio_read_until(stream, "\n", res.get());
            if (dpret_iserr(err)) {
                dplog_error("cpp_echo_cet", "Recv data with error: %d", err);
                break;
            }

            fwrite(dpbuf_crdata(res), dpbuf_crsize(res), 1, stdout);
            fprintf(stdout, "(time taken: %ldms)\n\n", dplog_millis() - startms);
            dpbuf_rdata(line_buf, dpbuf_crsize(line_buf));
        }

#if DPAPP_HAS_SSL
        if (cfg.proto == CET_SSL) {
            dpret_t sh_err = co_await dpcpp::ssl_shutdown(stream);
            if (dpret_iserr(sh_err)) {
                dplog_error("cpp_echo_cet", "ssl shutdown error: %d", sh_err);
            }
        }
#endif
#if DPAPP_LSQUIC_ENABLE
        if (cfg.proto == CET_QUIC) {
            dpele_del(stream);
            dpele_del(qconn);
        } else
#endif
        {
            dpele_del(stream);
        }
        dpele_del(owner);

        co_return DPV64_NULL;
    };
    hdrs[0].init_arg1.ptr = &cfg;

    return 1;
}
