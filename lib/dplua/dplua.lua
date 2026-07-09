-- dplua 运行时入口（协程 + 事件循环）
---@diagnostic disable: inject-field
if os.getenv("LOCAL_LUA_DEBUGGER_VSCODE") == "1" then
    local dtid = tonumber(os.getenv("DPLUA_DEBUG_TYPE_ID"))
    if dtid and dplua.type_id == dtid and dplua.each_ids[dtid][1] == dplua.id then
        require("lldebugger").start()
    end
end

-- Lua 5.1 下为带 __gc 的表提供析构代理
local setmetatableg = setmetatable
function setmetatable(object, metatable)
    if metatable.__gc and _VERSION == "Lua 5.1" then
        object.__gc_proxy = newproxy(true)
        getmetatable(object.__gc_proxy).__gc = function()
            metatable.__gc(object)
        end
    end
    return setmetatableg(object, metatable)
end

package.preload["dplua"] = function()
    return dplua
end

local ffi = require("ffi")

ffi.cdef([[
typedef int dpret_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned short uint16_t;
typedef unsigned long long uint64_t;
typedef long long int64_t;

typedef union {
    uint32_t u32;
    int32_t s32;
    float f32;
    int ret;
    uint32_t opt;
    uint32_t evs;
    uint32_t flags;
    uint8_t bytes[4];
} dpv32_t;

typedef union {
    void* ptr;
    const void* cptr;
    uint64_t u64;
    int64_t s64;
    double f64;
    uint8_t bytes[8];
} dpv64_t;

typedef enum {
    DPROLE_CLIENT = 1,
    DPROLE_SERVER = 2,
    DPROLE_UNSURE = 3,
} dprole_e;

typedef enum {
    DPELE_TYPE_EFD,
    DPELE_TYPE_USD,
    DPELE_TYPE_CTC,
    DPELE_TYPE_TMR,
} dpele_type_e;

typedef enum {
    DPAIO_TYPE_GFD,
    DPAIO_TYPE_SKT,
    DPAIO_TYPE_SSL,
    DPAIO_TYPE_QIC,
} dpaio_type_e;

typedef struct {
    const char* name;
    dpele_type_e type;
    uint32_t size;
    dpaio_type_e iotype;
    uint32_t events;
    dpret_t (*init)(void* udata, va_list vlist);
    dpret_t (*copy)(void* dst, const void* src);
    void (*fini)(void* udata);
} dpele_type_t;

typedef struct dpele dpele_t;
typedef struct dpele dpefd_t;
typedef struct dpele dpctc_t;
typedef struct dpele dptmr_t;
typedef struct dpasc dpasc_t;

dpele_t* dpele_new(const dpele_type_t* type, ...);
dpele_t* dpele_newv(const dpele_type_t* type, va_list args);
dpele_t* dpele_dup(dpele_t* self, bool unuse_);
dpele_t* dpele_ref(dpele_t* self);
uint32_t dpele_refc(dpele_t* self);
void dpele_del(dpele_t* self);
const dpele_type_t* dpele_type(dpele_t* self);
void* dpele_aux_data(dpele_t* self);
void* dpele_asc_data(dpele_t* self);
dpret_t dpele_set_timeout(dpele_t* self, double sec);
double dpele_timeout(dpele_t* self);
dpret_t dpele_ret(dpele_t* self);
void dpele_set_ret(dpele_t* self, dpret_t ret);
dpret_t dpele_set_detach(dpele_t* self, bool detach);
bool dpele_is_detach(dpele_t* self);
bool dpele_is_doing(dpele_t* self);
dpv64_t dpele_cop(dpele_t* self);
bool dpele_wait(dpele_t* self, dpv64_t cop);

const dpele_type_t* dpefd_init_type();
int dpefd_fd(dpele_t* self);
void dpefd_set_close(dpefd_t* self, bool cl);

typedef void (*dpv64_del_f)(dpv64_t);
const dpele_type_t* dpctc_init_type();
dpret_t dpctc_reto(dpctc_t* self, int toid);
int dpctc_fromid(dpctc_t* self);
int dpctc_toid(dpctc_t* self);

const dpele_type_t* dptmr_init_type();

int dpevp_id();
int dpevp_type();
dpret_t dpevp_end(dpele_t* ele, dpret_t ret);
dpele_t* dpevp_pop(int timeout_ms);
dpret_t dpevp_add(dpele_t* ele, const dpasc_t* asc, ...);
dpret_t dplua_add_ctc_submit(dpele_t* ctc, int64_t topic_id, dpv64_t arg);
dpele_t* dplua_new_ctc(int toid, int detach);
dpret_t dplua_add_tmr_timeout(dpele_t* tmr, double sec, int64_t cache_id, dpv64_t arg);
dpret_t dpevp_end_ctc_(dpele_t* ctc, dpret_t err);

const dpasc_t* dptmr_timeout();
]])

dplua.ffi = ffi
local C = ffi.C

if __DPLUA_MAIN__ then
    local init_func, loaderr = loadfile(__ARGS__[0], "bt")
    if not init_func then
        error("Input chunk or file is error: " .. tostring(loaderr))
    end
    local M = init_func()
    if type(M) ~= "table" or type(M.__main__) ~= "function" then
        error("__main__ not found in " .. tostring(__ARGS__[0]))
    end
    return M.__main__(__ARGS__, __HDR__)
end

dplua.ROLE_CLIENT = C.DPROLE_CLIENT
dplua.ROLE_SERVER = C.DPROLE_SERVER
dplua.ROLE_UNSURE = C.DPROLE_UNSURE

dplua.TYPE_EFD = C.DPELE_TYPE_EFD
dplua.TYPE_USD = C.DPELE_TYPE_USD
dplua.TYPE_CTC = C.DPELE_TYPE_CTC
dplua.TYPE_TMR = C.DPELE_TYPE_TMR

dplua.AIO_TYPE_GFD = C.DPAIO_TYPE_GFD
dplua.AIO_TYPE_SKT = C.DPAIO_TYPE_SKT
dplua.AIO_TYPE_SSL = C.DPAIO_TYPE_SSL
dplua.AIO_TYPE_QIC = C.DPAIO_TYPE_QIC

dplua.EVT_NAN = 0x00
dplua.EVT_IN = 0x01
dplua.EVT_PRI = 0x02
dplua.EVT_OUT = 0x04
dplua.EVT_ERR = 0x08
dplua.EVT_HUP = 0x10
dplua.EVT_AIN = 0x03
dplua.EVT_ALL = 0x1F

dplua.SEEK_SET = 0
dplua.SEEK_BEG = 0
dplua.SEEK_CUR = 1
dplua.SEEK_END = 2

local tonumber = tonumber
local coroutine = coroutine
local pcall = pcall
local unpack = unpack
local ffi_cast_int = ffi.cast
local ffi_cast_dpv64 = function(p) return ffi_cast_int("dpv64_t*", p) end
local dpele_asc_data = C.dpele_asc_data

local DPE_OK = 0
local DPE_WAIT = -11
local DPE_TIME = -62
local DPE_INVAL = -22
local DPE_INTERNAL_ERROR = -500
local DPE_INVCMD = -170
local TMR_MAX_AFTER = 4294967296.0
local tmr_timeout_type = C.dptmr_timeout()
local dpv64_null = ffi.new("dpv64_t")

local cache = {}
local cache_idx = 1

local function _cache_take(i)
    i = tonumber(i)
    local v = cache[i]
    cache[i] = nil
    return v
end

local function _cache_save(v)
    cache[cache_idx] = v
    local i = cache_idx
    cache_idx = cache_idx + 1
    return i
end

local dpv64_temp = ffi.new("dpv64_t")

local _app = nil
local _topic_name_to_id = {}
local _topic_id_to_name = {}

local function _topic_is_handler(name)
    return type(name) == 'string' and not name:match("^__")
end

--- 从用户模块表构建 topic 名与稳定 wire id 的双向映射（按名字典序编号）。
local function _topic_build_map(app)
    local names = {}
    for k, v in pairs(app) do
        if _topic_is_handler(k) and type(v) == 'function' then
            names[#names + 1] = k
        end
    end
    table.sort(names)
    _topic_name_to_id = {}
    _topic_id_to_name = {}
    for i, name in ipairs(names) do
        local id = i - 1
        _topic_name_to_id[name] = id
        _topic_id_to_name[id] = name
    end
end

--- 字符串 topic 编码为跨线程 wire id；未注册名返回 nil。
function dplua.topic_id(name)
    if type(name) ~= 'string' then
        return nil
    end
    return _topic_name_to_id[name]
end

--- wire id 解码为 topic 名；未知 id 返回 nil。
function dplua.topic_name(id)
    id = tonumber(id)
    if id == nil then
        return nil
    end
    return _topic_id_to_name[id]
end

local function _cop_index(ele)
    return tonumber(ffi_cast_int("int", C.dpele_cop(ele).s64))
end

--- 挂起当前协程，直到 `ele` 完成；返回 dpele_ret 或 yield 传入值。
function dplua.await(ele, ...)
    local i = _cache_save(coroutine.running())
    dpv64_temp.s64 = i
    if C.dpele_wait(ele, dpv64_temp) then
        return coroutine.yield(...)
    end
    _cache_take(i)
    return C.dpele_ret(ele)
end

--- 中止 `ele` 并以 `ret` 唤醒等待协程（内部 dpevp_end）。
function dplua.cease(ele, ret)
    local co = _cache_take(_cop_index(ele))
    local code = ffi_cast_int("int", tonumber(ret) or 0)
    local r = C.dpevp_end(ele, code)
    if r >= 0 and co then
        coroutine.resume(co, ret)
    end
    return r
end

--- 协程 sleep；`sec<0` 视为最大超时。内部临时 timer，完成后删除。
function dplua.sleep(sec, ...)
    local tmr = C.dpele_new(C.dptmr_init_type())
    if tmr == nil then
        return ffi.errno()
    end

    if sec < 0 then
        sec = TMR_MAX_AFTER
    end

    sec = ffi_cast_int("double", sec)
    local ret = C.dplua_add_tmr_timeout(tmr, sec, 0, dpv64_null)
    if ret ~= DPE_WAIT then
        C.dpele_del(tmr)
        return ret
    end

    ret = dplua.await(tmr, ...)
    C.dpele_del(tmr)
    if ret == DPE_TIME then
        return DPE_OK
    end
    return ret
end

local function _timer(sec, func, ...)
    if type(func) ~= 'function' then
        return DPE_INVAL
    end

    local i = _cache_save({func, {...}})
    local v1 = ffi.new("dpv64_t")
    local tmr = C.dpele_new(C.dptmr_init_type())
    if tmr == nil then
        _cache_take(i)
        return ffi.errno()
    end

    if sec < 0 then
        sec = TMR_MAX_AFTER
    end

    sec = ffi_cast_int("double", sec)
    local ret = C.dplua_add_tmr_timeout(tmr, sec, i, v1)
    if ret ~= DPE_WAIT then
        _cache_take(i)
        C.dpele_del(tmr)
        return ret
    end

    return DPE_OK, tmr
end

--- 一次性 timer；返回 (元素, WAIT) 或 (nil, 错误码)。
function dplua.timer(sec, func, ...)
    local ret, tmr = _timer(sec, func, ...)
    if ret == DPE_OK then
        return tmr, DPE_WAIT
    end
    return nil, ret
end

--- 同 timer，但成功时返回 timer 元素（已 ref，可 cease）。
function dplua.timer_take(sec, func, ...)
    local ret, tmr = _timer(sec, func, ...)
    if ret == DPE_OK then
        C.dpele_ref(tmr)
        return tmr
    else
        return nil, ret
    end
end

--- 异步 syscall：`dpevp_add(ele, asc, ...)`，DPE_WAIT 时 await。
function dplua.aexec(ele, asc, ...)
    local ret = C.dpevp_add(ele, asc, ...)
    if ret == DPE_WAIT then
        ret = dplua.await(ele, ...)
    end
    return ret
end

-- 加载用户模块并启动 init 协程
local dplog = require("dplog")
do
    local init_func, loaderr = nil, nil
    init_func, loaderr = loadfile(__ARGS__[0], "bt")

    if not init_func then
        dplog.alert("dplua", "Input chunk or file is error: %s",
            tostring(loaderr))
        os.exit(1)
    end

    local r, e = pcall(function()
        _app = init_func()
        if type(_app) ~= 'table' then
            error("user module must return a table")
        end
        _topic_build_map(_app)
        local hdr = __HDR__
        local func = hdr and hdr.init and _app[hdr.init]
        if func then
            coroutine.wrap(func)(hdr.args)
        end
    end)
    if not r then
        dplog.alert("dplua", "%s", tostring(e))
        return
    end
end

-- 事件循环：pop → resume 协程 / 执行 CTC·TMR 回调
local _coresume = coroutine.resume
local function resume(co, ...)
    local ok, ret1, ret2 = _coresume(co, ...)
    if not ok then
        dplog.error("dplua", "%s", tostring(ret1))
    end
    return ok, ret1, ret2
end

local function _run_ctc(ctc)
    local asc = C.dpele_asc_data(ctc)
    if asc == nil then
        C.dpevp_end_ctc_(ctc, DPE_INVCMD)
        return
    end
    local ud = ffi_cast_dpv64(asc)
    local topic = _topic_id_to_name[tonumber(ud[0].s64)]
    local fun = topic and _app[topic]
    local ok, ret = nil, DPE_INVCMD
    if type(fun) == 'function' then
        ok, ret = pcall(fun, ctc)
        if not ok then
            dplog.error("dplua", "execute ctc error:%s", tostring(ret))
            ret = -4
        end
    end

    C.dpevp_end_ctc_(ctc, ret)
end

local function _run_tmr(tmr, body)
    local func = body[1]
    local args = body[2]
    local argc = #args

    local ret = 0
    local ok = false
    if argc == 0 then
        ok, ret = pcall(func, tmr)
    elseif argc == 1 then
        ok, ret = pcall(func, tmr, args[1])
    elseif argc == 2 then
        ok, ret = pcall(func, tmr, args[1], args[2])
    elseif argc == 3 then
        ok, ret = pcall(func, tmr, args[1], args[2], args[3])
    else
        local packed = {tmr, unpack(args)}
        ok, ret = pcall(func, unpack(packed))
    end

    C.dpele_set_ret(tmr, ok and ret or DPE_INTERNAL_ERROR)
    C.dpele_del(tmr)
end

local DPELE_TYPE_EFD = C.DPELE_TYPE_EFD
local DPELE_TYPE_CTC = C.DPELE_TYPE_CTC
local DPELE_TYPE_TMR = C.DPELE_TYPE_TMR
local DPELE_TYPE_USD = C.DPELE_TYPE_USD

local ele, ret, co, et = nil, nil, nil, nil

--- 异步恢复等待协程；经 wrap 跳出当前 pop 栈，避免嵌套 resume+yield 饿死后续 timer。
local function _resume_event(ele, ret, et)
    local idx = _cop_index(ele)
    co = _cache_take(idx)
    if not co then
        dplog.warn("dplua", "event type %s without coroutine, cop_idx=%s ret=%d",
            tostring(et), tostring(idx), ret)
        return
    end
    coroutine.wrap(function()
        resume(co, ret)
    end)()
end

while true do
    ele = C.dpevp_pop(-1)
    if ele ~= nil then
        et = tonumber(ffi_cast_int("int", C.dpele_type(ele).type))
        ret = C.dpele_ret(ele)
        if et == DPELE_TYPE_EFD or et == DPELE_TYPE_USD then
            _resume_event(ele, ret, et)
        elseif et == DPELE_TYPE_CTC then
            if C.dpele_is_doing(ele) then
                coroutine.wrap(_run_ctc)(ele)
            else
                _resume_event(ele, ret, et)
            end
        elseif et == DPELE_TYPE_TMR then
            local asc = dpele_asc_data(ele)
            local idx = 0
            if asc ~= nil then
                idx = tonumber(ffi_cast_dpv64(asc)[0].s64) or 0
            end
            if idx ~= 0 then
                local body = _cache_take(idx)
                if body then
                    coroutine.wrap(_run_tmr)(ele, body)
                else
                    _resume_event(ele, ret, et)
                end
            else
                _resume_event(ele, ret, et)
            end
        end
    end
end
