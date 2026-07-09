-- yxfangcs<yxfangcs@yeah.net> / 20241127 / Linux 信号 fd
local dpret = require("dpret")
local dpasc = require("dpasc")
local ffi = require("ffi")
require("dpele")

ffi.cdef([[
struct signalfd_siginfo
{
    uint32_t ssi_signo;
    int32_t ssi_errno;
    int32_t ssi_code;
    uint32_t ssi_pid;
    uint32_t ssi_uid;
    int32_t ssi_fd;
    uint32_t ssi_tid;
    uint32_t ssi_band;
    uint32_t ssi_overrun;
    uint32_t ssi_trapno;
    int32_t ssi_status;
    int32_t ssi_int;
    uint64_t ssi_ptr;
    uint64_t ssi_utime;
    uint64_t ssi_stime;
    uint64_t ssi_addr;
    uint16_t ssi_addr_lsb;
    uint16_t __pad2;
    int32_t ssi_syscall;
    uint64_t ssi_call_addr;
    uint32_t ssi_arch;
    uint8_t __pad[28];
};

typedef struct signalfd_siginfo siginfo;

const dpele_type_t* dpsig_type();
dpret_t dpsig_addno(dpefd_t* self, int signo);
dpret_t dpsig_delno(dpefd_t* self, int signo);
bool dpsig_hasno(dpefd_t* self, int signo);
]])
local C = ffi.C

local DPE_OK = dpret.OK
local DPE_INVAL = dpret.INVAL
local dpret_isok = dpret.isok
local ffi_sizeof = ffi.sizeof

local M = {
    ["SIGHUP"] = 1,
    ["SIGINT"] = 2,
    ["SIGQUIT"] = 3,
    ["SIGILL"] = 4,
    ["SIGTRAP"] = 5,
    ["SIGABRT"] = 6,
    ["SIGBUS"] = 7,
    ["SIGFPE"] = 8,
    ["SIGKILL"] = 9, -- 忽略
    ["SIGUSR1"] = 10,
    ["SIGSEGV"] = 11,
    ["SIGUSR2"] = 12,
    ["SIGPIPE"] = 13,
    ["SIGALRM"] = 14,
    ["SIGTERM"] = 15,
    ["SIGSTKFLT"] = 16,
    ["SIGCHLD"] = 17,
    ["SIGCONT"] = 18,
    ["SIGSTOP"] = 19, -- 忽略
    ["SIGTSTP"] = 20,
    ["SIGTTIN"] = 21,
    ["SIGTTOU"] = 22,
    ["SIGURG"] = 23,
    ["SIGXCPU"] = 24,
    ["SIGXFSZ"] = 25,
    ["SIGVTALRM"] = 26,
    ["SIGPROF"] = 27,
    ["SIGWINCH"] = 28,
    ["SIGIO"] = 29,
    ["SIGPWR"] = 30,
    ["SIGSYS"] = 31,
    ["SIGRTMIN"] = 34,
    ["SIGRTMIN+1"] = 35,
    ["SIGRTMIN+2"] = 36,
    ["SIGRTMIN+3"] = 37,
    ["SIGRTMIN+4"] = 38,
    ["SIGRTMIN+5"] = 39,
    ["SIGRTMIN+6"] = 40,
    ["SIGRTMIN+7"] = 41,
    ["SIGRTMIN+8"] = 42,
    ["SIGRTMIN+9"] = 43,
    ["SIGRTMIN+10"] = 44,
    ["SIGRTMIN+11"] = 45,
    ["SIGRTMIN+12"] = 46,
    ["SIGRTMIN+13"] = 47,
    ["SIGRTMIN+14"] = 48,
    ["SIGRTMIN+15"] = 49,
    ["SIGRTMAX-14"] = 50,
    ["SIGRTMAX-13"] = 51,
    ["SIGRTMAX-12"] = 52,
    ["SIGRTMAX-11"] = 53,
    ["SIGRTMAX-10"] = 54,
    ["SIGRTMAX-9"] = 55,
    ["SIGRTMAX-8"] = 56,
    ["SIGRTMAX-7"] = 57,
    ["SIGRTMAX-6"] = 58,
    ["SIGRTMAX-5"] = 59,
    ["SIGRTMAX-4"] = 60,
    ["SIGRTMAX-3"] = 61,
    ["SIGRTMAX-2"] = 62,
    ["SIGRTMAX-1"] = 63,
    ["SIGRTMAX"] = 64,
}

M.type = C.dpsig_type()

local sig_efd = nil

local function _check_init_sig()
    if not sig_efd then
        sig_efd = C.dpele_new(C.dpsig_type())
        sig_efd:setgc(true)
    end
    return sig_efd
end

local function _signo(sig)
    sig = tonumber(sig)
    if sig == nil then
        return nil
    end
    return ffi.cast("int", sig)
end

function M.addno(sig)
    local signo = _signo(sig)
    if signo == nil then
        return DPE_INVAL
    end
    return C.dpsig_addno(_check_init_sig(), signo)
end

function M.delno(sig)
    local signo = _signo(sig)
    if signo == nil then
        return DPE_INVAL
    end
    return C.dpsig_delno(_check_init_sig(), signo)
end

function M.hasno(sig)
    local signo = _signo(sig)
    if signo == nil then
        return false
    end
    return C.dpsig_hasno(_check_init_sig(), signo)
end

local sig_info = ffi.new("siginfo")

function M.read()
    local efd = _check_init_sig()
    local ret = dpasc.gfd_read(efd, sig_info, ffi_sizeof("siginfo"))
    if not dpret_isok(ret) then
        return nil, ret
    end

    return {
        signo = sig_info.ssi_signo,
        errno = sig_info.ssi_errno,
        code = sig_info.ssi_code,
        pid = sig_info.ssi_pid,
        uid = sig_info.ssi_uid,
        fd = sig_info.ssi_fd,
        tid = sig_info.ssi_tid,
        band = sig_info.ssi_band,
        overrun = sig_info.ssi_overrun,
        trapno = sig_info.ssi_trapno,
        status = sig_info.ssi_status,
        int = sig_info.ssi_int,
        ptr = sig_info.ssi_ptr,
        utime = sig_info.ssi_utime,
        stime = sig_info.ssi_stime,
        addr = sig_info.ssi_addr,
        addr_lsb = sig_info.ssi_addr_lsb,
        syscall = sig_info.ssi_syscall,
        call_addr = sig_info.ssi_call_addr,
        arch = sig_info.ssi_arch,
    }, DPE_OK
end

return M
