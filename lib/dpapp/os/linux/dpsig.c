#include "dpapp/dpefd.h"
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/signalfd.h>
#include <unistd.h>

typedef struct
{
    sigset_t mask;
} dpsig_iop_t;

dpret_t dpsig_vopen(void* udata, va_list varg)
{
    dpsig_iop_t* siop = (dpsig_iop_t*)udata;
    sigemptyset(&siop->mask);
    int fd = signalfd(-1, &siop->mask, SFD_CLOEXEC);
    return fd;
}

dpret_t dpsig_copy(void* dst_data, const void* src_data)
{
    dpsig_iop_t* src = (dpsig_iop_t*)src_data;
    dpsig_iop_t* dst = (dpsig_iop_t*)dst_data;
    dst->mask = src->mask;
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
    if (signalfd(dpefd_fd(self), &siop->mask, 0) < 0) {
        return -errno;
    }
    if (sigprocmask(SIG_BLOCK, &siop->mask, NULL) != 0) {
        return -errno;
    }
    return DPE_OK;
}

dpret_t dpsig_delno(dpele_t* self, int signo)
{
    dpsig_iop_t* siop = (dpsig_iop_t*)dpele_aux_data(self);
    if (sigdelset(&siop->mask, signo) != 0) {
        return -errno;
    }
    if (signalfd(dpefd_fd(self), &siop->mask, 0) < 0) {
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
