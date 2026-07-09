#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "dpapp/dpefd.h"
#include "dpapp/dpevp.h"
#include <stdarg.h>
#include <stdint.h>
#include <sys/socket.h>

void dpudp_addr_clean(dpsockaddr_t* addr);

typedef struct dpudp_client
{
    dpsockaddr_t local_addr;
    dpsockaddr_t peer_addr;
} dpudp_client_t;

typedef struct dpudp_server
{
    dpsockaddr_t local_addr;
} dpudp_server_t;

dpret_t dpqic_server_setopt(dpefd_t* efd);
dpret_t dpqic_client_setopt(dpefd_t* efd);

void _parse_contorl_msg(struct msghdr* msg, struct sockaddr_storage* storage,
    uint32_t* n_dropped, int* ecn);

enum ctl_what
{
    CW_SENDADDR = 1 << 0,
    CW_ECN = 1 << 1,
};
void _setup_control_msg(struct msghdr* msg, enum ctl_what cw,
    struct sockaddr* local_sa, struct sockaddr* dest_sa, int ecn);

#ifdef __cplusplus
}
#endif
