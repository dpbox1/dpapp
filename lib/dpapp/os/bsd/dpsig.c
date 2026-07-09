#include "dpapp/dpefd.h"
#include "dpapp/os/dpevp_pri.h"
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

/* 与 Linux signalfd_siginfo 布局兼容，供绑定层 decode */
typedef struct
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
} dpsig_siginfo_t;

typedef struct
{
    sigset_t mask;
    int pipe_w;
} dpsig_iop_t;

dpret_t dpsig_vopen(void* udata, va_list varg)
{
    (void)varg;
    dpsig_iop_t* siop = (dpsig_iop_t*)udata;
    sigemptyset(&siop->mask);
    siop->pipe_w = -1;

    int fds[2];
    if (pipe(fds) != 0) {
        return -errno;
    }
    siop->pipe_w = fds[1];
    return fds[0];
}

static void _dpsig_fini(void* udata)
{
    dpsig_iop_t* siop = (dpsig_iop_t*)udata;
    if (siop->pipe_w >= 0) {
        close(siop->pipe_w);
        siop->pipe_w = -1;
    }
}

dpret_t dpsig_copy(void* dst_data, const void* src_data)
{
    dpsig_iop_t* src = (dpsig_iop_t*)src_data;
    dpsig_iop_t* dst = (dpsig_iop_t*)dst_data;
    dst->mask = src->mask;
    dst->pipe_w = -1;
    return DPE_OK;
}

static dpele_type_t _dpsig_type = {
    .name = "sig",
    .type = DPELE_TYPE_EFD,
    .size = sizeof(dpsig_iop_t),
    .iotype = DPAIO_TYPE_GFD,
    .events = DPEVT_IN,
    .init = dpsig_vopen,
    .copy = dpsig_copy,
    .fini = _dpsig_fini,
};

const dpele_type_t* dpsig_type()
{
    return &_dpsig_type;
}

dpret_t dpsig_addno(dpele_t* self, int signo)
{
    dpsig_iop_t* siop = (dpsig_iop_t*)dpele_aux_data(self);
    if (sigaddset(&siop->mask, signo) != 0) {
        return -errno;
    }
    if (sigprocmask(SIG_BLOCK, &siop->mask, NULL) != 0) {
        return -errno;
    }
    return _dpevp_kq_sig_add(self, signo);
}

dpret_t dpsig_delno(dpele_t* self, int signo)
{
    dpsig_iop_t* siop = (dpsig_iop_t*)dpele_aux_data(self);
    dpret_t r = _dpevp_kq_sig_del(signo);
    if (dpret_iserr(r)) {
        return r;
    }
    if (sigdelset(&siop->mask, signo) != 0) {
        return -errno;
    }
    if (sigprocmask(SIG_SETMASK, &siop->mask, NULL) != 0) {
        return -errno;
    }
    return DPE_OK;
}

bool dpsig_hasno(dpele_t* self, int signo)
{
    dpsig_iop_t* siop = (dpsig_iop_t*)dpele_aux_data(self);
    return 1 == sigismember(&siop->mask, signo);
}

void _dpsig_kq_notify(dpele_t* ele, int signo)
{
    dpsig_iop_t* siop = (dpsig_iop_t*)dpele_aux_data(ele);
    if (siop->pipe_w < 0) {
        return;
    }

    dpsig_siginfo_t si;
    memset(&si, 0, sizeof(si));
    si.ssi_signo = (uint32_t)signo;
    (void)write(siop->pipe_w, &si, sizeof(si));
}
