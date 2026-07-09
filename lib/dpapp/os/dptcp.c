#include "dpapp/dpefd.h"
#include "fcntl.h"
#include "unistd.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

static bool _set_nodelay(int fd)
{
    int optval = 1;
    return (0 == setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(int)));
}

static __thread char _g_tcpaddr_tmp[128] = {
    0}; // INET6_ADDRSTRLEN + NI_MAXSERV 所需空间
const char* get_address(const struct sockaddr* sockAddr, socklen_t sockLen)
{
    if (sockLen == 0) {
        return "";
    }

    char ipStr[INET6_ADDRSTRLEN] = "";
    char portStr[NI_MAXSERV] = "";
    if (getnameinfo(sockAddr, sockLen, ipStr, sizeof(ipStr), portStr,
            sizeof(portStr), NI_NUMERICHOST | NI_NUMERICSERV)
        != 0) {
        return "";
    }

    sprintf(_g_tcpaddr_tmp, "tcp://%s:%s", ipStr, portStr);
    return _g_tcpaddr_tmp;
}

typedef struct
{
    DPSOCKADDR_HEAD
} dptcp_data_t;

typedef dptcp_data_t dptcp_client_t;
typedef dptcp_data_t dptcp_server_t;
typedef dptcp_data_t dptcp_listen_t;

static void _dptcp_ctx_clean(void* iop)
{
    (void)iop;
}

static dpret_t _dptcp_ctx_copy(void* dst, const void* src)
{
    memcpy(dst, src, sizeof(dptcp_data_t));
    return DPE_OK;
}

static dpret_t _dptcp_client_vopen(void* udata, va_list varg)
{
    const char* host = va_arg(varg, const char*);
    int port = va_arg(varg, int);
    if (host == NULL || port <= 0 || port > UINT16_MAX) {
        return DPE_INVAL;
    }
    char portstr[8] = "";
    sprintf(portstr, "%d", port);

    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int r = getaddrinfo(host, portstr, &hints, &result);
    if (r != 0) {
        return -r;
    }

    dptcp_client_t* ios = (dptcp_client_t*)udata;

    int fd = -1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1) {
            continue;
        }
        if (!_set_nodelay(fd)) {
            close(fd);
            fd = -1;
            continue;
        }
        memcpy(&ios->addr, rp->ai_addr, rp->ai_addrlen);
        ios->real = rp->ai_addrlen;
        break;
    }

    freeaddrinfo(result);
    return fd;
}

static dpele_type_t _dptcp_client_type = {
    .name = "tcp_client",
    .type = DPELE_TYPE_EFD,
    .size = sizeof(dptcp_data_t),
    .iotype = DPAIO_TYPE_SKT,
    .events = DPEVT_IN | DPEVT_OUT,
    .init = _dptcp_client_vopen,
    .copy = _dptcp_ctx_copy,
    .fini = _dptcp_ctx_clean,
};

const dpele_type_t* dptcp_client_type()
{
    return &_dptcp_client_type;
}

static dpret_t _dptcp_server_vopen(void* udata, va_list varg)
{
    int fd = va_arg(varg, int);
    dptcp_server_t* d = (dptcp_server_t*)udata;

    dptcp_data_t* addr = va_arg(varg, dptcp_data_t*);
    if (addr == NULL)
        return DPE_INVAL;
    *d = *addr;

    return fd;
}

static dpele_type_t _dptcp_server_type = {
    .name = "tcp_server",
    .type = DPELE_TYPE_EFD,
    .size = sizeof(dptcp_data_t),
    .iotype = DPAIO_TYPE_SKT,
    .events = DPEVT_IN | DPEVT_OUT,
    .init = _dptcp_server_vopen,
    .copy = _dptcp_ctx_copy,
    .fini = _dptcp_ctx_clean,
};

const dpele_type_t* dptcp_server_type()
{
    return &_dptcp_server_type;
}

static dpret_t _dptcp_listen_vopen(void* udata, va_list vlist)
{
    const char* host = va_arg(vlist, const char*);
    int port = va_arg(vlist, int);
    int backlog = va_arg(vlist, int);
    if (host == NULL || port <= 0 || port > UINT16_MAX) {
        return DPE_INVAL;
    }
    char portstr[8] = "";
    sprintf(portstr, "%d", port);

    struct addrinfo hints;
    struct addrinfo *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = 0;

    int r = getaddrinfo(host, portstr, &hints, &result);
    if (r != 0) {
        return -r;
    }

    int fd = -1;
    dptcp_listen_t* ios = (dptcp_listen_t*)udata;

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1)
            continue;

        int optval = 1;
        if (0 != setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval))) {
            close(fd);
            fd = -1;
            continue;
        }

        if (_set_nodelay(fd) && bind(fd, rp->ai_addr, rp->ai_addrlen) == 0
            && listen(fd, backlog) == 0) {
            memcpy(&ios->addr, rp->ai_addr, rp->ai_addrlen);
            ios->real = rp->ai_addrlen;
            break;
        } else {
            close(fd);
            fd = -1;
            continue;
        }
    }
    freeaddrinfo(result);
    return fd;
}

static dpele_type_t _dptcp_listen_type = {
    .name = "tcp_listen",
    .type = DPELE_TYPE_EFD,
    .size = sizeof(dptcp_listen_t),
    .iotype = DPAIO_TYPE_SKT,
    .events = DPEVT_IN,
    .init = _dptcp_listen_vopen,
    .copy = _dptcp_ctx_copy,
    .fini = _dptcp_ctx_clean,
};

const dpele_type_t* dptcp_listen_type()
{
    return &_dptcp_listen_type;
}

const char* dptcp_addr(dpele_t* efd)
{
    const dpele_type_t* type = dpele_type(efd);
    if (type == dptcp_listen_type()) {
        dptcp_listen_t* ios = (dptcp_listen_t*)dpele_aux_data(efd);
        return get_address((struct sockaddr*)&ios->addr, ios->real);
    } else if (type == dptcp_server_type()) {
        dptcp_data_t* ios = (dptcp_data_t*)dpele_aux_data(efd);
        return get_address((struct sockaddr*)&ios->addr, ios->real);
    } else if (type == dptcp_client_type()) {
        dptcp_data_t* ios = (dptcp_data_t*)dpele_aux_data(efd);
        return get_address((struct sockaddr*)&ios->addr, ios->real);
    } else {
        return NULL;
    }
}

const char* dptcp_peeraddr(dpele_t* efd)
{
    const dpele_type_t* type = dpele_type(efd);
    if (type == dptcp_server_type()) {
        struct sockaddr_storage sockaddr;
        socklen_t socklen = sizeof(sockaddr);
        if (-1
            == getpeername(dpefd_fd(efd), (struct sockaddr*)&sockaddr, &socklen)) {
            return "";
        }
        return get_address((struct sockaddr*)&sockaddr, socklen);
    } else if (type == dptcp_client_type()) {
        dptcp_data_t* ios = (dptcp_data_t*)dpele_aux_data(efd);
        return get_address((struct sockaddr*)&ios->addr, ios->real);
    } else {
        return NULL;
    }
}

bool dptcp_set_keepalive(dpele_t* efd, int idle, int intvl, int cnt)
{
    if (dpele_type(efd) == dptcp_listen_type()) {
        return false;
    }

    int fd = dpefd_fd(efd);
#ifdef __APPLE__
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &idle, sizeof(idle)) < 0) {
        return false;
    }
#else
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) < 0) {
        return false;
    }
#endif

    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl)) < 0) {
        return false;
    }

    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt)) < 0) {
        return false;
    }
    return true;
}

int dptcp_errno(dpele_t* efd)
{
    int fd = dpefd_fd(efd);
    int err;
    socklen_t len = sizeof(err);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) == -1) {
        return -1;
    } else {
        return err;
    }
}
