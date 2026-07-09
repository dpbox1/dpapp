-- dpele 模块：事件元素 C API（对齐 lib/dpapp/dpevp.h）
--
local dpret = require("dpret")
local ffi = dplua.ffi
local C = ffi.C
local ffi_cast = ffi.cast
local ffi_new = ffi.new
local tonumber = tonumber

local M = {
    ROLE_SERVER = dplua.ROLE_SERVER,
    ROLE_CLIENT = dplua.ROLE_CLIENT,
    ROLE_UNSURE = dplua.ROLE_UNSURE,

    TYPE_EFD = C.DPELE_TYPE_EFD,
    TYPE_CTC = C.DPELE_TYPE_CTC,
    TYPE_TMR = C.DPELE_TYPE_TMR,
    TYPE_USD = C.DPELE_TYPE_USD,

    AIO_TYPE_GIO = C.DPAIO_TYPE_GFD,
    AIO_TYPE_SKT = C.DPAIO_TYPE_SKT,
    AIO_TYPE_SSL = C.DPAIO_TYPE_SSL,
    AIO_TYPE_QIC = C.DPAIO_TYPE_QIC,

    EVT_NAN = dplua.EVT_NAN,
    EVT_IN = dplua.EVT_IN,
    EVT_PRI = dplua.EVT_PRI,
    EVT_OUT = dplua.EVT_OUT,
    EVT_ERR = dplua.EVT_ERR,
    EVT_HUP = dplua.EVT_HUP,
    EVT_AIN = dplua.EVT_AIN,
    EVT_ALL = dplua.EVT_ALL,

    TMR_MAX_AFTER = 4294967296.0,
}

M.ctc_init_type = C.dpctc_init_type()
M.tmr_init_type = C.dptmr_init_type()
M.efd_init_type = C.dpefd_init_type()

M.evp_id = C.dpevp_id
M.evp_type = C.dpevp_type

local DPE_INVAL = dpret.INVAL
local DPE_OK = dpret.OK
local dpret_iserr = dpret.iserr
local dpv64_null = ffi_new("dpv64_t")

--- 将 nil/数字/cdata/dpv64_t 打包为 dpv64_t（CTC spec 用）。
local function _convert_spec(v)
    if v == nil then
        return nil, DPE_OK
    elseif ffi.istype("dpv64_t", v) then
        return v, DPE_OK
    elseif type(v) == 'number' then
        local spec = ffi_new("dpv64_t")
        spec.s64 = v
        return spec, DPE_OK
    elseif type(v) == 'cdata' then
        local spec = ffi_new("dpv64_t")
        spec.ptr = ffi_cast("void*", v)
        return spec, DPE_OK
    else
        return nil, DPE_INVAL
    end
end

--- 元素 AIO 子类型（DPAIO_TYPE_*）。
function M.aio_type(ele)
    return tonumber(C.dpele_type(ele).iotype)
end

--- `dpele_new(etype, ...)`。
function M.new(etype, ...)
    return C.dpele_new(etype, ...)
end

--- 创建 CTC 元素（回调数据经 `dpctc_submit` 传入）。
function M.new_ctc(toid_, detach_)
    local toid = tonumber(toid_) or 0
    local detach = detach_ and 1 or 0
    return C.dplua_new_ctc(toid, detach)
end

--- 由裸 fd 创建 EFD 元素。
function M.new_efd(fd)
    -- LuaJIT 可变参可能省略整数 0，显式传 cdata
    fd = ffi_cast("int", tonumber(fd) or 0)
    local efd = C.dpele_new(C.dpefd_init_type(), fd)
    if efd == nil then
        return nil, ffi.errno()
    else
        return efd
    end
end

--- 减引用；为 0 时销毁。
function M.del(ele)
    C.dpele_del(ele)
end

--- 是否为 dpele_t cdata。
function M.istype(ele)
    return type(ele) == 'cdata' and ffi.istype("dpele_t", ele)
end

local ELE = {}

--- 减引用；为 0 时销毁。
function ELE.del(self)
    C.dpele_del(self)
end

--- 绑定/解除 GC 析构（`setgc(true)` 时元素出作用域自动 del）。
function ELE.setgc(self, gc_)
    return ffi.gc(self, gc_ and C.dpele_del or nil)
end

--- 复制元素（共享底层资源）。
function ELE.dup(self)
    return C.dpele_dup(self, false)
end

--- 增加引用计数。
function ELE.ref(self)
    C.dpele_ref(self)
    return self
end

--- 当前引用计数。
function ELE.refc(self)
    return C.dpele_refc(self)
end

--- 元素类型描述（dpele_type_t*）。
function ELE.type(self)
    return C.dpele_type(self)
end

--- 元素种类枚举值（DPELE_TYPE_*）。
function ELE.ntype(self)
    return tonumber(C.dpele_type(self).type)
end

--- 类型绑定辅助区（`dpele_type_t::size`）；与 asc scratch 无关。
--- `ctype_` 指定 cast 类型，如 TCP client 可 `"dpsockaddr_t*"`。
function ELE.aux_data(self, ctype_)
    return ffi_cast(ctype_ or "void*", C.dpele_aux_data(self))
end

--- 当前 asc scratch（只读）；与 `aux_data` 是不同内存。
--- `ctype_` 指定 cast 类型。
function ELE.asc_data(self, ctype_)
    return ffi_cast(ctype_ or "void*", C.dpele_asc_data(self))
end

--- 设置操作超时（秒）。
function ELE.set_timeout(self, sec)
    return C.dpele_set_timeout(self, tonumber(sec))
end

--- 当前超时（秒）。
function ELE.timeout(self)
    return tonumber(C.dpele_timeout(self))
end

--- 完成结果码（dpret_t）。
function ELE.ret(self)
    return tonumber(C.dpele_ret(self))
end

--- 设置完成结果码。
function ELE.set_ret(self, ret)
    C.dpele_set_ret(self, tonumber(ret))
end

--- 不关心异步结果（detach）；add 异步成功返回 OK，不 pop 交付。
--- fire-and-forget 推荐 detach 后 dpele_del（while doing）。
function ELE.set_detach(self, detach)
    return C.dpele_set_detach(self, detach == true)
end

--- 是否已设置 detach。
function ELE.is_detach(self)
    return C.dpele_is_detach(self)
end

--- 是否有进行中的异步操作。
function ELE.is_doing(self)
    return C.dpele_is_doing(self)
end

local EFD = {}

--- 底层 fd 编号。
function EFD.fd(self)
    return C.dpefd_fd(self)
end

--- 最近一次 I/O 结果（字节数或错误码）。
function EFD.res(self)
    return tonumber(C.dpele_ret(self))
end

--- 设置 ret 字段。
function EFD.set_res(self, res)
    C.dpele_set_ret(self, tonumber(res))
end

--- 销毁时是否 close(fd)。
function EFD.set_close(self, cl)
    C.dpefd_set_close(self, cl == true)
end

local CTC = {}

--- CTC asc scratch（[0] topic id，[1] arg）；读 `dpele_asc_data`，非 aux_data。
function CTC.asc_data(self)
    return ffi_cast("dpv64_t*", C.dpele_asc_data(self))
end

--- 源 worker id。
function CTC.fromid(self)
    return tonumber(C.dpctc_fromid(self))
end

--- 目标 worker id。
function CTC.toid(self)
    return tonumber(C.dpctc_toid(self))
end

--- CTC 执行中续传（对齐 dpasc.ctc_once：更新 topic/ro_req 后转发）。
function CTC.reto(self, toid, topic_, ro_req_)
    local topic_id = dplua.topic_id(topic_)
    if topic_id == nil then
        return DPE_INVAL
    end

    local ud = CTC.asc_data(self)
    if ud == nil then
        return DPE_INVAL
    end
    ud[0].s64 = topic_id

    if ro_req_ ~= nil then
        local spec, err = _convert_spec(ro_req_)
        if dpret_iserr(err) then
            return err
        end
        if spec then
            ud[1] = spec
        end
    end

    toid = ffi_cast("int", tonumber(toid))
    if toid == nil then
        return DPE_INVAL
    end
    return C.dpctc_reto(self, toid)
end

local TMR = {}

--- timer asc scratch（[0] cache id，[1] arg）；读 `dpele_asc_data`，非 aux_data。
function TMR.asc_data(self)
    return ffi_cast("dpv64_t*", C.dpele_asc_data(self))
end

local MT = {
    [C.DPELE_TYPE_EFD] = EFD,
    [C.DPELE_TYPE_CTC] = CTC,
    [C.DPELE_TYPE_USD] = EFD,
    [C.DPELE_TYPE_TMR] = TMR,
}

ffi.metatype("dpele_t", {
    __index = function(self, k)
        local v = ELE[k]
        if v then
            return v
        end
        local mt = MT[tonumber(ffi.cast("int", C.dpele_type(self).type))]
        if mt then
            return mt[k]
        end
    end,
})

return M
