local dpbuf = require("dpbuf")
local dptcp = require("dptcp")
local dplog = require("dplog")
local dpret = require("dpret")
local dpasc = require("dpasc")
local dpele = require("dpele")

local M = {}

local ECHO_ALPN = "echo"
local ECHO_SNI = "echo.dpapp"

function M.__init00(arg)
    local proto = arg.proto
    local host = arg.host
    local port = arg.port

    -- SSL init before connect（对齐 cwc）
    if proto == "ssl" or proto == "qic" then
        local dpssl = require("dpssl")
        if not dpssl.enable() then
            dplog.error("echo_all", "ssl disabled")
            os.exit(1)
        end
        dpssl.add(ECHO_ALPN, dplua.ROLE_CLIENT, 0, 0)
        dpssl.add_alpn(ECHO_ALPN, ECHO_ALPN)
    end

    local cet, err, stream

    if proto == "tcp" then
        cet, err = dptcp.client(host, port)
        if not cet then
            dplog.error("echo_all", "connect failed(%d): %s:%d", err, host, port)
            os.exit(1)
        end
        stream = cet

    elseif proto == "ssl" then
        cet, err = dptcp.client(host, port)
        if not cet then
            dplog.error("echo_all", "connect failed(%d): %s:%d", err, host, port)
            os.exit(1)
        end
        local dpssl = require("dpssl")
        local dpasc = require("dpasc")
        local ssn = dpssl.client(cet, ECHO_ALPN, ECHO_SNI)
        if not ssn then
            dplog.error("echo_all", "ssl ssn failed")
            os.exit(1)
        end
        cet = ssn
        if not dpret.isok(dpasc.ssl_handshake(cet)) then
            dplog.error("echo_all", "ssl handshake failed")
            os.exit(1)
        end
        stream = cet

    elseif proto == "qic" then
        local dpqic = require("dpqic")
        local dpasc = require("dpasc")
        if not dpqic.enable() then
            dplog.error("echo_all", "quic disabled")
            os.exit(1)
        end
        dpqic.add_engine(ECHO_ALPN, nil)
        cet, err = dpqic.client(host, port, ECHO_ALPN)
        if not cet then
            dplog.error("echo_all", "quic client failed(%d): %s:%d", err, host, port)
            os.exit(1)
        end
        local conn
        conn, err = dpasc.qic_connect(cet, ECHO_SNI, nil)
        if not conn then
            dplog.error("echo_all", "quic conect failed(%d)", err)
            os.exit(1)
        end
        local stm
        stm, err = dpasc.qic_stream(conn, true)
        if not stm then
            dplog.error("echo_all", "quic stream failed(%d)", err)
            os.exit(1)
        end
        stream = stm

    else
        dplog.error("echo_all", "unknown protocol: %s", proto)
        os.exit(1)
    end

    if not stream then
        dplog.error("echo_all", "connect failed(%d): %s:%d", err, host, port)
        os.exit(1)
    end

    dplog.notice("echo_all", "connected to %s:%d", host, port)

    -- 从 stdin 逐行读取，发送到服务器，接收回声并打印（对齐 cwc）
    local line_efd = dpele.new_efd(0)
    line_efd:set_close(false)
    local line_buf = dpbuf.new(0)
    local res = dpbuf.new(0)

    while true do
        line_buf:reset()
        local line_ret = dpasc.aio_read_until(line_efd, line_buf, "\n")
        if line_ret <= 0 or line_buf:cbegwith("\\q", 2, true) then
            break
        end

        local startms = dplua.timestamp(dplog.TA_MILLIS)
        err = dpasc.aio_write_must(stream, line_buf)
        if dpret.iserr(err) then
            dplog.error("echo_all", "send error: %d", err)
            break
        end

        res:reset()
        err = dpasc.aio_read_until(stream, res, "\n")
        if dpret.iserr(err) then
            dplog.error("echo_all", "recv error: %d", err)
            break
        end

        io.write(res:cstr())
        res:reset()
        io.write(string.format("(time taken: %dms)\n\n",
            dplua.timestamp(dplog.TA_MILLIS) - startms))
    end

    if proto == "ssl" then
        local dpasc = require("dpasc")
        dpasc.ssl_shutdown(stream)
    end
end

function M.__main__(args, hdrs)
    local proto = "tcp"
    local host = "127.0.0.1"
    local port

    if type(args) == "table" then
        proto = tostring(args[1] or proto)
        host = tostring(args[2] or host)
        port = tonumber(args[3])
    end

    if not port then
        if proto == "tcp" then
            port = 4490
        elseif proto == "ssl" then
            port = 4491
        elseif proto == "qic" then
            port = 4492
        else
            dplog.error("echo_all", "unknown protocol: %s", proto)
            os.exit(1)
        end
    end

    hdrs[0] = {
        init = "__init00",
        args = {
            proto = proto,
            host = host,
            port = port
        }
    }
    return 1
end

return M
