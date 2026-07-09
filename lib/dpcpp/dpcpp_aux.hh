/** @file dpcpp_aux.hh
 *  @ingroup dpcpp_aux
 *  @brief Snowflake ID、多 listener server、CTC 与 endpoint 工厂（对齐 dpcwc_aux）。
 *
 *  endpoint 返回 `aco_efd`（失败时 co_return 错误码或 nullptr）；内部使用
 *  `aexec` / `co_await`。qic 监听/客户端直接用 `dpele_new(dpqic_*_type(), ...)`。
 *  server 连接回调为 `aco_callback`，与 dpcwc 的
 *  `dpaco_fun_t` 签名不同。 */
#pragma once

#include "dpapp/dpefd.h"
#include "dpapp/dpevp.h"
#include "dpapp/dpqic.h"
#include "dpapp/dpret.h"
#include "dpapp/dpssl.h"
#include "dpcpp/dpcpp_aco.hh"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace dpcpp
{

/** @brief 生成 Snowflake ID；`out_str` 可选，写入 11 字符 base62。 */
uint64_t next_id(char* out_str = nullptr);
/** @brief 解析 base62 字符串 ID。
 *  @param str base62 编码串
 *  @return 64 位 ID */
uint64_t id_from_string(const char* str);
/** @brief 渲染 ID 到 base62 字符串。
 *  @param id      Snowflake ID
 *  @param out_str 输出缓冲，至少 12 字节（含 '\0'） */
void id_to_string(uint64_t id, char* out_str);

namespace server
{

/** @brief 单个 listener 配置（字段语义同 `dpcwc_server_param_t`）。 */
struct parameter
{
    std::string type = "tcp";       ///< "tcp" | "qic" | "uds"
    std::string host = "127.0.0.1"; ///< uds 时为 socket 路径
    int port = 4326;
    std::string ssl; ///< dpssl 组名；空表示不使用 SSL（qic 必填）

    /** @brief 每连接协程：`fn(peer, start_args)`，`peer` 为 accept 得到的元素。 */
    dpcpp::aco_callback start = nullptr;
    dpv64_t start_args = DPV64_NULL;
};
/** @brief listener 配置列表（`server::start` 入参）。 */
using parameters = std::vector<parameter>;

/** @brief 启动 `params` 中全部 listener；返回成功启动数量。 */
dpcpp::aco_ret start(const parameters& params);

/** @brief CTC 恢复入口：`spec.ptr` 指向 `parameters*`（asc scratch）。 */
aco_ret start_with_task(dpele_t* task, dpv64_t unused, dpv64_t spec);

} // namespace server

/** @brief 单次 CTC：创建、派发、await、删除。 */
aco_ret ctc_once(int toid, aco_callback func, dpv64_t req);

/** @brief 向同 type 全部 worker 广播并依次 await。 */
aco_ret ctc_each(int totype, aco_callback func, dpv64_t req);

/** @brief 派发 CTC 不等待（detach）。 */
aco_ret ctc_detach(int toid, aco_callback func, dpv64_t req);

/** @brief CTC 执行中续传至 `toid`；更新回调与参数后由 `dpevp_end_ctc_` 转发。 */
aco_ret ctc_reto(dpele_t* task, int toid, aco_callback func, dpv64_t req);

/** @name TCP */
/**@{*/
/** @brief 创建 TCP client 并 connect（内部 co_await）。
 *  @return 成功时 client efd，失败时 co_return 错误码或 nullptr */
aco_efd tcp_client(const char* host, int port);
/** @brief 同步创建 TCP listener。
 *  @return listener efd 或错误 */
aco_efd tcp_listen(const char* addr, int port, int backlog = SOMAXCONN);
/** @brief accept 并包装为 server efd（内部 co_await）。 */
aco_efd tcp_accept(dpefd_t* listener);

/** @} */

/** @name Domain socket */
/**@{*/

/** @brief 创建 UDS client 并 connect（内部 co_await）。 */
aco_efd uds_client(const char* path);
/** @brief 同步创建 UDS listener。 */
aco_efd uds_listen(const char* path);
/** @brief accept UDS 连接（内部 co_await）。 */
aco_efd uds_accept(dpefd_t* listener);
/**@}*/

#if DPAPP_HAS_SSL
/** @name TLS */
/**@{*/
/** @brief 创建 TLS client：TCP connect + SSL 握手（内部 co_await）。
 *  @param group dpssl 组名
 *  @param sni   SNI；nullptr 时用 host */
aco_efd ssl_client(const char* host, int port, const char* group, const char* sni);
/** @brief accept TCP 并完成 TLS 握手（内部 co_await）。 */
aco_efd ssl_accept(dpefd_t* listener, const char* group);
/**@}*/
#endif

} // namespace dpcpp
