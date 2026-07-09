-- dpudp 模块：UDP fd 工厂（对齐 lib/dpapp/dpefd.h）
--
local dpret = require("dpret")
local ffi = require("ffi")

ffi.cdef([[
const dpele_type_t* dpudp_server_type();
const dpele_type_t* dpudp_client_type();

const char* dpudp_addr(dpefd_t* efd);
const char* dpudp_peeraddr(dpefd_t* efd);
]])

local C = ffi.C
local ffi_cast = ffi.cast
local ffi_string = ffi.string
local tonumber = tonumber

local DPE_OK = dpret.OK
local DPE_INVAL = dpret.INVAL

local function _cstr(ptr)
    return ptr == nil and nil or ffi_string(ptr)
end

local M = {}

M.server_type = C.dpudp_server_type()
M.client_type = C.dpudp_client_type()

--- `dpele_new(dpudp_server_type, host, port)`。
function M.server(host, port)
    if not host or not port then
        return nil, DPE_INVAL
    end

    port = ffi_cast("int", tonumber(port))
    local efd = C.dpele_new(M.server_type, host, port)
    if efd == nil then
        return nil, ffi.errno()
    end

    return efd, DPE_OK
end

--- `dpele_new(dpudp_client_type, host, port)`。
function M.client(host, port)
    if not host or not port then
        return nil, DPE_INVAL
    end

    port = ffi_cast("int", tonumber(port))
    local efd = C.dpele_new(M.client_type, host, port)
    if efd == nil then
        return nil, ffi.errno()
    end

    return efd, DPE_OK
end

--- 本地绑定地址。
function M.address(efd)
    return efd and _cstr(C.dpudp_addr(efd)) or nil
end

--- 对端地址（connected UDP）。
function M.peeraddr(efd)
    return efd and _cstr(C.dpudp_peeraddr(efd)) or nil
end

return M
