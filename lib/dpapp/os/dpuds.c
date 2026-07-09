#include "dpapp/dpefd.h"
#include "dpapp/dpevp.h"
#include "unistd.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

typedef struct
{
    socklen_t real;
    uint32_t _pending;
    struct sockaddr_un addr;
} dpuds_data_t;

typedef dpuds_data_t dpuds_client_t;
typedef dpuds_data_t dpuds_server_t;
typedef dpuds_data_t dpuds_listen_t;

static void _dpuds_ctx_clean(void* iop)
{}

static dpret_t _dpuds_ctx_copy(void* dst, const void* src)
{
    memcpy(dst, src, sizeof(dpuds_data_t));
    return DPE_OK;
}

static dpret_t _dpuds_client_vopen(void* udata, va_list varg)
{
    const char* udsfile = va_arg(varg, const char*);
    if (udsfile == NULL) {
        return DPE_INVAL;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -errno;
    }

    dpuds_data_t* ios = (dpuds_data_t*)udata;
    struct sockaddr_un* addr = &ios->addr;
    ios->real = SUN_LEN(addr);

    addr->sun_family = AF_UNIX;
    strncpy(addr->sun_path, udsfile, sizeof(addr->sun_path) - 1);
    addr->sun_path[sizeof(addr->sun_path) - 1] = '\0';

    return fd;
}

static dpele_type_t _dpuds_client_type = {
    .name = "uds_client",
    .type = DPELE_TYPE_EFD,
    .size = sizeof(dpuds_data_t),
    .iotype = DPAIO_TYPE_SKT,
    .events = DPEVT_IN | DPEVT_OUT,
    .init = _dpuds_client_vopen,
    .copy = _dpuds_ctx_copy,
    .fini = _dpuds_ctx_clean,
};

const dpele_type_t* dpuds_client_type()
{
    return &_dpuds_client_type;
}

static dpret_t _dpuds_server_vopen(void* udata, va_list varg)
{
    int fd = va_arg(varg, int);
    dpuds_server_t* ios = (dpuds_server_t*)udata;
    dpuds_server_t* d = va_arg(varg, dpuds_server_t*);
    *ios = *d;

    return fd;
}

static dpele_type_t _dpuds_server_type = {
    .name = "uds_server",
    .type = DPELE_TYPE_EFD,
    .size = sizeof(dpuds_data_t),
    .iotype = DPAIO_TYPE_SKT,
    .events = DPEVT_IN | DPEVT_OUT,
    .init = _dpuds_server_vopen,
    .copy = _dpuds_ctx_copy,
    .fini = _dpuds_ctx_clean,
};

const dpele_type_t* dpuds_server_type()
{
    return &_dpuds_server_type;
}

static dpret_t _dpuds_listen_vopen(void* udata, va_list varg)
{
    const char* udsfile = va_arg(varg, const char*);
    if (udsfile == NULL) {
        return DPE_INVAL;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -errno;

    dpuds_listen_t* ios = (dpuds_listen_t*)udata;
    struct sockaddr_un* addr = &ios->addr;
    ios->real = SUN_LEN(addr);

    memset(addr, 0, sizeof(ios->addr));
    addr->sun_family = AF_UNIX;
    strncpy(addr->sun_path, udsfile, sizeof(addr->sun_path) - 1);

    unlink(udsfile);

    if (bind(fd, (struct sockaddr*)addr, ios->real) != 0 || listen(fd, 100) != 0) {
        close(fd);
        return -errno;
    }

    return fd;
}

static dpele_type_t _dpuds_listen_type = {
    .name = "uds_listen",
    .type = DPELE_TYPE_EFD,
    .size = sizeof(dpuds_listen_t),
    .iotype = DPAIO_TYPE_SKT,
    .events = DPEVT_IN,
    .init = _dpuds_listen_vopen,
    .copy = _dpuds_ctx_copy,
    .fini = _dpuds_ctx_clean,
};

const dpele_type_t* dpuds_listen_type()
{
    return &_dpuds_listen_type;
}
