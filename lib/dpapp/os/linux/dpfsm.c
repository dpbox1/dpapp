#include "dpapp/dpefd.h"
#include "stdlib.h"

#include "fcntl.h"
#include "unistd.h"
#include <sys/inotify.h>

dpret_t dpfsm_vopen(void* udata, va_list varg)
{
    int fd = inotify_init1(IN_CLOEXEC);
    return fd;
}

static dpele_type_t _dpfsm_type = {
    .name = "fsm",
    .type = DPELE_TYPE_EFD,
    .iotype = DPAIO_TYPE_GFD,
    .events = DPEVT_IN,
    .init = dpfsm_vopen,
};

const dpele_type_t* dpfsm_type()
{
    return &_dpfsm_type;
}

dpret_t dpfsm_addev(dpele_t* self, const char* path, uint32_t mask)
{
    // dpfsm_iop_t* niop = (dpfsm_iop_t*)dpepfd_data(self);
    int ret = inotify_add_watch(dpefd_fd(self), path, mask);
    if (ret < 0) {
        ret = -errno;
    }
    return ret;
}

dpret_t dpfsm_delev(dpele_t* self, int wd)
{
    // dpfsm_iop_t* niop = (dpfsm_iop_t*)dpepfd_data(self);
    int ret = inotify_rm_watch(dpefd_fd(self), wd);
    return ret < 0 ? -errno : DPE_OK;
}
