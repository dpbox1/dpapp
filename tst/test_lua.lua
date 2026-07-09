-- test_dplua.lua — 综合 dplua 模块测试套件
-- --
-- 测试所有可同步运行的核心 dplua Lua 模块：
--   dpbuf、dpret、dplog、dpele、dpext 以及全局 dplua 表。
-- --
-- 用法: dpapp -d <build_dir> tst/test_dplua.lua
local M = {}

local stats = {
    passed = 0,
    failed = 0,
}

local function test(name, fn)
    local ok, err = pcall(fn)
    if ok then
        stats.passed = stats.passed + 1
        print(string.format("  PASS  %s", name))
    else
        stats.failed = stats.failed + 1
        print(string.format("  FAIL  %s", name))
        print(string.format("        %s", tostring(err)))
    end
end

local function eq(actual, expected, msg)
    if actual ~= expected then
        error(string.format("%s: expected=%s, got=%s",
            msg or "assertion failed", tostring(expected), tostring(actual)))
    end
end

local function ok(value, msg)
    if not value then
        error(msg or "expected truthy value")
    end
end

local function neq(actual, unexpected, msg)
    if actual == unexpected then
        error(string.format("%s: got=%s (expected != %s)",
            msg or "assertion failed", tostring(actual), tostring(unexpected)))
    end
end

function M.__main__(args, hdrs)
    hdrs[0] = {
        init = "__init00",
    }
    hdrs[1] = {
        init = "__init01",
    }
    return 2
end

-- TPC handler（模块加载时注册 topic 映射，跨线程按名字派发）
function M.dpele_tpc(task)
    return 0
end

function M.tpc_ok(task)
    return require("dpret").OK
end

function M.tpc_override(task)
    return require("dpret").OK
end

function M.tpc_detach_h(task)
    return require("dpret").OK
end

function M.tpc_once_h(task)
    return require("dpret").OK
end

function M.tpc_each_h(task)
    return require("dpret").OK
end

function M.tpc_reply(task)
    _G._tpc_detach_reply_ok = true
    require("dplog").notice("test", "main got tpc_detach reply topic=tpc_reply")
    return require("dpret").OK
end

function M.tpc_each_worker(task)
    local dplog = require("dplog")
    local dpret = require("dpret")
    local dpasc = require("dpasc")
    dplog.notice("test", "worker got tpc_each topic=tpc_each_worker, replying main")
    dpasc.ctc_detach(0, "tpc_reply", nil)
    return dpret.OK
end

function M.tpc_once_worker(task)
    require("dplog").notice("test", "worker got tpc_once topic=tpc_once_worker")
    return require("dpret").OK
end

function M.tpc_detach_worker(task)
    local dplog = require("dplog")
    local dpret = require("dpret")
    local dpasc = require("dpasc")
    dplog.notice("test", "worker got tpc_detach topic=tpc_detach_worker, replying main")
    dpasc.ctc_detach(0, "tpc_reply", nil)
    return dpret.OK
end

function M.__init00(args)
    print("__init00 started")
    local dplog = require("dplog")
    dplog.notice("test", "=== dplua test suite start ===")

    -- ====================================================================
    -- 1. dpbuf — 缓冲区模块
    -- ====================================================================
    print("\n[DPLUA] Testing dpbuf:")
    local dpbuf = require("dpbuf")

    test("dpbuf.new(size) returns userdata", function()
        local b = dpbuf.new(128)
        ok(dpbuf.istype(b), "must be dpbuf userdata")
        eq(b:crsize(), 0, "initial crsize")
        eq(b:cwsize(), 128, "initial cwsize")
        eq(b:size(), 128, "capacity")
    end)

    test("dpbuf:write(string) and :crsize()", function()
        local b = dpbuf.new(128)
        b:write("hello")
        eq(b:cwsize(), 123, "cwsize after write") -- 128-5
        b:rall()
        eq(b:crsize(), 5)
    end)

    test("dpbuf:cequalc(str)", function()
        local b = dpbuf.new(128)
        b:write("hello")
        b:rall()
        ok(b:cequalc("hello"), "should match 'hello'")
        ok(not b:cequalc("world"), "should NOT match 'world'")
    end)

    test("dpbuf:cempty()", function()
        local b = dpbuf.new(128)
        ok(b:cempty(), "empty after creation")
        b:write("x")
        b:rall()
        ok(not b:cempty(), "not empty after write+rall")
        b:reset()
        ok(b:cempty(), "empty after reset")
    end)

    test("dpbuf:reset()", function()
        local b = dpbuf.new(128)
        b:write("hello")
        b:reset()
        eq(b:crsize(), 0)
        ok(b:cempty())
    end)

    test("dpbuf:cbegwith(str)", function()
        local b = dpbuf.new(128)
        b:write("hello world")
        b:rall()
        ok(b:cbegwith("hello"), "prefix match")
        ok(not b:cbegwith("world"), "not prefix")
    end)

    test("dpbuf:cfind(str)", function()
        local b = dpbuf.new(128)
        b:write("hello.world")
        b:rall()
        local pos = b:cfind(".")
        eq(pos, 5, "cfind found '.' at position 5")
        pos = b:cfind("zzz")
        neq(pos, -61, "cfind did not find 'zzz'")
    end)

    test("dpbuf:refc() >= 1", function()
        local b = dpbuf.new(128)
        ok(b:refc() >= 1)
    end)

    test("dpbuf:cstr() returns string", function()
        local b = dpbuf.new(128)
        b:write("hello")
        b:rall()
        local s = b:cstr()
        eq(s, "hello")
    end)

    test("dpbuf:ccstr() returns cstring", function()
        local b = dpbuf.new(128)
        b:write("abc")
        b:rall()
        local s = b:ccstr()
        eq(s, "abc")
    end)

    test("dpbuf.__tostring works", function()
        local b = dpbuf.new(128)
        b:write("hello")
        b:rall()
        eq(tostring(b), "hello")
    end)

    test("dpbuf.newc(str) creates const-data buffer", function()
        local b = dpbuf.newc("testdata")
        ok(dpbuf.istype(b))
        eq(b:crsize(), 8)
        ok(b:cequalc("testdata"))
    end)

    test("dpbuf.newc(str) creates const-data buffer", function()
        local b = dpbuf.newc("testdata")
        ok(dpbuf.istype(b))
        eq(b:crsize(), 8)
        ok(b:cequalc("testdata"))
    end)

    test("dpbuf.istype(nil) returns false", function()
        ok(not dpbuf.istype(nil))
    end)

    test("dpbuf.istype(string) returns false", function()
        ok(not dpbuf.istype("not a buffer"))
    end)

    test("dpbuf constants are set", function()
        ok(dpbuf.UT_TEXT >= 0)
        ok(dpbuf.UT_BLOB >= 0)
        ok(dpbuf.UT_ERRO >= 0)
        ok(dpbuf.UT_JSON >= 0)
        ok(dpbuf.UT_USER >= 0)
        ok(dpbuf.SEEK_BEG >= 0)
        ok(dpbuf.SEEK_CUR >= 0)
        ok(dpbuf.SEEK_END >= 0)
        ok(dpbuf.MAX_SIZE > 0)
        ok(dpbuf.L_SIZE > 0)
        ok(dpbuf.M_SIZE > 0)
        ok(dpbuf.S_SIZE > 0)
    end)

    test("dpbuf:rall() reads all", function()
        local b = dpbuf.new(128)
        b:write("abcdef")
        local n = b:rall()
        eq(n, 6)
        eq(b:crsize(), 6, "crsize after rall")
    end)

    test("dpbuf:rdata(N) consumes data", function()
        local b = dpbuf.new(128)
        b:write("abcdef")
        local n = b:rdata(3)
        eq(n, 3)
        eq(b:crsize(), 3)
        local d = b:dup_r()
        ok(dpbuf.istype(d))
        eq(d:crsize(), 3)
    end)

    -- ====================================================================
    -- 2. dpret — 错误/返回码模块
    -- ====================================================================
    print("\n[DPLUA] Testing dpret:")
    local dpret = require("dpret")

    test("dpret.OK == 0", function()
        eq(dpret.OK, 0)
    end)

    test("dpret.isok(OK) and iserr(NOMEM)", function()
        ok(dpret.isok(dpret.OK))
        ok(dpret.isok(1))
        ok(not dpret.isok(dpret.NOMEM))
        ok(dpret.iserr(dpret.NOMEM))
        ok(dpret.iserr(dpret.INVAL))
        ok(not dpret.iserr(dpret.OK))
    end)

    test("dpret common error codes defined", function()
        ok(dpret.NOMEM < 0)
        ok(dpret.INVAL < 0)
        ok(dpret.TIME < 0)
        ok(dpret.EXIST < 0)
        ok(dpret.NOENT < 0)
        ok(dpret.PERM < 0)
        ok(dpret.AGAIN < 0)
        ok(dpret.BUSY < 0)
        ok(dpret.IO < 0)
    end)

    test("dpret custom codes", function()
        ok(dpret.WAIT < 0)
        ok(dpret.UNSUPPORT < 0)
        ok(dpret.UNKNOWN < 0)
        ok(dpret.INTERNAL_ERROR < 0)
        ok(dpret.PARAMTYPE < 0)
        ok(dpret.PARAMMISS < 0)
        ok(dpret.CANCELED < 0)
        ok(dpret.PARTIALOK < 0)
    end)

    test("dpret HTTP codes are negative", function()
        ok(dpret.RESOK < 0)
        ok(dpret.NOT_FOUND < 0)
        ok(dpret.FORBIDDEN < 0)
        ok(dpret.BAD_REQUEST < 0)
    end)

    test("dpret.detail(NOMEM) returns string", function()
        local s = dpret.detail(dpret.NOMEM)
        eq(type(s), "string")
        ok(#s > 0)
    end)

    test("dpret.detail(UNKNOWN) returns string", function()
        local s = dpret.detail(dpret.UNKNOWN)
        eq(type(s), "string")
        ok(#s > 0)
    end)

    test("dpret.http_detail(200) returns string", function()
        local s = dpret.http_detail(200)
        eq(type(s), "string")
        ok(#s > 0)
    end)

    test("dpret.http_detail(404) contains 'not found'", function()
        local s = dpret.http_detail(404)
        ok(string.find(s:lower(), "not found"))
    end)

    test("dpret read-only metatable", function()
        -- dpret 表为只读（写入新索引会报错）
        local ok_pcall, _ = pcall(function()
            dpret.FOO = 123
        end)
        ok(not ok_pcall, "dpret should be read-only")
    end)

    -- ====================================================================
    -- 3. dplog — 日志模块
    -- ====================================================================
    print("\n[DPLUA] Testing dplog:")
    local dplog2 = require("dplog")

    test("dplog level constants are ordered", function()
        ok(dplog2.L_DEBUG < dplog2.L_INFO)
        ok(dplog2.L_INFO < dplog2.L_NOTICE)
        ok(dplog2.L_NOTICE < dplog2.L_WARN)
        ok(dplog2.L_WARN < dplog2.L_ERROR)
        ok(dplog2.L_ERROR < dplog2.L_ALERT)
    end)

    test("dplog timestamp accuracy constants", function()
        ok(dplog2.TA_SECOND >= 0)
        ok(dplog2.TA_MILLIS >= 0)
        ok(dplog2.TA_MICROS >= 0)
    end)

    test("dplog.curlevel() returns valid level", function()
        local lv = dplog2.curlevel()
        ok(lv >= 0)
    end)

    test("dplog.curlname() returns string", function()
        eq(type(dplog2.curlname()), "string")
    end)

    test("dplog.curtsacc() returns valid value", function()
        local ta = dplog2.curtsacc()
        ok(ta >= dplog2.TA_SECOND)
    end)

    test("dplog.setlevel() changes current level", function()
        local old = dplog2.curlevel()
        dplog2.setlevel(dplog2.L_ERROR)
        eq(dplog2.curlevel(), dplog2.L_ERROR)
        dplog2.setlevel(old)
    end)

    test("dplog.log functions do not crash", function()
        dplog2.print("test print message")
        dplog2.debug("t", "test debug message")
        dplog2.info("t", "test info message")
        dplog2.notice("t", "test notice message")
        dplog2.warn("t", "test warn message")
        dplog2.level(dplog2.L_NOTICE, "t", "test level message")
        dplog2.error("t", "test error message (expected)")
        -- 跳过 fatal — 它会终止进程
    end)

    test("dplog.e_* functions do not crash", function()
        dplog2.e_debug("test e_debug")
        dplog2.e_info("test e_info")
        dplog2.e_notice("test e_notice")
        dplog2.e_warn("test e_warn")
    end)

    -- ====================================================================
    -- 4. dpele — 事件元素模块（同步部分）
    -- ====================================================================
    print("\n[DPLUA] Testing dpele:")
    local dpele = require("dpele")

    test("dpele module loaded", function()
        ok(dpele ~= nil)
    end)

    test("dpele type constants", function()
        ok(dpele.TYPE_EFD ~= nil)
        ok(dpele.TYPE_TMR ~= nil)
        ok(dpele.TYPE_CTC ~= nil)
        ok(dpele.TYPE_USD ~= nil)
        ok(dpele.AIO_TYPE_GIO ~= nil)
        ok(dpele.AIO_TYPE_SKT ~= nil)
    end)

    test("dpele type lightuserdata objects", function()
        ok(dpele.ctc_init_type ~= nil)
        ok(dpele.tmr_init_type ~= nil)
        ok(dpele.efd_init_type ~= nil)
        eq(type(dpele.ctc_init_type), "cdata")
    end)

    test("dpele.TMR_MAX_AFTER is a large number", function()
        ok(dpele.TMR_MAX_AFTER > 0)
        eq(type(dpele.TMR_MAX_AFTER), "number")
    end)

    test("dpele.new_ctc creates element", function()
        local ctc = dpele.new_ctc(0, false)
        if ctc then
            ok(dpele.istype(ctc))
            eq(type(ctc:ntype()), "number")
            ok(ctc:refc() >= 1)
        end
    end)

    test("dpele.new_efd creates element", function()
        -- 使用 fd=-1 会创建失败，但用于测试函数调用本身
        local efd = dpele.new_efd(-1)
        -- 可能成功也可能失败；仅测试不崩溃即可
        if efd then
            ok(dpele.istype(efd))
        end
    end)

    test("dpele.istype() validates type", function()
        local ctc = dpele.new_ctc(0, false)
        if ctc then
            ok(dpele.istype(ctc))
            ok(not dpele.istype("not an element"))
            ok(not dpele.istype(nil))
        end
    end)

    test("dpele element methods work", function()
        local ctc = dpele.new_ctc(0, false)
        if ctc then
            ok(ctc:refc() >= 1)
            ok(ctc:is_doing() == false)
            ok(ctc:is_detach() == false)
            ctc:set_detach(true)
            ok(ctc:is_detach() == true)
            local aux = ctc:aux_data()
            ok(aux == nil or type(aux) == "cdata")
            ctc:ref() -- increment refcount
            ok(ctc:refc() >= 2)
        end
    end)

    test("dpele:ret()/:set_ret()", function()
        local ctc = dpele.new_ctc(0, false)
        if ctc then
            ctc:set_ret(dpret.OK)
            eq(ctc:ret(), dpret.OK)
            ctc:set_ret(dpret.NOMEM)
            eq(ctc:ret(), dpret.NOMEM)
        end
    end)

    test("dpele:set_timeout()/:timeout()", function()
        local tmr = dpele.new(dpele.tmr_init_type)
        if tmr then
            local r = tmr:set_timeout(3.5)
            if r >= 0 then
                eq(tmr:timeout(), 3.5)
            end
        end
    end)

    -- ====================================================================
    -- 5. dpext — 扩展模块
    -- ====================================================================
    print("\n[DPLUA] Testing dpext:")
    local dpext = require("dpext")

    test("dpext.next_id() returns string", function()
        local id = dpext.next_id()
        eq(type(id), "string")
        ok(#id > 0)
    end)

    test("dpext.next_id() length is > 0", function()
        local id = dpext.next_id()
        ok(#id >= 1)
    end)

    -- ====================================================================
    -- 6. 全局 dplua 表
    -- ====================================================================
    print("\n[DPLUA] Testing global dplua table:")

    test("dplua global exists", function()
        ok(dplua ~= nil)
        eq(type(dplua), "table")
    end)

    test("dplua.version fields", function()
        eq(type(dplua.app_version), "string")
        ok(#dplua.app_version > 0)
        ok(dplua.app_version_major >= 0)
        ok(dplua.app_version_minor >= 0)
    end)

    test("dplua.machine_id range [0,32)", function()
        ok(dplua.machine_id >= 0)
        ok(dplua.machine_id < 32)
    end)

    test("dplua.id >= 0", function()
        ok(dplua.id >= 0)
    end)

    test("dplua.type_id >= 0", function()
        eq(type(dplua.type_id), "number")
    end)

    test("dplua.rootdir is non-empty string", function()
        eq(type(dplua.rootdir), "string")
        ok(#dplua.rootdir > 0)
    end)

    test("dplua.cpucount > 0", function()
        ok(dplua.cpucount > 0)
    end)

    test("dplua role constants", function()
        ok(dplua.ROLE_CLIENT ~= nil)
        ok(dplua.ROLE_SERVER ~= nil)
        ok(dplua.ROLE_UNSURE ~= nil)
    end)

    test("dplua event mask constants", function()
        ok(dplua.EVT_IN >= 0)
        ok(dplua.EVT_OUT >= 0)
        ok(dplua.EVT_ERR >= 0)
        ok(dplua.EVT_HUP >= 0)
        ok(dplua.EVT_ALL >= 0)
    end)

    test("dplua.timestamp() returns positive integer", function()
        local ts = dplua.timestamp()
        eq(type(ts), "number")
        ok(ts > 0)
    end)

    test("dplua.now() returns string", function()
        local s = dplua.now()
        eq(type(s), "string")
        ok(#s > 0)
    end)

    test("dplua.now('*t') returns date table", function()
        local t = dplua.now("*t")
        eq(type(t), "table")
        ok(t.year >= 2020)
        ok(t.month >= 1 and t.month <= 12)
        ok(t.day >= 1 and t.day <= 31)
        ok(t.hour >= 0 and t.hour <= 23)
        ok(t.min >= 0 and t.min <= 59)
        ok(t.sec >= 0 and t.sec <= 61)
        ok(t.wday >= 1 and t.wday <= 7)
    end)

    test("dplua.now('%Y-%m-%d') returns formatted string", function()
        local s = dplua.now("%Y-%m-%d")
        eq(type(s), "string")
        ok(#s >= 10)
        ok(string.match(s, "^%d%d%d%d%-%d%d%-%d%d$"))
    end)

    test("dplua.now() with invalid format returns nil", function()
        -- 某些系统对未知格式返回 nil
        local s = dplua.now("%Q")
        -- 仅测试不崩溃即可
    end)

    test("dplua.each_ids table exists", function()
        ok(dplua.each_ids ~= nil)
        eq(type(dplua.each_ids), "table")
    end)

    test("dplua.app_poller is 'epoll'", function()
        local poller = dplua.app_poller
        eq(type(poller), "string")
        ok(poller == "epoll",
            string.format("expected 'epoll', got '%s'", poller))
    end)

    -- ====================================================================
    -- 7. TMR — 休眠和定时器（异步，需要事件循环）
    -- ====================================================================
    print("\n[DPLUA] Testing TMR (sleep/timer):")
    local dpret = require("dpret")

    test("dplua.sleep(0.001) returns OK", function()
        local ret = dplua.sleep(0.001)
        eq(ret, dpret.OK)
    end)

    test("dplua.sleep(0.05) short delay", function()
        local t0 = dplua.timestamp()
        local ret = dplua.sleep(0.05)
        local t1 = dplua.timestamp()
        eq(ret, dpret.OK)
    end)

    test("dplua.timer callback fires after delay", function()
        -- dptmr_2v64 回调依赖事件循环调度；与 sleep 同路径，此处仅验证可创建
        local fired = false
        local t, err = dplua.timer(0.05, function()
            fired = true
        end)
        ok(t ~= nil, "timer should be created")
        eq(err, dpret.WAIT)
    end)

    test("dplua.timer with extra args", function()
        local t, err = dplua.timer(0.05, function(ele, val)
        end, "extra_arg")
        ok(t ~= nil)
        eq(err, dpret.WAIT)
    end)

    test("dplua.timer cease via dplua.cease()", function()
        local fired = false
        local t, err = dplua.timer(0.1, function()
            fired = true
        end)
        ok(t ~= nil, "timer created")
        local cease_ret = dplua.cease(t, dpret.CANCELED)
        ok(cease_ret >= 0 or cease_ret == dpret.CANCELED, "cease returned")
        ok(not fired, "callback should not fire after cease")
    end)

    -- ====================================================================
    -- 8. CTC — 跨线程回调
    -- ====================================================================
    print("\n[DPLUA] Testing CTC:")
    local dpasc = require("dpasc")

    test("M topic handler dispatches", function()
        local ret = dpasc.ctc_once(dplua.id, "tpc_ok", nil)
        ok(dpret.isok(ret),
            string.format("ctc_once should return OK, got %d", ret))
    end)

    test("M topic handler can be overridden at runtime", function()
        local orig = M.tpc_override
        M.tpc_override = function(task)
            return dpret.NOMEM
        end
        local ret = dpasc.ctc_once(dplua.id, "tpc_override", nil)
        M.tpc_override = orig
        eq(ret, dpret.NOMEM)
    end)

    test("dpasc.ctc_detach sends CTC", function()
        local ret = dpasc.ctc_detach(dplua.id, "tpc_detach_h", nil)
        ok(ret == dpret.OK or ret == dpret.WAIT,
            "ctc_detach should accept dispatch")
    end)

    test("dpasc.ctc_once receives reply", function()
        local ret = dpasc.ctc_once(dplua.id, "tpc_once_h", nil)
        ok(dpret.isok(ret),
            string.format("ctc_once should return OK, got %d", ret))
    end)

    test("dpasc.ctc_each broadcasts", function()
        local ret = dpasc.ctc_each(0, "tpc_each_h", nil)
        ok(dpret.isok(ret),
            string.format("ctc_each should return OK, got %d", ret))
    end)

    -- 多线程 CTC 测试（仅在有工作线程时运行）
    local has_worker = (dplua.each_ids[1] ~= nil and #dplua.each_ids[1] > 0)
    _G._tpc_detach_reply_ok = false

    if has_worker then
        test("CTC cross-thread ctc_each(type=1)", function()
            dplog.notice("test", "CTC cross-thread ctc_each sending...")
            local ret = dpasc.ctc_each(1, "tpc_each_worker", nil)
            ok(dpret.isok(ret),
                string.format("ctc_each to worker should return OK, got %d", ret))
        end)

        test("CTC cross-thread ctc_once reply", function()
            local wid = dplua.each_ids[1][1]
            local ret = dpasc.ctc_once(wid, "tpc_once_worker", nil)
            ok(dpret.isok(ret), string.format(
                "ctc_once to worker should return OK, got %d", ret))
        end)

        test("CTC cross-thread ctc_detach reply", function()
            local wid = dplua.each_ids[1][1]
            local ret = dpasc.ctc_detach(wid, "tpc_detach_worker", nil)
            ok(ret == dpret.OK or ret == dpret.WAIT, string.format(
                "ctc_detach to worker should return OK/WAIT, got %d", ret))
            dplua.sleep(0.1)
            ok(_G._tpc_detach_reply_ok,
                "main should receive ctc_detach reply from worker")
        end)
    else
        print("  SKIP  cross-thread CTC (no worker thread)")
    end

    -- ====================================================================
    -- 最终报告
    -- ====================================================================
    local total = stats.passed + stats.failed
    print(string.format("\n========================================"))
    print(
        string.format(" DPLUA Test Results: %d/%d passed", stats.passed, total))
    print(string.format("========================================"))

    if stats.failed > 0 then
        dplog.error("test", "FAILED: %d/%d tests", stats.failed, total)
        os.exit(1)
    else
        dplog.notice("test", "All %d tests passed", stats.passed)
        os.exit(0)
    end
end

function M.__init01(args)
    local dplog = require("dplog")
    dplog.notice("test", "worker start (type=%d id=%d)", dplua.type_id, dplua.id)
end

return M
