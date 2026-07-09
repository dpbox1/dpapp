-- dpuds 模块：Unix 域套接字（对齐 lib/dpapp/dpefd.h）
--
local dpret = require("dpret")
local dpasc = require("dpasc")
local ffi = require("ffi")

ffi.cdef([[
const dpele_type_t* dpuds_listen_type();
const dpele_type_t* dpuds_client_type();
const dpele_type_t* dpuds_server_type();
]])

local C = ffi.C
local ffi_cast = ffi.cast

local DPE_OK = dpret.OK
local DPE_INVAL = dpret.INVAL
local dpret_isok = dpret.isok

local client_type = C.dpuds_client_type()
local listen_type = C.dpuds_listen_type()
local server_type = C.dpuds_server_type()

local M = {
    client_type = client_type,
    listen_type = listen_type,
    server_type = server_type,
}

--- `dpele_new(dpuds_client_type, path)` 并 connect。
function M.client(udsfile)
    if not udsfile then
        return nil, DPE_INVAL
    end

    local cet = C.dpele_new(client_type, tostring(udsfile))
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

--- `dpele_new(dpuds_listen_type, path)`。
function M.listen(host, udsfile)
    if not udsfile then
        return nil, DPE_INVAL
    end
    local lsr = C.dpele_new(listen_type, tostring(udsfile))
    if lsr == nil then
        return nil, ffi.errno()
    else
        return lsr, DPE_OK
    end
end

--- accept 并创建 dpuds_server_type 元素。
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

return M
