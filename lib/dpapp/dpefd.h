/** @file dpefd.h
 *  @ingroup dpapp_efd
 *  @brief 事件驱动 fd 工厂与地址辅助结构。
 *
 *  封装 TCP/UDP/domain socket/signalfd/inotify/FIFO/串口等，供 `dpele_new` 使用。
 *  异步 I/O 通过 `dpevp_add(ele, dpasc_*, ...)` 提交，参数见 `dpasc.h`。 */

#pragma once

#include "dpapp/dpevp.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>
#include <sys/socket.h>

/** @brief 通用 sockaddr 存储头（`addr` + 有效长度 `real`）。 */
#define DPSOCKADDR_HEAD                                                             \
    struct sockaddr_storage addr;                                                   \
    socklen_t real;

typedef struct
{
    DPSOCKADDR_HEAD
} dpsockaddr_t;

/** @name TCP */
/**@{*/
/** @brief `dpele_new(dptcp_listen_type(), host, port, backlog)` — 监听套接字。 */
const dpele_type_t* dptcp_listen_type();
/** @brief `dpele_new(dptcp_client_type(), host, port)` — 主动连接客户端。 */
const dpele_type_t* dptcp_client_type();
/** @brief `dpele_new(dptcp_server_type(), fd, addr)` — accept 得到的已连接 fd。 */
const dpele_type_t* dptcp_server_type();

/** @brief 本地绑定地址字符串；未知类型返回 NULL。 */
const char* dptcp_addr(dpefd_t* efd);
/** @brief 对端地址字符串；未知类型返回 NULL。 */
const char* dptcp_peeraddr(dpefd_t* efd);
/** @brief 设置 TCP keepalive（idle/intvl/cnt 秒/秒/次）。
 *  @return 成功 true。 */
bool dptcp_set_keepalive(dpefd_t* efd, int idle, int intvl, int cnt);
/** @brief 最近一次 socket 相关 errno；无错误时返回 0。 */
int dptcp_errno(dpefd_t* efd);
/**@}*/

/** @name Domain socket（Unix domain） */
/**@{*/
/** @brief `dpele_new(dpuds_listen_type(), path)` — 监听端（bind + listen）。 */
const dpele_type_t* dpuds_listen_type();
/** @brief `dpele_new(dpuds_client_type(), path)` — 客户端（connect 前）。 */
const dpele_type_t* dpuds_client_type();
/** @brief `dpele_new(dpuds_server_type(), fd, addr)` — accept 得到的已连接 fd。 */
const dpele_type_t* dpuds_server_type();
/**@}*/

/** @name UDP */
/**@{*/
/** @brief `dpele_new(dpudp_server_type(), host, port)` —
 * 绑定本地地址（SO_REUSEPORT）。 */
const dpele_type_t* dpudp_server_type();
/** @brief `dpele_new(dpudp_client_type(), host, port)` — 带默认对端的 UDP 客户端。
 */
const dpele_type_t* dpudp_client_type();

/** @brief UDP 本地绑定地址字符串。 */
const char* dpudp_addr(dpefd_t* efd);
/** @brief UDP 默认对端地址字符串。 */
const char* dpudp_peeraddr(dpefd_t* efd);
/**@}*/

/** @name 串口 */
/**@{*/
/** @brief `dpele_new(dpspd_type(), path, baud, databits, stopbits, parity)`。
 *  `parity` 为 `'N'`/`'E'`/`'O'`（int 传入）；其余 ≤0 用默认值。 */
const dpele_type_t* dpspd_type();

/** @brief 串口设备路径。 */
const char* dpspd_device(dpefd_t* self);
/** @brief 波特率。 */
int dpspd_baud(dpefd_t* self);
/** @brief 数据位。 */
int dpspd_databits(dpefd_t* self);
/** @brief 校验位（`'N'`/`'E'`/`'O'`）。 */
char dpspd_parity(dpefd_t* self);
/** @brief 停止位。 */
int dpspd_stopbits(dpefd_t* self);
/**@}*/

/** @name signalfd */
/**@{*/
/** @brief `dpele_new(dpsig_type())` — 空 mask 的 signalfd。 */
const dpele_type_t* dpsig_type();
/** @brief 向 signalfd 掩码追加信号号。
 *  @return @ref DPE_OK 或错误码。 */
dpret_t dpsig_addno(dpefd_t* self, int signo);
/** @brief 从 signalfd 掩码移除信号号。
 *  @return @ref DPE_OK 或错误码。 */
dpret_t dpsig_delno(dpefd_t* self, int signo);
/** @brief 信号号是否在掩码中。 */
bool dpsig_hasno(dpefd_t* self, int signo);
/**@}*/

/** @name inotify */
/**@{*/
/** @brief `dpele_new(dpfsm_type())` — inotify 实例。 */
const dpele_type_t* dpfsm_type();
/** @brief 添加 inotify 监视路径。
 *  @return 成功时 watch descriptor；失败返回负 @ref dpret_t 。 */
dpret_t dpfsm_addev(dpefd_t* self, const char* path, uint32_t mask);
/** @brief 移除 inotify 监视项。
 *  @return @ref DPE_OK 或错误码。 */
dpret_t dpfsm_delev(dpefd_t* self, int wd);
/**@}*/

/** @name 命名管道（FIFO） */
/**@{*/
/** @brief `dpele_new(dppip_type(), path, dpevt, unlink_on_del)`。
 *  `dpevt` 仅 `DPEVT_IN`（读）或 `DPEVT_OUT`（写）；不存在则 mkfifo。 */
const dpele_type_t* dppip_type();
/** @brief 命名管道路径。 */
const char* dppip_path(dpefd_t* efd);
/**@}*/

/** @name 通用 */
/**@{*/

/** @brief 切换 fd 阻塞/非阻塞。
 *  @return 成功 @ref DPE_OK 。 */
dpret_t dpefd_set_block(dpefd_t* efd, bool block);

/**@}*/

#ifdef __cplusplus
}
#endif
