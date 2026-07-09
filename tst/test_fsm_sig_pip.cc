#include "dpapp/dpevp.h"
#include "dpcpp/dpcpp.hh"
#include "dpcpp/dpcpp_asc.hh"
#include <fcntl.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{

struct fsp_test_ctx
{
    std::atomic<int> failed{0};
    char pip_path[256];
};

static fsp_test_ctx g_ctx;

static void fsp_fail(const char* name)
{
    g_ctx.failed.fetch_add(1);
    dplog_error("test", "%s: failed", name);
}

static void fsp_ok(const char* name)
{
    dplog_notice("test", "%s: ok", name);
}

static void fsp_reset()
{
    g_ctx.failed.store(0);
    g_ctx.pip_path[0] = '\0';
}

static void test_pip_sync()
{
    char path[256];
    std::snprintf(path, sizeof(path), "/tmp/dpapp_cpp_pip_sync_%d", getpid());

    unlink(path);
    int fd = open(path, O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        fsp_fail("pip sync regular file prep");
        return;
    }
    close(fd);

    dpele_t* bad = dpele_new(dppip_type(), path, DPEVT_IN, 0);
    if (bad != nullptr) {
        dpele_del(bad);
        fsp_fail("pip sync reject non-fifo");
    } else {
        fsp_ok("pip sync reject non-fifo");
    }
    unlink(path);

    dpele_t* bad_ev = dpele_new(dppip_type(), path, DPEVT_IN | DPEVT_OUT, 0);
    if (bad_ev != nullptr) {
        dpele_del(bad_ev);
        fsp_fail("pip sync reject invalid ev");
    } else {
        fsp_ok("pip sync reject invalid ev");
    }
}

static dpcpp::aco_ret pip_writer(dpele_t* task, dpv64_t arg)
{
    (void)task;
    const char* path = (const char*)arg.ptr;

    int wr = open(path, O_WRONLY);
    if (wr < 0) {
        fsp_fail("pip async writer open");
        co_return DPE_INTERNAL_ERROR;
    }
    (void)write(wr, "pip", 3);
    close(wr);
    co_return DPE_OK;
}

static int fsp_type1_worker()
{
    const dpapp_info_t* info = dpapp_info();
    if (info == nullptr || info->each_count[1] <= 0) {
        return -1;
    }
    return info->each_ids[1][0];
}

static dpcpp::aco_ret test_pip_async()
{
    int wid = fsp_type1_worker();
    if (wid < 0) {
        dplog_notice("test", "skip pip async (no type1 worker)");
        co_return DPE_OK;
    }

    std::snprintf(g_ctx.pip_path, sizeof(g_ctx.pip_path),
        "/tmp/dpapp_cpp_pip_async_%d", getpid());
    unlink(g_ctx.pip_path);
    if (mkfifo(g_ctx.pip_path, 0666) != 0) {
        fsp_fail("pip async mkfifo");
        co_return DPE_OK;
    }

    dpret_t ret = co_await dpcpp::ctc_detach(wid, pip_writer,
        DPV64_PTR(g_ctx.pip_path));
    if (!dpret_isok(ret)) {
        fsp_fail("pip async detach writer");
        unlink(g_ctx.pip_path);
        co_return DPE_OK;
    }

    dpele_t* efd = dpele_new(dppip_type(), g_ctx.pip_path, DPEVT_IN, 1);
    if (efd == nullptr) {
        fsp_fail("pip async reader open");
        unlink(g_ctx.pip_path);
        co_return DPE_OK;
    }
    co_await dpcpp::sleep(0.05);

    char buf[8] = {0};
    ssize_t n = read(dpefd_fd(efd), buf, 3);
    if (n != 3 || std::memcmp(buf, "pip", 3) != 0) {
        fsp_fail("pip async read");
    } else {
        fsp_ok("pip async read");
    }

    const char* got = dppip_path((dpefd_t*)efd);
    if (got == nullptr || std::strcmp(got, g_ctx.pip_path) != 0) {
        fsp_fail("pip async path");
    } else {
        fsp_ok("pip async path");
    }

    dpele_del(efd);
    co_return DPE_OK;
}

static void test_fsm_sync()
{
    char dir[256];
    std::snprintf(dir, sizeof(dir), "/tmp/dpapp_cpp_fsm_sync_%d", getpid());
    if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
        fsp_fail("fsm sync mkdir");
        return;
    }

    dpele_t* fsm = dpele_new(dpfsm_type());
    if (fsm == nullptr) {
        fsp_fail("fsm sync create");
        rmdir(dir);
        return;
    }

    dpret_t wd = dpfsm_addev((dpefd_t*)fsm, dir, IN_CREATE);
    if (wd < 0) {
        fsp_fail("fsm sync addev");
        dpele_del(fsm);
        rmdir(dir);
        return;
    }
    fsp_ok("fsm sync addev");

    if (!dpret_isok(dpfsm_delev((dpefd_t*)fsm, (int)wd))) {
        fsp_fail("fsm sync delev");
    } else {
        fsp_ok("fsm sync delev");
    }

    dpele_del(fsm);
    rmdir(dir);
}

static dpcpp::aco_ret test_fsm_async()
{
    char dir[256];
    std::snprintf(dir, sizeof(dir), "/tmp/dpapp_cpp_fsm_async_%d", getpid());
    if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
        fsp_fail("fsm async mkdir");
        co_return DPE_OK;
    }

    dpele_t* fsm = dpele_new(dpfsm_type());
    if (fsm == nullptr) {
        fsp_fail("fsm async create");
        rmdir(dir);
        co_return DPE_OK;
    }

    dpret_t wd = dpfsm_addev((dpefd_t*)fsm, dir, IN_CREATE);
    if (wd < 0) {
        fsp_fail("fsm async addev");
        dpele_del(fsm);
        rmdir(dir);
        co_return DPE_OK;
    }

    char file[320];
    std::snprintf(file, sizeof(file), "%s/event.txt", dir);
    int fd = open(file, O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) {
        close(fd);
    }

    char evbuf[4096];
    ssize_t rn = read(dpefd_fd(fsm), evbuf, sizeof(evbuf));
    auto* ev = reinterpret_cast<struct inotify_event*>(evbuf);
    if (rn < (ssize_t)sizeof(struct inotify_event) || ev->wd != wd
        || !(ev->mask & IN_CREATE)) {
        fsp_fail("fsm async read");
    } else {
        fsp_ok("fsm async read");
    }

    dpfsm_delev((dpefd_t*)fsm, (int)wd);
    dpele_del(fsm);
    unlink(file);
    rmdir(dir);
    co_return DPE_OK;
}

static void test_sig_sync()
{
    dpele_t* sig = dpele_new(dpsig_type());
    if (sig == nullptr) {
        fsp_fail("sig sync create");
        return;
    }

    if (!dpret_isok(dpsig_addno((dpefd_t*)sig, SIGUSR1))) {
        fsp_fail("sig sync addno");
    } else if (!dpsig_hasno((dpefd_t*)sig, SIGUSR1)) {
        fsp_fail("sig sync hasno");
    } else if (!dpret_isok(dpsig_delno((dpefd_t*)sig, SIGUSR1))) {
        fsp_fail("sig sync delno");
    } else if (dpsig_hasno((dpefd_t*)sig, SIGUSR1)) {
        fsp_fail("sig sync hasno after del");
    } else {
        fsp_ok("sig sync mask");
    }

    dpele_del(sig);
}

static dpcpp::aco_ret test_sig_async()
{
    dpele_t* sig = dpele_new(dpsig_type());
    if (sig == nullptr) {
        fsp_fail("sig async create");
        co_return DPE_OK;
    }

    if (!dpret_isok(dpsig_addno((dpefd_t*)sig, SIGUSR1))) {
        fsp_fail("sig async addno");
        dpele_del(sig);
        co_return DPE_OK;
    }

    if (raise(SIGUSR1) != 0) {
        fsp_fail("sig async kill");
        dpele_del(sig);
        co_return DPE_OK;
    }

    struct signalfd_siginfo si;
    ssize_t n = read(dpefd_fd(sig), &si, (int)sizeof(si));
    if (n != (ssize_t)sizeof(si) || si.ssi_signo != SIGUSR1) {
        fsp_fail("sig async read");
    } else {
        fsp_ok("sig async read");
    }

    dpele_del(sig);
    co_return DPE_OK;
}

static dpcpp::aco_v64 init00(dpv64_t, dpv64_t)
{
    fsp_reset();
    dplog_notice("test",
        "=== FSM SIG PIP test begin (type=%d id=%d) ===", dpevp_type(), dpevp_id());

    test_pip_sync();
    test_fsm_sync();
    test_sig_sync();
    co_await test_pip_async();
    co_await test_fsm_async();
    co_await test_sig_async();

    int failed = g_ctx.failed.load();
    if (failed == 0) {
        dplog_notice("test", "=== FSM SIG PIP test ALL PASSED ===");
    } else {
        dplog_error("test", "=== FSM SIG PIP test FAILED: %d ===", failed);
    }
    co_return DPV64_NULL;
}

static dpcpp::aco_v64 init01(dpv64_t, dpv64_t)
{
    dplog_notice("test", "worker ready (type=%d id=%d)", dpevp_type(), dpevp_id());
    co_return DPV64_NULL;
}

} // namespace

extern "C" dpret_t dpcpp__test_cpp_fsm_sig_pip(int argc, char** argv,
    dpcpp::app_hdr* hdrs)
{
    (void)argc;
    (void)argv;

    hdrs[0].init = init00;
    hdrs[1].init = init01;
    return 2;
}
