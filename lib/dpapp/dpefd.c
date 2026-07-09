#include "dpapp/dpefd.h"
#include "fcntl.h"
#include "unistd.h"

dpret_t dpefd_set_block(dpefd_t* efd, bool block)
{
    int fd = dpefd_fd(efd);
    if (fd >= 0) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags != -1) {
            int desired = flags;
            if (block) {
                desired &= ~O_NONBLOCK;
            } else {
                desired |= O_NONBLOCK;
            }

            if (desired == flags) {
                return DPE_OK;
            }

            if (fcntl(fd, F_SETFL, desired) != -1) {
                return DPE_OK;
            }
        }
        return -errno;
    } else {
        return DPE_INVAL;
    }
}
