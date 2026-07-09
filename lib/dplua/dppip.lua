-- 命名管道（FIFO），通过 dpefd / dpele_new(dppip_type(), path, evmask, unlink_on_del) 创建
--
local dpret = require("dpret")
local ffi = require("ffi")

ffi.cdef([[
const dpele_type_t* dppip_type();
const char* dppip_path(dpefd_t* efd);
]])

local C = ffi.C
local ffi_cast = ffi.cast

local DPE_OK = dpret.OK
local DPE_INVAL = dpret.INVAL

local M = {}

M.type = C.dppip_type()

--- 打开一个 FIFO 作为可轮询的事件元素。
--- @param fifo_path string 管道路径（必要时通过 mkfifo 创建）
--- @param evmask_ number 可选；默认 dplua.EVT_IN（读）或使用 dplua.EVT_OUT（写）
--- @param unlink_on_del_ 为 true 时，元素删除时 unlink 路径（默认 false）
--- @return ele, err
function M.open(fifo_path, evmask_, unlink_on_del_)
    if not fifo_path or type(fifo_path) ~= 'string' then
        return nil, DPE_INVAL
    end

    local evmask = ffi_cast("int", tonumber(evmask_) or dplua.EVT_IN)
    local unlink_on_del = ffi_cast("int", unlink_on_del_ == true and 1 or 0)
    local ele = C.dpele_new(M.type, fifo_path, evmask, unlink_on_del)
    if ele == nil then
        return nil, ffi.errno()
    end
    return ele, DPE_OK
end

function M.path(efd)
    if not efd then
        return nil
    end
    local p = C.dppip_path(efd)
    if p == nil then
        return nil
    end
    return ffi.string(p)
end

return M
