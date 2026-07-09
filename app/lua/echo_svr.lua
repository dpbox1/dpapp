local dplog = require("dplog")
local dpret = require("dpret")
local dpbuf = require("dpbuf")
local dpasc = require("dpasc")
local dpele = require("dpele")
local dpsvr = require("dpsvr")
local dpqic = require("dpqic")

local M = {}

local ECHO_ALPN = "echo"
local ECHO_SNI = "echo.dpapp"

function M.echo_session(peer, start_args)
    dplog.notice("echo_all", "new connection")
    local buf = dpbuf.new(4096)
    local err = dpret.OK
    while true do
        err = dpasc.aio_read_some(peer, buf)
        if dpret.iserr(err) then
            break
        end
        err = dpasc.aio_write_must(peer, buf)
        if dpret.iserr(err) then
            break
        end
    end
    dplog.notice("echo_all", "session closed: %d", err)
    dpele.del(peer)
end

function M.echo_session_ssl(peer, start_args)
    dplog.notice("echo_all", "new SSL connection")
    local dpasc = require("dpasc")
    local ret = dpasc.ssl_handshake(peer)
    if not dpret.isok(ret) then
        dplog.warn("echo_all", "ssl handshake failed: %d", ret)
        dpele.del(peer)
        return
    end
    local buf = dpbuf.new(4096)
    local err = dpret.OK
    while true do
        err = dpasc.aio_read_some(peer, buf)
        if dpret.iserr(err) then
            break
        end
        err = dpasc.aio_write_must(peer, buf)
        if dpret.iserr(err) then
            break
        end
    end
    if err == dpret.EOF then
        dpasc.ssl_shutdown(peer)
    end
    dplog.notice("echo_all", "SSL session closed: %d", err)
    dpele.del(peer)
end

function M.echo_session_quic(conn, start_args)
    dplog.notice("echo_all", "new QUIC connection")
    if not dpqic.enable() then
        return
    end

    local stream, err = dpasc.qic_stream(conn, false)
    if not stream then
        dplog.notice("echo_all", "QUIC open stream failed: %d", err)
        return
    end

    local buf = dpbuf.new(4096)
    local err = dpret.OK
    while true do
        err = dpasc.aio_read_some(stream, buf)
        if dpret.iserr(err) then
            break
        end
        err = dpasc.aio_write_must(stream, buf)
        if dpret.iserr(err) then
            break
        end
    end
    dplog.notice("echo_all", "QUIC session closed: %d", err)
    dpele.del(stream)
end

function M.__init01(arg)
    if arg.crt and arg.key then
        local dpssl = require("dpssl")
        if dpssl.enable() then
            dpssl.add(ECHO_ALPN, dplua.ROLE_SERVER, 0, 0)
            dpssl.add_alpn(ECHO_ALPN, ECHO_ALPN)
            dpssl.add_ctx(ECHO_ALPN, "", arg.crt, arg.key)
            dpssl.add_ctx(ECHO_ALPN, ECHO_SNI, arg.crt, arg.key)
        end
    end
    for _, p in ipairs(arg.params) do
        if type(p.handler) == "string" then
            p.handler = M[p.handler] or _G[p.handler]
        end
    end
    dpsvr.start(arg.params)
end

function M.__main__(args, hdrs)
    local cert, key = args[1], args[2]
    local params = {{
        type = "tcp",
        host = "127.0.0.1",
        port = 4490,
        handler = "echo_session"
    }}
    if cert and key then
        params[#params + 1] = {
            type = "tcp",
            host = "127.0.0.1",
            port = 4491,
            ssl = ECHO_ALPN,
            handler = "echo_session_ssl"
        }
        if dpqic.enable() then
            params[#params + 1] = {
                type = "qic",
                host = "127.0.0.1",
                port = 4492,
                ssl = ECHO_ALPN,
                handler = "echo_session_quic"
            }
        end
    end
    hdrs[1] = {
        init = "__init01",
        args = {
            alpn = ECHO_ALPN,
            crt = cert,
            key = key,
            params = params
        }
    }
    return 2
end

return M
