/** @file dpqic.h
 *  @ingroup dpapp_qic
 *  @brief QUIC / HTTP3 API（lsquic）。
 *
 *  连接、流、HTTP/3 头集与异步 I/O；示例见 echo_svr / http_svr。 */

#pragma once

#include "dpapp/dpefd.h"
#include "dpapp/dpevp.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @name Engine 与元素类型 */
/**@{*/

/** @brief 编译期是否启用 QUIC（依赖 lsquic 与 TLS）。
 *  @return 已启用 true，否则 false。 */
bool dpqic_enable();

/** @brief 每 worker 线程调用一次，初始化 lsquic 线程上下文。
 *  @return @ref DPE_OK 或错误码。 */
dpret_t dpqic_thrd_init();
/** @brief 每 worker 线程退出时释放本线程 engine 与 lsquic 全局引用。 */
void dpqic_thrd_exit();

/** @brief lsquic `lsquic_engine_settings` 不透明别名。 */
typedef struct lsquic_engine_settings dpqic_engine_settings_t;

/** @brief 为 ssl group 注册 engine；group 须已由 dpssl_add 创建，role 从 group
 * 读取，且组必须为TLS 1.3。 */
dpret_t dpqic_add_engine(const char* group, dpqic_engine_settings_t* settings_);
/** @brief 删除空闲 engine；group 为 NULL 时删除全部空闲 engine。 */
dpret_t dpqic_del_engine(const char* group);

/** @brief `dpele_new(dpqic_listen_type(), group, host, port)` — QUIC 监听。 */
const dpele_type_t* dpqic_listen_type();
/** @brief `dpele_new(dpqic_client_type(), group, host, port)` — QUIC 客户端。 */
const dpele_type_t* dpqic_client_type();

/** @brief 内部：`dpele_new(dpqic_conect_type(), lsquic_conn*)` — QUIC 连接元素。 */
const dpele_type_t* dpqic_conect_type();
/** @brief 内部：`dpele_new(dpqic_stream_type(), lsquic_stream*)` — QUIC 流元素。 */
const dpele_type_t* dpqic_stream_type();

/** @} */

/** @name dpasc QUIC prep */
/**@{*/
/** @brief QUIC 发起连接。
 *  参数： `(const char* sni, const char* token, dpele_t** conn_out)` */
const dpasc_t* dpqic_connect();
/** @brief QUIC 接受连接。
 *  参数： `(dpele_t** conn_out)` */
const dpasc_t* dpqic_accept();
/** @brief QUIC 获取或创建流。
 *  参数： `(dpele_t** stm_out, bool create_new)` */
const dpasc_t* dpqic_stream();
/** @brief QUIC recv。
 *  参数： `(void* buf, int len)` */
const dpasc_t* dpqic_recv();
/** @brief QUIC send。
 *  参数： `(const void* buf, int len)` */
const dpasc_t* dpqic_send();
/** @brief QUIC recvv，scatter 接收。
 *  参数： `(struct iovec* iov, int iovcnt)` */
const dpasc_t* dpqic_recvv();
/** @brief QUIC sendv，gather 发送。
 *  参数： `(struct iovec* iov, int iovcnt)` */
const dpasc_t* dpqic_sendv();
/** @brief QUIC 接收 HTTP/3 头集。
 *  参数： `(dpqic_hdrset_t** hdrset_out)` */
const dpasc_t* dpqic_recv_hdrset();
/** @brief QUIC 发送 HTTP/3 头集。
 *  参数： `(const dpqic_hdrset_t* hdrset)` */
const dpasc_t* dpqic_send_hdrset();
/**@}*/

/** @name HTTP/3 头集 */
/**@{*/

/** @brief HTTP/3 头集不透明类型。 */
typedef struct dpqic_hdrset dpqic_hdrset_t;

/** @brief 创建 HTTP/3 头集；`pre_size` 为初始槽位数（≤0 时用 2）。
 *  @return 新头集；失败返回 NULL 并设 errno。 */
dpqic_hdrset_t* dpqic_hdrset_new(int pre_size);
/** @brief 按名取头字段值；未找到返回 NULL。 */
const char* dpqic_hdrset_get(const dpqic_hdrset_t* hdrset, const char* name);
/** @brief 设置或追加头字段；`value` 为 NULL 时删除已有同名项。
 *  @return @ref DPE_OK 或错误码。 */
dpret_t dpqic_hdrset_set(dpqic_hdrset_t* phdrset, const char* name,
    const char* value);
/** @brief 释放头集及其缓冲区。 */
void dpqic_hdrset_del(dpqic_hdrset_t* hdrset);
/** @brief 当前头字段个数；hdrset 无效时返回 @ref DPE_INVAL 。 */
dpret_t dpqic_hdrset_count(const dpqic_hdrset_t* hdrset);
/** @brief 按索引取头字段值；`name` 非 NULL 时写入字段名。
 *  @return 字段值；索引越界或 `hdrset` 无效时返回 NULL。 */
const char* dpqic_hdrset_at(const dpqic_hdrset_t* hdrset, int index,
    const char** name);

/** @brief 取底层 lsquic stream（调试用）。 */
typedef struct lsquic_stream lsquic_stream_t;
/** @brief 取底层 lsquic connection（调试用）。 */
typedef struct lsquic_conn lsquic_conn_t;

/** @brief 取流元素对应的底层 lsquic_stream；非流元素返回 NULL。 */
lsquic_stream_t* dpqic_original_stream(dpele_t* ele);
/** @brief 取连接元素对应的底层 lsquic_conn；非连接元素返回 NULL。 */
lsquic_conn_t* dpqic_original_conn(dpele_t* ele);

/** @} */

#ifdef __cplusplus
}
#endif
