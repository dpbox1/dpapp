#include "dpapp/dpefd.h"
#include "dpapp/dpevp.h"
#include "dpapp/dplog.h"
#include "dpapp/dpret.h"
#include "dpudp_pri.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static __thread char _g_udpaddr_tmp[128] = {0};

static const char* _get_address(const struct sockaddr* sockAddr, socklen_t sockLen)
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

    snprintf(_g_udpaddr_tmp, sizeof(_g_udpaddr_tmp), "udp://%s:%s", ipStr, portStr);
    return _g_udpaddr_tmp;
}

void dpudp_addr_clean(dpsockaddr_t* addr)
{
    (void)addr;
}

dpret_t dpudp_client_vopen(void* udata, va_list varg)
{
    const char* host = va_arg(varg, const char*);
    int port = va_arg(varg, int);

    if (host == NULL || port <= 0 || port > UINT16_MAX) {
        return DPE_INVAL;
    }

    char portstr[8] = "";
    snprintf(portstr, sizeof(portstr), "%d", port);

    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    int r = getaddrinfo(host, portstr, &hints, &result);
    if (r != 0) {
        return -r;
    }

    dpudp_client_t* ucet = (dpudp_client_t*)udata;
    int fd = -1;

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1) {
            continue;
        }

        memcpy(&ucet->peer_addr.addr, rp->ai_addr, rp->ai_addrlen);
        ucet->peer_addr.real = rp->ai_addrlen;
        break;
    }

    freeaddrinfo(result);

    dpret_t ret = DPE_OK;
    socklen_t socklen;
    union
    {
        struct sockaddr_in sin;
        struct sockaddr_in6 sin6;
    } u;
    struct sockaddr* sa_local = (struct sockaddr*)&u;
    struct sockaddr* sa_peer = (struct sockaddr*)&ucet->peer_addr.addr;
    switch (sa_peer->sa_family) {
    case AF_INET:
        socklen = sizeof(struct sockaddr_in);
        u.sin.sin_family = AF_INET;
        u.sin.sin_addr.s_addr = INADDR_ANY;
        u.sin.sin_port = 0;
        break;
    case AF_INET6:
        socklen = sizeof(struct sockaddr_in6);
        memset(&u.sin6, 0, sizeof(u.sin6));
        u.sin6.sin6_family = AF_INET6;
        break;
    default:
        errno = DPE_INVAL;
        goto ERROR;
    }

    if (0 != bind(fd, sa_local, socklen)) {
        goto ERROR;
    }

    if (0 != connect(fd, sa_peer, ucet->peer_addr.real)) {
        goto ERROR;
    }

    if (0 != getsockname(fd, sa_local, &socklen)) {
        goto ERROR;
    }

    ucet->local_addr.real = socklen;
    memcpy((void*)&ucet->local_addr.addr, sa_local, ucet->local_addr.real);

    return fd;

ERROR:
    ret = -errno;
    close(fd);
    return ret;
}

void dpudp_client_clean(void* udata)
{
    dpudp_client_t* ucet = (dpudp_client_t*)udata;
    dpudp_addr_clean(&ucet->peer_addr);
    dpudp_addr_clean(&ucet->local_addr);
}

static dpret_t _dpudp_client_copy(void* dst, const void* src)
{
    memcpy(dst, src, sizeof(dpudp_client_t));
    return DPE_OK;
}

static dpele_type_t _dpudp_client_type = {
    .name = "udp_client",
    .type = DPELE_TYPE_EFD,
    .size = sizeof(dpudp_client_t),
    .iotype = DPAIO_TYPE_SKT,
    .events = DPEVT_IN | DPEVT_OUT,
    .init = dpudp_client_vopen,
    .copy = _dpudp_client_copy,
    .fini = dpudp_client_clean,
};

const dpele_type_t* dpudp_client_type()
{
    return &_dpudp_client_type;
}

dpret_t dpudp_server_vopen(void* udata, va_list varg)
{
    const char* host = va_arg(varg, const char*);
    int port = va_arg(varg, int);

    if (host == NULL || port <= 0 || port > UINT16_MAX) {
        return DPE_INVAL;
    }

    char portstr[8] = "";
    snprintf(portstr, sizeof(portstr), "%d", port);

    struct addrinfo hints;
    struct addrinfo *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = 0;

    int r = getaddrinfo(host, portstr, &hints, &result);
    if (r != 0) {
        return -r;
    }

    int fd = -1;
    int on = 1;
    dpudp_server_t* usvr = (dpudp_server_t*)udata;

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1) {
            continue;
        }

        if (0 != setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on))) {
            close(fd);
            fd = -1;
            continue;
        }

        if (bind(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            memcpy(&usvr->local_addr.addr, rp->ai_addr, rp->ai_addrlen);
            usvr->local_addr.real = rp->ai_addrlen;
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

void dpudp_server_clean(void* udata)
{
    dpudp_server_t* ucet = (dpudp_server_t*)udata;
    dpudp_addr_clean(&ucet->local_addr);
}

static dpret_t _dpudp_server_copy(void* dst, const void* src)
{
    memcpy(dst, src, sizeof(dpudp_server_t));
    return DPE_OK;
}

static dpele_type_t _dpudp_server_type = {
    .name = "udp_server",
    .type = DPELE_TYPE_EFD,
    .size = sizeof(dpudp_server_t),
    .iotype = DPAIO_TYPE_SKT,
    .events = DPEVT_IN | DPEVT_OUT,
    .init = dpudp_server_vopen,
    .copy = _dpudp_server_copy,
    .fini = dpudp_server_clean,
};

const dpele_type_t* dpudp_server_type()
{
    return &_dpudp_server_type;
}

const char* dpudp_addr(dpefd_t* efd)
{
    const dpele_type_t* type = dpele_type(efd);

    if (type == dpudp_server_type()) {
        dpudp_server_t* ios = (dpudp_server_t*)dpele_aux_data(efd);
        return _get_address((struct sockaddr*)&ios->local_addr.addr,
            ios->local_addr.real);
    } else if (type == dpudp_client_type()) {
        dpudp_client_t* ios = (dpudp_client_t*)dpele_aux_data(efd);
        return _get_address((struct sockaddr*)&ios->local_addr.addr,
            ios->local_addr.real);
    } else {
        return NULL;
    }
}

const char* dpudp_peeraddr(dpefd_t* efd)
{
    const dpele_type_t* type = dpele_type(efd);

    if (type == dpudp_client_type()) {
        dpudp_client_t* ios = (dpudp_client_t*)dpele_aux_data(efd);
        return _get_address((struct sockaddr*)&ios->peer_addr.addr,
            ios->peer_addr.real);
    }

    return NULL;
}
