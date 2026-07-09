-- dptcp 模块：TCP fd 工厂（对齐 lib/dpapp/dpefd.h）
--
-- accept 经 `dpevp_add(efd, dpskt_accept, dpsockaddr)`；
-- client 经 `dpasc.skt_connect`（dpskt_connect）。
--
local dpret = require("dpret")
local dpasc = require("dpasc")
local ffi = require("ffi")

ffi.cdef([[
const dpele_type_t* dptcp_listen_type();
const dpele_type_t* dptcp_client_type();
const dpele_type_t* dptcp_server_type();

const char* dptcp_addr(dpele_t* efd);
const char* dptcp_peeraddr(dpele_t* efd);
bool dptcp_set_keepalive(dpele_t* efd, int idle, int intvl, int cnt);
int dptcp_errno(dpele_t* efd);
]])

-- Linux SOMAXCONN；listen 未传 backlog 时内核队列过小会导致 tcpkali connect-timeout
local LISTEN_BACKLOG = 4096
pcall(function()
    if ffi.C.SOMAXCONN then
        LISTEN_BACKLOG = tonumber(ffi.C.SOMAXCONN)
    end
end)

local C = ffi.C
local ffi_cast = ffi.cast
local tonumber = tonumber

local DPE_OK = dpret.OK
local DPE_INVAL = dpret.INVAL
local dpret_isok = dpret.isok

local client_type = C.dptcp_client_type()
local listen_type = C.dptcp_listen_type()
local server_type = C.dptcp_server_type()

local M = {
    client_type = client_type,
    listen_type = listen_type,
    server_type = server_type,
}

--- `dpele_new(dptcp_client_type, host, port)` 并 connect。
function M.client(host, port)
    if not host or not port then
        return nil, DPE_INVAL
    end

    host = tostring(host)
    port = ffi_cast("int", tonumber(port))

    local cet = C.dpele_new(client_type, host, port)
    if cet == nil then
        return nil, ffi.errno()
    end

    local ret = dpasc.skt_connect(cet)
    if dpret_isok(ret) then
        return cet, DPE_OK
    else
        C.dpele_del(cet)
        return nil, ret
    end
end

--- `dpele_new(dptcp_listen_type, host, port, backlog)`。
function M.listen(host, port, backlog_)
    if not host or not port then
        return nil, DPE_INVAL
    end
    local backlog = tonumber(backlog_) or LISTEN_BACKLOG
    port = ffi_cast("int", tonumber(port))
    backlog = ffi_cast("int", backlog)
    local lsr = C.dpele_new(listen_type, tostring(host), port, backlog)
    if lsr == nil then
        return nil, ffi.errno()
    else
        return lsr, DPE_OK
    end
end

--- `dpevp_add(efd, dpskt_accept, addr)` → `dpele_new(dptcp_server_type, fd, addr)`。
function M.accept(efd)
    local addr = dpasc.sockaddr()
    local ret = dpasc.skt_accept(efd, addr)
    if not dpret_isok(ret) then
        return nil, ret
    end

    local fd = ffi_cast("int", ret)
    local svr = C.dpele_new(server_type, fd, addr)
    if svr == nil then
        return nil, ffi.errno()
    end

    return svr, DPE_OK
end

--- 本地绑定地址字符串。
function M.addr(efd)
    local r = C.dptcp_addr(efd)
    return r == nil and nil or ffi.string(r)
end

--- 对端地址字符串。
function M.peeraddr(efd)
    local r = C.dptcp_peeraddr(efd)
    return r == nil and nil or ffi.string(r)
end

--- 设置 TCP keepalive（idle/intvl/cnt 秒）。
function M.set_keepalive(efd, idle, intvl, cnt)
    idle = ffi_cast("int", tonumber(idle))
    intvl = ffi_cast("int", tonumber(intvl))
    cnt = ffi_cast("int", tonumber(cnt))
    return C.dptcp_set_keepalive(efd, idle, intvl, cnt)
end

--- 最近一次 socket 错误 errno。
function M.errno(efd)
    return tonumber(C.dptcp_errno(efd))
end

return M
