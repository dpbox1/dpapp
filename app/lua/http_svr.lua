local dplog = require("dplog")
local dpret = require("dpret")
local dptcp = require("dptcp")
local dpbuf = require("dpbuf")
local dpasc = require("dpasc")
local dpele = require("dpele")
local server = require("dpsvr")

local M = {}

local HELLO_RES = table.concat({
    "HTTP/1.1 200 OK", "Server: dpapp-http3-demo", "Content-Type: text/plain",
    "Content-Length: 11", "Connection: keep-alive", "", "Hello World",
}, "\r\n")

local K_HELLO_BODY = "Hello World"

-- 演示证书 CN/SAN 为 h3.dpapp。
-- sni="" 为默认证书槽：客户端 SNI 与已注册域名均不匹配时回退使用该证书；
-- 若不设置默认证书，则 SNI 必须精确匹配已注册域名，否则 TLS 握手失败。
-- HTTP_H3_SNI 供 lsquic http_client -H / curl --resolve 等显式指定主机名时使用。
local HTTP_H3_SNI = "h3.dpapp"

-- HTTP/1.1 请求处理（HTTP 与 HTTPS 监听器共用）
function M.http11_session(peer, start_args)
    local buf = dpbuf.new()
    local err = dpret.OK
    while true do
        err = dpasc.aio_read_until(peer, buf, "\r\n")
        if dpret.iserr(err) then
            break
        end

        while true do
            buf:rseek(0, 2)
            err = dpasc.aio_read_until(peer, buf, "\r\n")
            if dpret.iserr(err) then
                break
            end
            if buf:crsize() == 2 and buf:cbegwith("\r\n", 2) then
                break
            end
        end
        if dpret.iserr(err) then
            break
        end

        buf:reset()
        buf:write(HELLO_RES)
        buf:eseek(0, 2)
        err = dpasc.aio_write_must(peer, buf)
        if dpret.iserr(err) then
            break
        end
    end
    return err
end

-- SSL 处理：先握手，再委托给 http11_session
function M.http11_session_ssl(peer, start_args)
    local dpasc = require("dpasc")
    if not dpret.isok(dpasc.ssl_handshake(peer)) then
        return
    end
    local err = M.http11_session(peer, start_args)
    if not dpret.iserr(err) or err == dpret.EOF then
        dpasc.ssl_shutdown(peer)
    end
end

-- HTTP/3 会话处理：accept 连接、打开流、回写响应
function M.http3_session(conn, start_args)
    local dpqic = require("dpqic")
    local dpasc = require("dpasc")
    if not dpqic.enable() then
        return
    end

    local stream, err = dpasc.qic_stream(conn, false)
    if not stream then
        dplog.error("http3", "http3 open stream failed: %d", err)
        return
    end

    local reqhdr, err = dpasc.qic_recv_hdrset(stream)
    if not reqhdr then
        dplog.error("http3", "http3 recv headers failed: %d", err)
        return
    end

    local reshdr = dpqic.new_res_hdrset(200)
    if reshdr then
        reshdr:set("content-type", "text/plain")
        dpasc.qic_send_hdrset(stream, reshdr)
    end

    err = dpasc.aio_write_must(stream, K_HELLO_BODY)
    dpele.del(stream)
    dpele.del(conn)
    dplog.notice("http3", "http3 stream quit %d", err)
end

function M.__init01(arg)
    if arg.crt and arg.key then
        local dpssl = require("dpssl")
        if dpssl.enable() then
            dpssl.add("http/1.1", dplua.ROLE_SERVER, 0, 0)
            dpssl.add_alpn("http/1.1", "http/1.1")
            dpssl.add_ctx("http/1.1", "", arg.crt, arg.key)
            dpssl.add_ctx("http/1.1", HTTP_H3_SNI, arg.crt, arg.key)

            local dpqic = require("dpqic")
            if dpqic.enable() then
                dpssl.add("h3", dplua.ROLE_SERVER, 0, 0)
                dpssl.add_alpn("h3", "h3")
                dpssl.add_ctx("h3", "", arg.crt, arg.key)
                dpssl.add_ctx("h3", HTTP_H3_SNI, arg.crt, arg.key)
            end
        end
    end
    for _, p in ipairs(arg.params) do
        if type(p.handler) == "string" then
            p.handler = M[p.handler] or _G[p.handler]
        end
    end
    server.start(arg.params)
end

function M.__main__(args, hdrs)
    local cert, key
    if type(args) == "table" and args[1] and args[2] then
        cert, key = args[1], args[2]
    end

    local params = {
        {
            type = "tcp",
            host = "0.0.0.0",
            port = 4480,
            handler = "http11_session",
        },
    }

    if cert and key then
        local dpssl = require("dpssl")
        if dpssl.enable() then
            params[#params + 1] = {
                type = "tcp",
                host = "0.0.0.0",
                port = 4443,
                ssl = "http/1.1",
                handler = "http11_session_ssl",
            }

            local dpqic = require("dpqic")
            if dpqic.enable() then
                params[#params + 1] = {
                    type = "qic",
                    host = "0.0.0.0",
                    port = 4443,
                    ssl = "h3",
                    handler = "http3_session",
                }
            end
        end
    end

    if not cert or not key then
        dplog.warn("http3",
            "Only HTTP enabled; provide <cert> <key> to enable HTTPS and HTTP/3")
    end

    hdrs[1] = {
        init = "__init01",
        args = {
            crt = cert,
            key = key,
            params = params,
        },
    }
    return 2
end

return M
