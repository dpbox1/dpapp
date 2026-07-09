/** @file dpcwc_aux.h
 *  @ingroup dpcwc_aux
 *  @brief dpcwc 扩展：Snowflake ID、多 listener server、CTC 与 endpoint 工厂。
 *
 *  endpoint 工厂（tcp / domain socket / ssl）在内部组合 `dpele_new` 与
 *  `dpcwc_aexec`；qic 监听/客户端直接用 `dpele_new(dpqic_*_type(), ...)`。
 *  `listen` 仅同步创建 listener 元素。 */
#pragma once

#include "dpcwc.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include "dpaco/dpaco.h"
#include "dpapp/dpevp.h"
#include "dpapp/dpssl.h"
#include <stdbool.h>
#include <stdint.h>

/** @brief 生成单调 Snowflake ID；`out_str` 非 NULL 时写入 11 字符 base62 串。 */
uint64_t dpcwc_id_next(char* out_str);
/** @brief 解析 base62 字符串 ID。 */
uint64_t dpcwc_id_2u64(const char* str);
/** @brief 渲染 ID 到 `out_str`（至少 12 字节，含 '\0'）。 */
void dpcwc_id_2str(uint64_t id, char* out_str);

/** @brief `dpcwc_server_param_t::start` 协程入参（栈上传递）。
 *  @note `start` 内须先拷贝本结构再 await/yield。 */
typedef struct
{
    dpele_t* peer; ///< 已 accept 的连接元素（tcp/uds/ssl 为 efd，qic 为 conn）
    dpv64_t args; ///< 来自 `start_args`
} dpcwc_server_start_args_t;

/** @brief 单个 listener 配置。 */
typedef struct
{
    const char* type; ///< "tcp" | "uds" | "qic"
    const char* host; ///< tcp/qic 绑定地址；`type=="uds"` 时为 socket 路径
    int port;         ///< tcp/qic 端口；domain socket 忽略
    const char* ssl;  ///< dpssl 组名；NULL 表示不使用 SSL（qic 必填）

    dpaco_fun_t
        start; ///< `dpv64_t fn(dpv64_t)`，入参为 `dpcwc_server_start_args_t*`
    dpv64_t start_args;
} dpcwc_server_param_t;

/** @brief 启动 `params` 中全部 listener；数组以 `{ .type = NULL }` 哨兵结束。
 *  @return 成功启动的 listener 数量（>=0）；单项失败跳过并打日志。 */
dpret_t dpcwc_server_start(const dpcwc_server_param_t* params);

/** @brief CTC 入口：`arg.ptr` 指向 `dpcwc_server_param_t` 数组。 */
dpret_t dpcwc_server_start_with_task(dpctc_t* task, dpv64_t arg);

/** @brief 创建 CTC、派发至 `toid`、await 完成后删除 CTC 元素。 */
dpret_t dpcwc_ctc_once(int toid, dpcwc_call_f func, dpv64_t ro_req);

/** @brief 向同 type 全部 worker 广播 CTC 并依次 await。 */
dpret_t dpcwc_ctc_each(int totype, dpcwc_call_f func, dpv64_t ro_req);

/** @brief 派发 CTC 至 `toid` 但不等待（元素 set_detach）。 */
dpret_t dpcwc_ctc_detach(int toid, dpcwc_call_f func, dpv64_t req);

/** @brief CTC 执行中续传至 `toid`；更新回调与参数后由 `dpevp_end_ctc_` 转发。 */
dpret_t dpcwc_ctc_reto(dpctc_t* task, int toid, dpcwc_call_f func, dpv64_t arg);

/** @name TCP */
/**@{*/
/** @brief 创建 client 并 `dpskt_connect`（内部 await）。 */
dpret_t dpcwc_tcp_client(const char* host, int port, dpefd_t** client);
/** @brief 同步创建 listener（不 await）。 */
dpret_t dpcwc_tcp_listen(const char* addr, int port, int backlog,
    dpefd_t** listener);
/** @brief accept 并包装为 `dptcp_server_type`（内部 await）。 */
dpret_t dpcwc_tcp_accept(dpefd_t* listener, dpefd_t** server);
/**@}*/

/** @name Domain socket */
/**@{*/
/** @brief 创建 UDS client 并 connect（内部 await）。
 *  @param path   socket 路径
 *  @param client 成功时输出 client efd
 *  @return `dpret_t` */
dpret_t dpcwc_uds_client(const char* path, dpefd_t** client);
/** @brief 同步创建 UDS listener。
 *  @param path     socket 路径
 *  @param listener 成功时输出 listener efd
 *  @return `dpret_t` */
dpret_t dpcwc_uds_listen(const char* path, dpefd_t** listener);
/** @brief accept 并包装为 `dpuds_server_type`（内部 await）。
 *  @param listener 监听 efd
 *  @param server   成功时输出连接 efd
 *  @return `dpret_t` */
dpret_t dpcwc_uds_accept(dpefd_t* listener, dpefd_t** server);
/**@}*/

#if DPAPP_HAS_SSL
/** @name TLS */
/**@{*/
/** @brief 创建 TLS client：TCP connect + SSL 握手（内部 await）。
 *  @param host   目标主机
 *  @param port   目标端口
 *  @param group  dpssl 组名
 *  @param sni    SNI；NULL 时用 host
 *  @param client 成功时输出 SSL efd
 *  @return `dpret_t` */
dpret_t dpcwc_ssl_client(const char* host, int port, const char* group,
    const char* sni, dpefd_t** client);
/** @brief accept TCP 连接并完成 TLS 握手（内部 await）。
 *  @param listener TCP listener efd
 *  @param group    dpssl 组名
 *  @param server   成功时输出 SSL server efd
 *  @return `dpret_t` */
dpret_t dpcwc_ssl_accept(dpefd_t* listener, const char* group, dpefd_t** server);
/**@}*/
#endif

#ifdef __cplusplus
}
#endif
