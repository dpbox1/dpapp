/** @file dpssl.h
 *  @ingroup dpapp_ssl
 *  @brief TLS 会话与 OpenSSL 上下文管理。
 *
 *  数据面经 `dpssl_*` + `dpevp_add` 异步调度；握手/关闭可能返回 DPE_WAIT
 *  需再次等待事件。示例见 `app/cwc/echo_svr`、`app/cpp/echo_svr`。 */

#pragma once

#include "dpapp/dpdef.h"
#include "dpapp/dpevp.h"
#include "dpapp/dpret.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
#include <sys/socket.h>

#if DPAPP_HAS_SSL
#include <openssl/ssl.h>
#endif

/** @brief OpenSSL `SSL` 会话不透明别名。 */
typedef struct ssl_st dpssl_ori_ssn_t;
/** @brief OpenSSL `SSL_CTX` 不透明别名。 */
typedef struct ssl_ctx_st dpssl_ori_ctx_t;

/** @name 组/证书/ALPN 配置 */
/**@{*/

/** @brief 编译期是否启用 TLS（DPAPP_HAS_SSL 宏）。
 *  @return 已启用 true，否则 false。 */
bool dpssl_enable();

/** @brief 每 worker 线程初始化 TLS 线程上下文。
 *  @return @ref DPE_OK 或错误码。 */
dpret_t dpssl_thrd_init();
/** @brief 每 worker 线程退出时释放本线程 TLS 组与上下文。 */
void dpssl_thrd_exit();

/** @brief 注册 TLS 协议组（版本范围、角色）。 */
dpret_t dpssl_add(const char* group, dprole_e role, uint16_t min_version,
    uint16_t max_version);
/** @brief 注销 TLS 组并释放其证书与 ALPN 配置。
 *  @param group 组名。
 *  @return @ref DPE_OK 或错误码。 */
dpret_t dpssl_del(const char* group);

/** @brief 查询 TLS 组角色；组不存在时返回 @ref DPROLE_UNSURE 。 */
dprole_e dpssl_role(const char* group);

/** @brief 为组追加 ALPN 协议标识（可多次注册）。
 *  @return @ref DPE_OK 或错误码。 */
dpret_t dpssl_add_alpn(const char* group, const char* alpn);
/** @brief 检查组是否已注册指定 ALPN。 */
dpret_t dpssl_has_alpn(const char* group, const char* alpn);

/** @brief 判断组 TLS 版本范围是否与 `[min_version, max_version]` 有交集。
 *  查询参数为 `0` 时表示不校验该边界。
 *  @return @ref DPE_OK 有交集；@ref DPE_NOTEXISTS 无交集或组不存在。 */
dpret_t dpssl_has_version(const char* group, uint16_t min_version,
    uint16_t max_version);

/** @brief 按索引取组内 ALPN；idx 越界或组不存在时返回 NULL。 */
const char* dpssl_get_alpn(const char* group, int idx);

/** @brief 为组注册 SNI 证书与私钥（PEM 路径）。
 *  @param sni 证书键；空串为内部默认槽；NULL 返回 @ref DPE_INVAL 。
 *  @return @ref DPE_OK 或错误码。 */
dpret_t dpssl_add_ctx(const char* group, const char* sni, const char* crt,
    const char* key);
/** @brief 取 SSL_CTX：`sni` 为 `NULL` 返回 group `default_ctx`；`""` 为默认证书槽；
 *  否则按 SNI 查命名 ctx，未命中回退默认证书槽。 */
dpssl_ori_ctx_t* dpssl_get_ctx(const char* group, const char* sni);
/** @brief 删除组内指定 SNI 的 SSL_CTX；空串为默认证书槽；NULL 返回 @ref DPE_INVAL 。
 *  @return @ref DPE_OK 或错误码。 */
dpret_t dpssl_del_ctx(const char* group, const char* sni);
/** @brief 删除组内全部 SNI 证书 ctx（保留组本身）。 */
void dpssl_del_all_ctx(const char* group);

/** @brief `dpele_new(dpssl_client_type(), tcp_efd, group, sni)` */
const dpele_type_t* dpssl_client_type();
/** @brief `dpele_new(dpssl_server_type(), tcp_efd, group)` */
const dpele_type_t* dpssl_server_type();

/** @} */

/** @name dpasc SSL prep */
/**@{*/
/** @brief TLS 握手 prep；无额外 @ref dpevp_add 参数。 */
const dpasc_t* dpssl_handshake();
/** @brief TLS 单向关闭 prep；无额外 @ref dpevp_add 参数。 */
const dpasc_t* dpssl_shutdown();
/** @brief SSL_read。
 *  参数： `(void* buf, int len)` */
const dpasc_t* dpssl_recv();
/** @brief SSL_write。
 *  参数： `(const void* buf, int len)` */
const dpasc_t* dpssl_send();
/**@}*/

#ifdef __cplusplus
}
#endif
