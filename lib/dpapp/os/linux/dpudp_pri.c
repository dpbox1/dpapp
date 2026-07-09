#include "dpudp_pri.h"
#include "dpapp/dpret.h"
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <sys/socket.h>

dpret_t dpqic_server_setopt(dpefd_t* efd)
{
    int fd = dpefd_fd(efd);
    int on = 1;
    dpudp_server_t* usvr = (dpudp_server_t*)dpele_aux_data(efd);

    if (AF_INET6 == usvr->local_addr.addr.ss_family) {
        int mton = IP_PMTUDISC_PROBE;
        if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) != 0
            || setsockopt(fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on)) != 0
            || setsockopt(fd, IPPROTO_IPV6, IPV6_RECVTCLASS, &on, sizeof(on)) != 0
            || setsockopt(fd, IPPROTO_IPV6, IPV6_TCLASS, &on, sizeof(on)) != 0
            || setsockopt(fd, IPPROTO_IPV6, IPV6_MTU_DISCOVER, &mton, sizeof(mton))
                != 0) {
            return -errno;
        }
    } else {
        int flag1 = 0;
        int flag2 = 0;
        int mton = IP_PMTUDISC_PROBE;
#ifdef IP_RECVORIGDSTADDR
        flag1 = IP_RECVORIGDSTADDR;
#else
        flag1 = IP_PKTINFO;
#endif
#ifdef SO_RXQ_OVFL
        flag2 = SO_RXQ_OVFL;
#endif
        if ((flag1 && setsockopt(fd, IPPROTO_IP, flag1, &on, sizeof(on)) != 0)
            || (flag2 && setsockopt(fd, SOL_SOCKET, flag2, &on, sizeof(on)) != 0)
            || setsockopt(fd, IPPROTO_IP, IP_RECVTOS, &on, sizeof(on)) != 0
            || setsockopt(fd, IPPROTO_IP, IP_TOS, &on, sizeof(on)) != 0
            || setsockopt(fd, IPPROTO_IP, IP_MTU_DISCOVER, &mton, sizeof(mton))
                != 0) {
            return -errno;
        }
    }
    return DPE_OK;
}

dpret_t dpqic_client_setopt(dpefd_t* efd)
{
    int fd = dpefd_fd(efd);
    int on = 1;
    dpudp_client_t* ucet = (dpudp_client_t*)dpele_aux_data(efd);

    int peer_family = ((struct sockaddr_storage*)&ucet->peer_addr.addr)->ss_family;
    if (AF_INET6 == peer_family) {
        int mton = IP_PMTUDISC_PROBE;
        if (setsockopt(fd, IPPROTO_IPV6, IPV6_RECVTCLASS, &on, sizeof(on)) != 0
            || setsockopt(fd, IPPROTO_IPV6, IPV6_TCLASS, &on, sizeof(on)) != 0
            || setsockopt(fd, IPPROTO_IPV6, IPV6_MTU_DISCOVER, &mton, sizeof(mton))
                != 0) {
            return -errno;
        }
    } else {
        int mton = IP_PMTUDISC_PROBE;
        if (setsockopt(fd, IPPROTO_IP, IP_RECVTOS, &on, sizeof(on)) != 0
            || setsockopt(fd, IPPROTO_IP, IP_TOS, &on, sizeof(on)) != 0
            || setsockopt(fd, IPPROTO_IP, IP_MTU_DISCOVER, &mton, sizeof(mton))
                != 0) {
            return -errno;
        }
    }
    return DPE_OK;
}

void _parse_contorl_msg(struct msghdr* msg, struct sockaddr_storage* storage,
    uint32_t* n_dropped, int* ecn)
{
    const struct in6_pktinfo* in6_pkt;
    struct cmsghdr* cmsg;

    for (cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {
        if (cmsg->cmsg_level == IPPROTO_IP
            && cmsg->cmsg_type ==
#ifdef IP_ORIGDSTADDR
                IP_ORIGDSTADDR
#else
                IP_PKTINFO
#endif
        ) {
#ifdef IP_ORIGDSTADDR
            memcpy(storage, CMSG_DATA(cmsg), sizeof(struct sockaddr_in));
#else
            const struct in_pktinfo* in_pkt = (void*)CMSG_DATA(cmsg);
            ((struct sockaddr_in*)storage)->sin_addr = in_pkt->ipi_addr;
#endif
        } else if (cmsg->cmsg_level == IPPROTO_IPV6
            && cmsg->cmsg_type == IPV6_PKTINFO) {
            in6_pkt = (void*)CMSG_DATA(cmsg);
            ((struct sockaddr_in6*)storage)->sin6_addr = in6_pkt->ipi6_addr;
#ifdef SO_RXQ_OVFL
        } else if (cmsg->cmsg_level == SOL_SOCKET
            && cmsg->cmsg_type == SO_RXQ_OVFL) {
            memcpy(n_dropped, CMSG_DATA(cmsg), sizeof(*n_dropped));
#endif
        } else if ((cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_TOS)
            || (cmsg->cmsg_level == IPPROTO_IPV6
                && cmsg->cmsg_type == IPV6_TCLASS)) {
            *ecn = 0;
            memcpy(ecn, CMSG_DATA(cmsg),
                cmsg->cmsg_len - sizeof(struct cmsghdr) < sizeof(*ecn)
                    ? cmsg->cmsg_len - sizeof(struct cmsghdr)
                    : sizeof(*ecn));
            *ecn &= IPTOS_ECN_MASK;
        }
    }
}

void _setup_control_msg(struct msghdr* msg, enum ctl_what cw,
    struct sockaddr* local_sa, struct sockaddr* dest_sa, int ecn)
{
    struct cmsghdr* cmsg;
    struct sockaddr_in* local_sai;
    struct sockaddr_in6* local_sa6;
    struct in_pktinfo info;
    struct in6_pktinfo info6;
    size_t ctl_len = 0;

    for (cmsg = CMSG_FIRSTHDR(msg); cw && cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {
        if (cw & CW_SENDADDR) {
            if (AF_INET == dest_sa->sa_family) {
                local_sai = (struct sockaddr_in*)local_sa;
                memset(&info, 0, sizeof(info));
                info.ipi_spec_dst = local_sai->sin_addr;
                cmsg->cmsg_level = IPPROTO_IP;
                cmsg->cmsg_type = IP_PKTINFO;
                cmsg->cmsg_len = CMSG_LEN(sizeof(info));
                ctl_len += CMSG_SPACE(sizeof(info));
                memcpy(CMSG_DATA(cmsg), &info, sizeof(info));
            } else {
                local_sa6 = (struct sockaddr_in6*)local_sa;
                memset(&info6, 0, sizeof(info6));
                info6.ipi6_addr = local_sa6->sin6_addr;
                cmsg->cmsg_level = IPPROTO_IPV6;
                cmsg->cmsg_type = IPV6_PKTINFO;
                cmsg->cmsg_len = CMSG_LEN(sizeof(info6));
                memcpy(CMSG_DATA(cmsg), &info6, sizeof(info6));
                ctl_len += CMSG_SPACE(sizeof(info6));
            }
            cw &= ~CW_SENDADDR;
        } else if (cw & CW_ECN) {
            if (AF_INET == dest_sa->sa_family) {
                const int tos = ecn;
                cmsg->cmsg_level = IPPROTO_IP;
                cmsg->cmsg_type = IP_TOS;
                cmsg->cmsg_len = CMSG_LEN(sizeof(tos));
                memcpy(CMSG_DATA(cmsg), &tos, sizeof(tos));
                ctl_len += CMSG_SPACE(sizeof(tos));
            } else {
                const int tos = ecn;
                cmsg->cmsg_level = IPPROTO_IPV6;
                cmsg->cmsg_type = IPV6_TCLASS;
                cmsg->cmsg_len = CMSG_LEN(sizeof(tos));
                memcpy(CMSG_DATA(cmsg), &tos, sizeof(tos));
                ctl_len += CMSG_SPACE(sizeof(tos));
            }
            cw &= ~CW_ECN;
        } else {
            msg->msg_controllen = 0;
            return;
        }
    }

    msg->msg_controllen = ctl_len;
}
