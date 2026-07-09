-- test_fsm_sig_pip.lua — fsm / sig / pip 模块测试（dplua 绑定）
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

function M.__main__(args, hdrs)
    hdrs[0] = { init = "__init00" }
    hdrs[1] = { init = "__init01" }
    return 2
end

function M.__init01(args)
    local dplog = require("dplog")
    dplog.notice("test", "worker ready (type=%d id=%d)", dplua.type_id, dplua.id)
end

function M.pip_writer(task)
    local ffi = require("ffi")
    ffi.cdef("void* dpele_asc_data(dpele_t* self);")
    ffi.cdef("int open(const char *pathname, int flags);")
    ffi.cdef("ssize_t write(int fd, const void *buf, size_t count);")
    ffi.cdef("int close(int fd);")

    local C = ffi.C
    local asc = C.dpele_asc_data(task)
    if asc == nil then
        error("pip_writer: ctc asc_data missing")
    end
    local ud = ffi.cast("dpv64_t*", asc)
    local path = ffi.string(ffi.cast("char*", ud[1].ptr))
    local O_WRONLY = 1
    local wr = ffi.C.open(path, O_WRONLY)
    if wr < 0 then
        error(string.format("pip writer open failed: %d", ffi.errno()))
    end
    local msg = ffi.new("char[3]", "pip")
    ffi.C.write(wr, msg, 3)
    ffi.C.close(wr)
    return require("dpret").OK
end

function M.__init00(args)
    local dpret = require("dpret")
    local dplog = require("dplog")
    local dpasc = require("dpasc")
    local dpsig = require("dpsig")
    local dppip = require("dppip")
    local dpele = require("dpele")
    local ffi = require("ffi")
    local bit = require("bit")

    ffi.cdef([[
const dpele_type_t* dpfsm_type();
dpret_t dpfsm_addev(dpefd_t* self, const char* path, uint32_t mask);
dpret_t dpfsm_delev(dpefd_t* self, int wd);
struct inotify_event {
    int wd;
    uint32_t mask;
    uint32_t cookie;
    uint32_t len;
    char name[256];
};
int mkfifo(const char *pathname, unsigned mode);
ssize_t read(int fd, void *buf, size_t count);
int raise(int sig);
]])

    local C = ffi.C
    local IN_CREATE = 0x00000100

    dplua.sleep(0.001)

    print("\n[DPLUA] Testing FSM / SIG / PIP:")

    local tmp = os.getenv("TMPDIR") or "/tmp"
    local tag = string.format("%d_%d", dplua.type_id, dplua.id)

    test("dppip.open rejects invalid ev", function()
        local path = string.format("%s/dpapp_lua_pip_bad_%s", tmp, tag)
        os.remove(path)
        local fd = io.open(path, "w")
        fd:write("x")
        fd:close()
        local efd, err = dppip.open(path, bit.bor(dplua.EVT_IN, dplua.EVT_OUT), false)
        ok(efd == nil, "should reject invalid ev mask")
        os.remove(path)
    end)

    test("dppip async read via FIFO", function()
        local path = string.format("%s/dpapp_lua_pip_%s", tmp, tag)
        os.remove(path)
        eq(C.mkfifo(path, 438), 0) -- 0666

        local cmd = string.format("(sleep 0.05; printf pip > %q) >/dev/null 2>&1 &", path)
        eq(os.execute(cmd), 0)

        local efd, err = dppip.open(path, dplua.EVT_IN, true)
        ok(efd ~= nil, string.format("pip reader open failed: %s", tostring(err)))

        dplua.sleep(0.06)

        local buf = ffi.new("char[8]")
        local n = C.read(efd:fd(), buf, 3)
        ok(n == 3 and ffi.string(buf, 3) == "pip",
            string.format("pip read failed: %s", tostring(n)))
        eq(dppip.path(efd), path, "pip path")
        dpele.del(efd)
    end)

    test("dpasc.ctc_detach pip_writer", function()
        local wid = dplua.each_ids[1] and dplua.each_ids[1][1]
        ok(wid ~= nil, "type1 worker required")

        local path = string.format("%s/dpapp_lua_pip_ctc_%s", tmp, tag)
        os.remove(path)
        eq(C.mkfifo(path, 438), 0)

        local path_cstr = ffi.new("char[?]", #path + 1, path)
        local ret = dpasc.ctc_detach(wid, "pip_writer", path_cstr)
        ok(dpret.isok(ret) or ret == dpret.WAIT,
            string.format("ctc_detach pip_writer: %s", tostring(ret)))

        -- 与 C 版一致：读端 open 在内核阻塞，worker 写端 open 后双方唤醒
        local efd, err = dppip.open(path, dplua.EVT_IN, true)
        ok(efd ~= nil, string.format("pip reader open failed: %s", tostring(err)))

        local buf = ffi.new("char[8]")
        local n = C.read(efd:fd(), buf, 3)
        ok(n == 3 and ffi.string(buf, 3) == "pip",
            string.format("ctc pip read failed: %s", tostring(n)))
        eq(dppip.path(efd), path, "pip path")
        dpele.del(efd)
        os.remove(path)
    end)

    test("dpfsm addev/delev via ffi", function()
        local dir = string.format("%s/dpapp_lua_fsm_%s", tmp, tag)
        os.execute(string.format("mkdir -p %q", dir))
        local fsm = dpele.new(C.dpfsm_type())
        ok(fsm ~= nil, "fsm create")
        local wd = C.dpfsm_addev(fsm, dir, IN_CREATE)
        ok(wd >= 0, string.format("fsm addev failed: %s", tostring(wd)))
        eq(C.dpfsm_delev(fsm, wd), dpret.OK)
        dpele.del(fsm)
        os.execute(string.format("rmdir %q", dir))
    end)

    test("dpfsm read inotify event via ffi", function()
        local dir = string.format("%s/dpapp_lua_fsm_evt_%s", tmp, tag)
        os.execute(string.format("mkdir -p %q", dir))
        local fsm = dpele.new(C.dpfsm_type())
        ok(fsm ~= nil, "fsm create")
        local wd = C.dpfsm_addev(fsm, dir, IN_CREATE)
        ok(wd >= 0, "fsm addev")

        local file = string.format("%s/event.txt", dir)
        os.remove(file)
        local fd = io.open(file, "w")
        fd:close()

        local ev = ffi.new("struct inotify_event")
        local read_ret = C.read(fsm:fd(), ev, ffi.sizeof("struct inotify_event"))
        ok(read_ret >= 16, string.format("fsm read failed: %s", tostring(read_ret)))
        ok(ev.wd == wd and bit.band(ev.mask, IN_CREATE) ~= 0, "fsm CREATE event")

        eq(C.dpfsm_delev(fsm, wd), dpret.OK)
        dpele.del(fsm)
        os.remove(file)
        os.execute(string.format("rmdir %q", dir))
    end)

    test("dpsig addno/hasno/delno", function()
        eq(dpsig.addno(dpsig.SIGUSR1), dpret.OK)
        ok(dpsig.hasno(dpsig.SIGUSR1))
        eq(dpsig.delno(dpsig.SIGUSR1), dpret.OK)
        ok(not dpsig.hasno(dpsig.SIGUSR1))
    end)

    test("dpsig.read signalfd event", function()
        eq(dpsig.addno(dpsig.SIGUSR1), dpret.OK)
        eq(C.raise(dpsig.SIGUSR1), 0)

        local info, err = dpsig.read()
        ok(info ~= nil, string.format("sig read failed: %s", tostring(err)))
        eq(info.signo, dpsig.SIGUSR1)
        eq(dpsig.delno(dpsig.SIGUSR1), dpret.OK)
    end)

    local total = stats.passed + stats.failed
    print(string.format("\n========================================"))
    print(string.format(" FSM/SIG/PIP Test Results: %d/%d passed", stats.passed, total))
    print(string.format("========================================"))

    if stats.failed > 0 then
        dplog.error("test", "=== FSM SIG PIP test FAILED: %d ===", stats.failed)
        os.exit(1)
    end
    dplog.notice("test", "=== FSM SIG PIP test ALL PASSED ===")
end

return M
