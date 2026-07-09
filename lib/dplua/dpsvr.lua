-- dpsvr：TCP/UDS/QUIC 多 listener 服务端框架
--
-- start(params) 的 params 为 listener 配置数组，每项字段：
--
--   type / protocol  "tcp" | "uds" | "qic"（默认 "tcp"）
--   host             绑定地址；uds 为 socket 路径
--   port             tcp / qic 端口（uds 忽略）
--   handler          function(peer, start_args)，每连接在新协程中调用
--   start_args       可选，原样传给 handler 第二参数
--   ssl              可选字符串，dpssl 组名；省略表示不使用 SSL
--                    qic 必填，兼作 ALPN / engine 名
--
-- dpssl 组（add / add_alpn / add_ctx）由应用在 __init 中事先注册。
--
-- handler 约定：
--   tcp / uds  → peer 为 dpefd（SSL 时 server 框架已 upgrade 为 ssl ele）
--   qic        → conn 为 quic 连接 ele，handler 内自行 qic_stream
local dplog = require("dplog")
local dpret = require("dpret")
local dptcp = require("dptcp")
local dpuds = require("dpuds")
local dpqic = require("dpqic")
local dpasc = require("dpasc")

local dpret_iserr = dpret.iserr

local M = {}

local function _tcp_accept_loop(listener, handler, ssl_group, start_args)
    while true do
        local peer, err = dptcp.accept(listener)
        if not peer then
            dplog.warn("server", "tcp accept error: %d", err)
            break
        end

        if ssl_group then
            local dpssl = require("dpssl")
            local ssn = dpssl.server(peer, ssl_group)
            if ssn then
                peer = ssn
            end
        end

        coroutine.wrap(handler)(peer, start_args)
    end
end

local function _uds_accept_loop(listener, handler, start_args)
    while true do
        local peer, err = dpuds.accept(listener)
        if not peer then
            dplog.warn("server", "uds accept error: %d", err)
            break
        end

        coroutine.wrap(handler)(peer, start_args)
    end
end

local function _qic_accept_loop(listener, handler, start_args)
    while true do
        local conn, err = dpasc.qic_accept(listener)
        if not conn then
            dplog.warn("server", "quic accept error: %d", err)
            break
        end

        coroutine.wrap(handler)(conn, start_args)
    end
end

local function _start_tcp(p)
    local ssl_group = p.ssl and tostring(p.ssl) or nil
    if ssl_group == "" then
        ssl_group = nil
    end

    local listener, err = dptcp.listen(p.host, p.port)
    if not listener then
        dplog.error("server", "tcp listen failed(%d): %s:%d", err, p.host, p.port)
        return
    end

    dplog.notice("server", "tcp server(ssl:%s) started on %s:%d",
        ssl_group or "false", p.host, p.port)

    coroutine.wrap(_tcp_accept_loop)(listener, p.handler, ssl_group, p.start_args)
end

local function _start_uds(p)
    local listener, err = dpuds.listen(p.host)
    if not listener then
        dplog.error("server", "uds listen failed(%d): %s", err, p.host)
        return
    end

    dplog.notice("server", "uds server started on %s", p.host)

    coroutine.wrap(_uds_accept_loop)(listener, p.handler, p.start_args)
end

local function _start_qic(p)
    if not dpqic or not dpqic.enable() then
        dplog.error("server", "quic disabled at build time")
        return
    end

    local group = p.ssl and tostring(p.ssl) or nil
    if not group or group == "" then
        dplog.error("server", "qic listener requires ssl group")
        return
    end

    local ret = dpqic.add_engine(group, nil)
    if dpret_iserr(ret) then
        dplog.error("server", "quic engine add failed(%d): %s", ret, group)
        return
    end

    local listener, err = dpqic.listen(group, p.host, p.port)
    if not listener then
        dplog.error("server", "quic listen failed(%d): %s:%d/%s", err, p.host,
            p.port, group)
        return
    end

    dplog.notice("server", "quic server started on %s:%d/%s", p.host, p.port, group)

    coroutine.wrap(_qic_accept_loop)(listener, p.handler, p.start_args)
end

--- 启动全部 listener（各 listener 在独立协程中 accept 循环）。
--- @param params table[] listener 配置列表，字段见文件头注释
function M.start(params)
    for _, p in ipairs(params) do
        local t = p.type or p.protocol or "tcp"
        if t == "tcp" then
            coroutine.wrap(_start_tcp)(p)
        elseif t == "uds" then
            coroutine.wrap(_start_uds)(p)
        elseif t == "qic" then
            coroutine.wrap(_start_qic)(p)
        else
            dplog.error("server", "unknown type: %s", t)
        end
    end
end

return M
