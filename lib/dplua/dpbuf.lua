-- yxfangcs<yxfangcs@yeah.net> / 20241127 / 缓冲区模块
--
local ffi = require("ffi")
local bit = require("bit")
local dpret = require("dpret")

ffi.cdef([[
typedef struct dpbuf dpbuf_t;

dpbuf_t* dpbuf_new(int size);
dpbuf_t* dpbuf_new_d(void* data, int size, int mode);
dpbuf_t* dpbuf_new_f(const char* fmt, ...);
dpbuf_t* dpbuf_new_v(const char* fmt, va_list args);
dpbuf_t* dpbuf_new_r(const dpbuf_t* self);
dpbuf_t* dpbuf_dup_r(const dpbuf_t* self);
dpbuf_t* dpbuf_dup_e(const dpbuf_t* self);
void dpbuf_del(dpbuf_t* self);

size_t dpbuf_refc(dpbuf_t* self);
int dpbuf_size(const dpbuf_t* self);
void* dpbuf_data(const dpbuf_t* self);
int dpbuf_utype(const dpbuf_t* self);
void dpbuf_set_utype(dpbuf_t* self, int utype);

int dpbuf_uflag(const dpbuf_t* self, int uflag);
void dpbuf_rmv_uflag(dpbuf_t* self, int uflag);
void dpbuf_add_uflag(dpbuf_t* self, int uflag);

void dpbuf_recycle(dpbuf_t* self, bool force);
void dpbuf_set_recycle(dpbuf_t* self, bool b);
void dpbuf_reset(dpbuf_t* self, int mode);
bool dpbuf_resizew(dpbuf_t* self, int s);
char* dpbuf_crdata(const dpbuf_t* self);
char* dpbuf_cwdata(const dpbuf_t* self);
char* dpbuf_cedata(const dpbuf_t* self);
int dpbuf_crsize(const dpbuf_t* self);
int dpbuf_cwsize(const dpbuf_t* self);
int dpbuf_cesize(const dpbuf_t* self);

bool dpbuf_cempty(const dpbuf_t* self);
bool dpbuf_cequalc(const dpbuf_t* self, const char* other, int len);
int dpbuf_ccmp(const dpbuf_t* self, const dpbuf_t* other);
int dpbuf_cfind(const dpbuf_t* self, const char* match, int len, int left);
int dpbuf_cstrlen(const dpbuf_t* self);
bool dpbuf_cbegwith(const dpbuf_t* self, const char* sub, int len, bool skip_begws);

int dpbuf_rseek(dpbuf_t* self, int offset, int seek);
int dpbuf_wseek(dpbuf_t* self, int offset, int seek);
int dpbuf_eseek(dpbuf_t* self, int offset, int seek);

void dpbuf_cpusr(const dpbuf_t* self, dpbuf_t* dst);

int dpbuf_rws(dpbuf_t* self);
int dpbuf_readto(dpbuf_t* self, dpbuf_t* det, int size);
int dpbuf_rdata(dpbuf_t* self, int len);
int dpbuf_rmust(dpbuf_t* self, int len);
int dpbuf_runtil(dpbuf_t* self, const char* until, int until_sz);
int dpbuf_rcstr(dpbuf_t* self);
int dpbuf_rall(dpbuf_t* self);

int dpbuf_wbuf(dpbuf_t* self, const dpbuf_t* buf, int len);
int dpbuf_wbuf_r(dpbuf_t* self, dpbuf_t* buf, int len);
int dpbuf_wfill(dpbuf_t* self, int len, int8_t v);
int dpbuf_wdata(dpbuf_t* self, const void* data, int len);
int dpbuf_wstrf(dpbuf_t* self, const char* fmt, ...);
int dpbuf_wstrv(dpbuf_t* self, const char* fmt, va_list args);
]])

local C = ffi.C
local _LEN = ffi.new("int[1]")
local select = select
local string_format = string.format

local MF = {}

function MF.setgc(self, use_)
    return ffi.gc(self, use_ and C.dpbuf_del or nil)
end

function MF.view(self)
    return C.dpbuf_new_r(self)
end

function MF.dup_r(self)
    return C.dpbuf_dup_r(self)
end

function MF.dup_e(self)
    return C.dpbuf_dup_e(self)
end

function MF.refc(self)
    return tonumber(C.dpbuf_refc(self))
end

function MF.data(self)
    return C.dpbuf_data(self)
end

function MF.size(self)
    return tonumber(C.dpbuf_size(self))
end

function MF.utype(self)
    return tonumber(C.dpbuf_utype(self))
end

function MF.set_utype(self, utype)
    C.dpbuf_set_utype(self, tonumber(utype))
end

function MF.uflag(self, uflag)
    uflag = tonumber(uflag) or 0xFFFFFF
    return tonumber(C.dpbuf_uflag(self, uflag))
end

function MF.rmv_uflag(self, uflag)
    C.dpbuf_rmv_uflag(self, tonumber(uflag))
end

function MF.add_uflag(self, uflag)
    C.dpbuf_add_uflag(self, tonumber(uflag))
end

function MF.recycle(self, isforce_)
    return C.dpbuf_recycle(self, isforce_ == true)
end

function MF.set_recycle(self, enable)
    return C.dpbuf_set_recycle(self, enable == true)
end

function MF.reset(self, mode_)
    return C.dpbuf_reset(self, mode_ or 0)
end

function MF.resizew(self, sz)
    return C.dpbuf_resizew(self, sz)
end

function MF.crdata(self)
    return C.dpbuf_crdata(self)
end

function MF.cwdata(self)
    return C.dpbuf_cwdata(self)
end

function MF.cedata(self)
    return C.dpbuf_cedata(self)
end

function MF.crsize(self)
    return C.dpbuf_crsize(self)
end

function MF.cwsize(self)
    return C.dpbuf_cwsize(self)
end

function MF.cesize(self)
    return C.dpbuf_cesize(self)
end

function MF.cempty(self)
    return C.dpbuf_cempty(self)
end

function MF.cequal(self, substr)
    return C.dpbuf_cequalc(self, substr, #substr)
end

function MF.cequalc(self, other, len_)
    len_ = tonumber(len_) or (type(other) == 'string' and #other or 0)
    return C.dpbuf_cequalc(self, other, len_)
end

function MF.ccmp(self, other)
    return C.dpbuf_ccmp(self, other)
end

function MF.ref(self)
    return C.dpbuf_new_r(self)
end

function MF.write(self, val, ...)
    local t = type(val)
    if t == 'cdata' and ffi.istype("dpbuf_t", val) then
        return C.dpbuf_wbuf(self, val, tonumber(...) or -1)
    elseif t == 'string' then
        if select("#", ...) == 0 then
            return C.dpbuf_wstrf(self, val)
        end
        return C.dpbuf_wstrf(self, string_format(val, ...))
    elseif t == 'cdata' then
        return C.dpbuf_wdata(self, val, tonumber(...) or 0)
    end
    return dpret.PARAMTYPE
end

function MF.wvar(self, val)
    local sb = require("string.buffer")
    local blob = sb.encode(val)
    return C.dpbuf_wdata(self, blob, #blob)
end

function MF.rvar(self)
    local crsize = C.dpbuf_crsize(self)
    if crsize <= 0 then
        return nil, 0
    end
    local sb = require("string.buffer")
    local lbuf = sb.new()
    lbuf:set(ffi.string(C.dpbuf_crdata(self), crsize))
    local len0 = #lbuf
    local val = lbuf:decode()
    return val, len0 - #lbuf
end

function MF.cfind(self, substr, left_)
    return C.dpbuf_cfind(self, substr, #substr, left_ or 0)
end

function MF.cstrlen(self)
    return C.dpbuf_cstrlen(self)
end

function MF.ccstr(self)
    local len = C.dpbuf_cstrlen(self)
    return len > 0 and ffi.string(C.dpbuf_crdata(self), len) or ""
end

function MF.cstr(self)
    return ffi.string(C.dpbuf_crdata(self), C.dpbuf_crsize(self))
end

function MF.cbegwith(self, sub, len_, skip_begws_)
    local len, skip
    if type(len_) == "boolean" then
        len = #sub
        skip = len_
    else
        len = tonumber(len_) or #sub
        skip = skip_begws_ or false
    end
    return C.dpbuf_cbegwith(self, sub, len, skip)
end

function MF.rseek(self, offset, seek_)
    return C.dpbuf_rseek(self, offset, seek_ or 1)
end

function MF.rws(self)
    return C.dpbuf_rws(self)
end

function MF.readto(self, det, size_)
    return C.dpbuf_readto(self, det, tonumber(size_) or -1)
end

function MF.rdata(self, len_)
    len_ = tonumber(len_) or -1
    return C.dpbuf_rdata(self, len_)
end

function MF.rmust(self, len)
    return C.dpbuf_rmust(self, tonumber(len))
end

function MF.runtil(self, sp_)
    sp_ = sp_ or "\n"
    return C.dpbuf_runtil(self, sp_, #sp_)
end

function MF.rcstr(self)
    return C.dpbuf_rcstr(self)
end

function MF.rall(self)
    return C.dpbuf_rall(self)
end

function MF.wseek(self, offset, seek_)
    return C.dpbuf_wseek(self, offset, seek_ or 1)
end

function MF.wbuf(self, buf, len_)
    return C.dpbuf_wbuf(self, buf, tonumber(len_) or -1)
end

function MF.wbuf_r(self, buf, len_)
    return C.dpbuf_wbuf_r(self, buf, tonumber(len_) or -1)
end

function MF.wfill(self, len, v_)
    if len < 0 then
        return false
    end
    C.dpbuf_wfill(self, len < 0 and C.dpbuf_cwsize(self) or len, v_ or 0)
end

function MF.wdata(self, cptr, len)
    return C.dpbuf_wdata(self, cptr, len)
end

function MF.wstr(self, str, ...)
    if select("#", ...) == 0 then
        return C.dpbuf_wstrf(self, str)
    end
    local s = string_format(str, ...)
    return C.dpbuf_wstrf(self, s)
end

function MF.eseek(self, offset, seek_)
    return C.dpbuf_eseek(self, offset, seek_ or 1)
end

function MF.del(self)
    C.dpbuf_del(self)
end

-- ffi.metatype 的返回值不使用
-- 因为 dpbuf_t 结构体未定义。
local _ = ffi.metatype("dpbuf_t", {
    __index = MF,
    __tostring = MF.cstr,
})

local M = {}

M.CONST_DATA = bit.lshift(1, 1)
M.NO_RECYCLE = bit.lshift(1, 2)

M.FLAG_MASK = 0xF

M.INIT_R = bit.lshift(1, 8)
M.INIT_W = bit.lshift(1, 9)
M.INIT_CR = bit.bor(M.INIT_R, M.CONST_DATA)
M.INIT_CW = bit.bor(M.INIT_W, M.CONST_DATA)
M.DUP_DATA = bit.lshift(1, 10) -- newd 时复制数据

M.UT_TEXT = 0
M.UT_BLOB = 1
M.UT_ERRO = 2
M.UT_JSON = 3
M.UT_USER = 4
M.UF_SHORT = bit.lshift(1, 0)
M.UF_ALL = 0xFFFFFF
M.MAX_SIZE = 0x4fffffff
M.X_SIZE = 65536
M.L_SIZE = 16384
M.M_SIZE = 4096
M.S_SIZE = 1024
M.SEEK_BEG = 0
M.SEEK_SET = 0
M.SEEK_CUR = 1
M.SEEK_END = 2

-- 自动初始化，模式 INIT_W
function M.new(size_)
    return C.dpbuf_new(tonumber(size_) or 0)
end

-- 自动初始化，模式 INIT_R|DUP_DATA
function M.newf(fmt, ...)
    return C.dpbuf_new_f(fmt, ...)
end

--[[ 从 cdata 创建
在 luajit 中，通过 ffi.new/ffi.cast 分配的内存始终由 luajit 管理，
即使通过 ffi.gc 注册了 gc 函数也是如此。
实际上 gc 函数在 cdata 生命周期结束时也会被调用。
例如：
    ffi.new("int")
    ffi.cast("const char*", "hello")
    ffi.cast("int", 10)

因此，如果 cdata 来自 ffi.new 或 ffi.cast，必须添加 CONST_DATA 标志，
防止被 dpbuf 释放。
如果内存通过 ffi.C.malloc 申请，则可以完全由 dpbuf 管理。
--]]
function M.newd(cdata, size, ...)
    if type(cdata) ~= 'cdata' or cdata == nil then
        return nil
    end
    local flag = bit.bor(0, ...)
    return C.dpbuf_new_d(ffi.cast("void*", cdata), size, flag)
end

-- 从字符串创建，初始化模式 CONST_DATA|INIT_R
local _flag_cr = bit.bor(M.CONST_DATA, M.INIT_R)
function M.newc(str)
    if type(str) == 'string' then
        return C.dpbuf_new_d(ffi.cast("void*", str), #str, _flag_cr)
    else
        return nil
    end
end

function M.del(self)
    C.dpbuf_del(self)
end

function M.istype(var)
    return type(var) == 'cdata' and ffi.istype("dpbuf_t", var)
end

function M.valid(var)
    return M.istype(var) and var ~= nil
end

function M.batch_del(tbbuf)
    if type(tbbuf) == 'table' then
        for k, buf in pairs(tbbuf) do
            if M.valid(buf) then
                M.del(buf)
                tbbuf[k] = nil
            end
        end
    end
end

function M.batch_setgc(tbbuf, gc_)
    if type(tbbuf) == 'table' then
        for _, buf in pairs(tbbuf) do
            if M.valid(buf) then
                buf:setgc(gc_)
            end
        end
    end
end

return M
