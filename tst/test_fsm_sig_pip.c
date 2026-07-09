#include "dpapp/dpapp.h"
#include "dpapp/dpevp.h"
#include "dpapp/dplog.h"
#include "dpcwc/dpcwc.h"
#include "dpcwc/dpcwc_asc.h"
#include "dpcwc/dpcwc_aux.h"
#include <fcntl.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct
{
    atomic_int failed;
    char pip_path[256];
} fsp_test_ctx_t;

static fsp_test_ctx_t g_ctx;

static void _fsp_fail(const char* name)
{
    atomic_fetch_add(&g_ctx.failed, 1);
    dplog_error("test", "%s: failed", name);
}

static void _fsp_ok(const char* name)
{
    dplog_notice("test", "%s: ok", name);
}

static void _fsp_test_pip_sync(void)
{
    char path[256];
    snprintf(path, sizeof(path), "/tmp/dpapp_pip_sync_%d", getpid());

    unlink(path);
    int fd = open(path, O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        _fsp_fail("pip sync regular file prep");
        return;
    }
    close(fd);

    dpele_t* bad = dpele_new(dppip_type(), path, DPEVT_IN, 0);
    if (bad != NULL) {
        dpele_del(bad);
        _fsp_fail("pip sync reject non-fifo");
    } else {
        _fsp_ok("pip sync reject non-fifo");
    }
    unlink(path);

    dpele_t* bad_ev = dpele_new(dppip_type(), path, DPEVT_IN | DPEVT_OUT, 0);
    if (bad_ev != NULL) {
        dpele_del(bad_ev);
        _fsp_fail("pip sync reject invalid ev");
    } else {
        _fsp_ok("pip sync reject invalid ev");
    }
}

static int _fsp_type1_worker(void)
{
    const dpapp_info_t* info = dpapp_info();
    if (info == NULL || info->each_count[1] <= 0) {
        return -1;
    }
    return info->each_ids[1][0];
}

static dpret_t _fsp_pip_writer(dpele_t* task, dpv64_t arg)
{
    (void)task;
    const char* path = (const char*)arg.ptr;

    int wr = open(path, O_WRONLY);
    if (wr < 0) {
        _fsp_fail("pip async writer open");
        return DPE_INTERNAL_ERROR;
    }
    (void)write(wr, "pip", 3);
    close(wr);
    return DPE_OK;
}

static void _fsp_test_pip_async(void)
{
    int wid = _fsp_type1_worker();
    if (wid < 0) {
        dplog_notice("test", "skip pip async (no type1 worker)");
        return;
    }

    snprintf(g_ctx.pip_path, sizeof(g_ctx.pip_path), "/tmp/dpapp_pip_async_%d",
        getpid());
    unlink(g_ctx.pip_path);
    if (mkfifo(g_ctx.pip_path, 0666) != 0) {
        _fsp_fail("pip async mkfifo");
        return;
    }

    dpret_t ret = dpcwc_ctc_detach(wid, _fsp_pip_writer, DPV64_PTR(g_ctx.pip_path));
    if (!dpret_isok(ret)) {
        _fsp_fail("pip async detach writer");
        unlink(g_ctx.pip_path);
        return;
    }

    dpele_t* efd = dpele_new(dppip_type(), g_ctx.pip_path, DPEVT_IN, 1);
    if (efd == NULL) {
        _fsp_fail("pip async reader open");
        unlink(g_ctx.pip_path);
        return;
    }
    dpcwc_sleep(0.05, DPV64_NULL);

    char buf[8] = {0};
    ssize_t n = read(dpefd_fd(efd), buf, 3);
    if (n != 3 || memcmp(buf, "pip", 3) != 0) {
        _fsp_fail("pip async read");
    } else {
        _fsp_ok("pip async read");
    }

    const char* got = dppip_path((dpefd_t*)efd);
    if (got == NULL || strcmp(got, g_ctx.pip_path) != 0) {
        _fsp_fail("pip async path");
    } else {
        _fsp_ok("pip async path");
    }

    dpele_del(efd);
}

static void _fsp_test_fsm_sync(void)
{
    char dir[256];
    snprintf(dir, sizeof(dir), "/tmp/dpapp_fsm_sync_%d", getpid());
    if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
        _fsp_fail("fsm sync mkdir");
        return;
    }

    dpele_t* fsm = dpele_new(dpfsm_type());
    if (fsm == NULL) {
        _fsp_fail("fsm sync create");
        rmdir(dir);
        return;
    }

    dpret_t wd = dpfsm_addev((dpefd_t*)fsm, dir, IN_CREATE);
    if (wd < 0) {
        _fsp_fail("fsm sync addev");
        dpele_del(fsm);
        rmdir(dir);
        return;
    }
    _fsp_ok("fsm sync addev");

    if (!dpret_isok(dpfsm_delev((dpefd_t*)fsm, (int)wd))) {
        _fsp_fail("fsm sync delev");
    } else {
        _fsp_ok("fsm sync delev");
    }

    dpele_del(fsm);
    rmdir(dir);
}

static void _fsp_test_fsm_async(void)
{
    char dir[256];
    snprintf(dir, sizeof(dir), "/tmp/dpapp_fsm_async_%d", getpid());
    if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
        _fsp_fail("fsm async mkdir");
        return;
    }

    dpele_t* fsm = dpele_new(dpfsm_type());
    if (fsm == NULL) {
        _fsp_fail("fsm async create");
        rmdir(dir);
        return;
    }

    dpret_t wd = dpfsm_addev((dpefd_t*)fsm, dir, IN_CREATE);
    if (wd < 0) {
        _fsp_fail("fsm async addev");
        dpele_del(fsm);
        rmdir(dir);
        return;
    }

    char file[320];
    snprintf(file, sizeof(file), "%s/event.txt", dir);
    int fd = open(file, O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) {
        close(fd);
    }

    char evbuf[4096];
    ssize_t n = read(dpefd_fd(fsm), evbuf, sizeof(evbuf));
    struct inotify_event* ev = (struct inotify_event*)evbuf;
    if (n < (ssize_t)sizeof(struct inotify_event) || ev->wd != wd
        || !(ev->mask & IN_CREATE)) {
        _fsp_fail("fsm async read");
    } else {
        _fsp_ok("fsm async read");
    }

    dpfsm_delev((dpefd_t*)fsm, (int)wd);
    dpele_del(fsm);
    unlink(file);
    rmdir(dir);
}

static void _fsp_test_sig_sync(void)
{
    dpele_t* sig = dpele_new(dpsig_type());
    if (sig == NULL) {
        _fsp_fail("sig sync create");
        return;
    }

    if (!dpret_isok(dpsig_addno((dpefd_t*)sig, SIGUSR1))) {
        _fsp_fail("sig sync addno");
    } else if (!dpsig_hasno((dpefd_t*)sig, SIGUSR1)) {
        _fsp_fail("sig sync hasno");
    } else if (!dpret_isok(dpsig_delno((dpefd_t*)sig, SIGUSR1))) {
        _fsp_fail("sig sync delno");
    } else if (dpsig_hasno((dpefd_t*)sig, SIGUSR1)) {
        _fsp_fail("sig sync hasno after del");
    } else {
        _fsp_ok("sig sync mask");
    }

    dpele_del(sig);
}

static void _fsp_test_sig_async(void)
{
    dpele_t* sig = dpele_new(dpsig_type());
    if (sig == NULL) {
        _fsp_fail("sig async create");
        return;
    }

    if (!dpret_isok(dpsig_addno((dpefd_t*)sig, SIGUSR1))) {
        _fsp_fail("sig async addno");
        dpele_del(sig);
        return;
    }

    if (raise(SIGUSR1) != 0) {
        _fsp_fail("sig async kill");
        dpele_del(sig);
        return;
    }

    struct signalfd_siginfo si;
    ssize_t n = read(dpefd_fd(sig), &si, (int)sizeof(si));
    if (n != (ssize_t)sizeof(si) || si.ssi_signo != SIGUSR1) {
        _fsp_fail("sig async read");
    } else {
        _fsp_ok("sig async read");
    }

    dpele_del(sig);
}

static dpv64_t init00(dpv64_t arg1, dpv64_t arg2)
{
    (void)arg1;
    (void)arg2;

    memset(&g_ctx, 0, sizeof(g_ctx));
    dplog_notice("test",
        "=== FSM SIG PIP test begin (type=%d id=%d) ===", dpevp_type(), dpevp_id());

    _fsp_test_pip_sync();
    _fsp_test_fsm_sync();
    _fsp_test_sig_sync();
    _fsp_test_pip_async();
    _fsp_test_fsm_async();
    _fsp_test_sig_async();

    int failed = atomic_load(&g_ctx.failed);
    if (failed == 0) {
        dplog_notice("test", "=== FSM SIG PIP test ALL PASSED ===");
    } else {
        dplog_error("test", "=== FSM SIG PIP test FAILED: %d ===", failed);
    }
    return DPV64_NULL;
}

static dpv64_t init01(dpv64_t arg1, dpv64_t arg2)
{
    (void)arg1;
    (void)arg2;
    dplog_notice("test", "worker ready (type=%d id=%d)", dpevp_type(), dpevp_id());
    return DPV64_NULL;
}

extern dpret_t dpcwc__test_fsm_sig_pip(int argc, char** argv, dpapp_hdr_t* hdrs)
{
    (void)argc;
    (void)argv;

    hdrs[0].init = init00;
    hdrs[1].init = init01;
    return 2;
}
