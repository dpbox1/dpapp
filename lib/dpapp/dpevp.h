/** @file dpevp.h
 *  @ingroup dpapp_evp
 *  @brief 事件元素 `dpele_t` 与每线程事件循环 `dpevp`。
 *
 *  `dpele_t` 统一表示异步工作项：
 *  - DPELE_TYPE_EFD：可 poll 的 fd（socket、pipe、signalfd 等）
 *  - DPELE_TYPE_USD：用户自驱动槽（QIC 等，无内核事件）
 *  - DPELE_TYPE_CTC：跨线程调用（用户回调、异步 syscall）
 *  - DPELE_TYPE_TMR：定时器 / sleep
 *
 *  典型流程：`dpevp_add` 提交 → `dpevp_pop` 取完成项（非 FIFO，就绪优先）。 */

#pragma once

#include "dpapp/dpdef.h"
#include "dpapp/dpret.h"
#include "dpapp_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdarg.h>
#include <stdint.h>
#include <sys/poll.h>

typedef enum
{
    DPELE_TYPE_EFD, ///< 事件驱动文件描述符
    DPELE_TYPE_USD, ///< 用户自驱动（QIC 等）
    DPELE_TYPE_CTC, ///< 跨线程调用
    DPELE_TYPE_TMR, ///< 定时器
} dpele_type_e;

typedef struct dpele dpele_t;
typedef struct dpele dpefd_t;
typedef struct dpele dpctc_t;
typedef struct dpele dpusd_t;
typedef struct dpele dptmr_t;

/** @brief 所有的IO类型 */
typedef enum
{
    DPAIO_TYPE_NAN = -1, ///< 非IO类型
    DPAIO_TYPE_GFD = 0,  ///< 通用fd read/write等（含部分 socket）,
    DPAIO_TYPE_SKT,      ///< 专用于socket send/recv/sendmsg/recvmsg
    DPAIO_TYPE_SSL,      ///< 专用于TLS
    DPAIO_TYPE_QIC,      ///< 专用于QUIC
} dpaio_type_e;

/** @brief `dpele_new` 类型描述：种类、poll 掩码、生命周期钩子。 */
typedef struct
{
    const char* name;
    dpele_type_e type;
    uint32_t size;
    dpaio_type_e iotype;
    uint32_t events; ///< 默认 epoll 事件掩码

    dpret_t (*init)(void* udata, va_list vlist);
    dpret_t (*copy)(void* dst, const void* src);
    void (*fini)(void* udata);
} dpele_type_t;

typedef const dpele_type_t* (*dpele_type_f)();

/** @name 元素生命周期 */
/**@{*/

/** @brief 创建元素；可变参数由 `type->init` 定义（见各 `*_type()` 注释）。 */
dpele_t* dpele_new(const dpele_type_t* type, ...);
/** @brief 同 @ref dpele_new ；参数来自 va_list。 */
dpele_t* dpele_newv(const dpele_type_t* type, va_list args);
/** @brief 复制元素；`unuse_` 为 true 时源元素标记为未使用。 */
dpele_t* dpele_dup(dpele_t* self, bool unuse_);
/** @brief 增加引用计数并返回 `self`。 */
dpele_t* dpele_ref(dpele_t* self);
/** @brief 当前引用计数。 */
uint32_t dpele_refc(dpele_t* self);

/** @brief 减引用；为 0 时销毁。 */
void dpele_del(dpele_t* self);

const dpele_type_t* dpele_type(dpele_t* self);
/** @brief 类型绑定辅助数据区指针（`dpele_type_t::size` 字节）。 */
void* dpele_aux_data(dpele_t* self);

/** @brief 设置元素异步操作超时（秒）；sec 小于 0 表示清除。CTC 类型返回 @ref DPE_INVAL 。
 *  @return @ref DPE_OK 或错误码。 */
dpret_t dpele_set_timeout(dpele_t* self, double sec);
/** @brief 异步操作超时（秒）；未设置时返回 -1。 */
double dpele_timeout(dpele_t* self);

/** 最近一次异步操作返回值（字节数或负错误码）。 */
dpret_t dpele_ret(dpele_t* self);
/** @brief 设置最近一次异步操作返回值。 */
void dpele_set_ret(dpele_t* self, dpret_t ret);

/** @brief 不关心异步结果（detach）；`dpevp_add` 异步成功返回 `DPE_OK`，不 pop 交付。
 *  与 refc 归零延迟删除正交；fire-and-forget 推荐 detach 后 `dpele_del`（while
 * doing）。 */
dpret_t dpele_set_detach(dpele_t* self, bool detach);
bool dpele_is_detach(dpele_t* self);

/** @brief 元素是否正在执行异步操作（已提交且未完成）。 */
bool dpele_is_doing(dpele_t* self);

/** @brief 当前 asc scratch 缓冲（只读）；未分配时返回 NULL。 */
void* dpele_asc_data(dpele_t* self);

/** @brief 关联协程/等待者 opaque 值。 */
dpv64_t dpele_cop(dpele_t* self);
/** @brief 登记等待者；元素正在执行且非 detach 时写入 `cop` 并返回 true。 */
bool dpele_wait(dpele_t* self, dpv64_t cop);

/** @} */

/** @name EFD 通用 */
/**@{*/

/** @brief 取 EFD 元素底层 fd；非 EFD 或未初始化时可能为 -1。
 *  @ingroup dpapp_efd */
int dpefd_fd(dpele_t* self);
/** @brief 元素销毁时是否 close 底层 fd（默认多数类型为 true）。
 *  @ingroup dpapp_efd */
void dpefd_set_close(dpefd_t* self, bool cl);

/** @} */

/** @name 事件循环与异步提交 */
/**@{*/

/** @brief CTC 执行中转发至指定 worker；可配合 `dpele_asc_data` 更新参数后
 * `dpevp_end_ctc_` 续传。
 *  @param toid 目标 worker id；负数为 type 索引（规则同 `dpctc_submit`）。 */
dpret_t dpctc_reto(dpctc_t* self, int toid);
/** @brief 设置 CTC 目标 worker；`toid` 解析规则同 `dpctc_reto`。 */
dpret_t dpctc_set_toid(dpctc_t* self, int toid);
int dpctc_fromid(dpctc_t* self);
int dpctc_toid(dpctc_t* self);

typedef struct dpasc dpasc_t;
typedef const dpasc_t* (*dpasc_type_f)();

/** @brief 当前 worker 的线程 id；非 worker 上下文返回 -1。 */
int dpevp_id();
/** 当前 worker 的 type 索引（对应 `-n<t>`）。 */
int dpevp_type();

/** @brief 提交异步操作；`asc` 为 prep（如 `dpskt_recv()`），后续为 prep 参数。 */
dpret_t dpevp_add(dpele_t* ele, const dpasc_t* asc, ...);
/** @brief 同 @ref dpevp_add ；prep 参数来自 va_list。 */
dpret_t dpevp_addv(dpele_t* ele, const dpasc_t* asc, va_list vargs);

/** @brief 强制结束元素并唤醒等待方（`dpele_ret` 设为 `ret`）。 */
dpret_t dpevp_end(dpele_t* ele, dpret_t ret);

/** @brief 等待下一个完成元素（毫秒；`-1` 可阻塞至有事件）。 */
dpele_t* dpevp_pop(int timeout_ms);

/** @brief 从 CTC 执行侧结束任务并投递结果（绑定层 `await` 完成路径）。 */
dpret_t dpevp_end_ctc_(dpctc_t* ctc, dpret_t err);

/** @} */

/** @name 元素工厂类型 */
/**@{*/

/** @brief 包装已有 fd：`dpele_new(dpefd_init_type(), fd)`
 *  @ingroup dpapp_efd */
const dpele_type_t* dpefd_init_type();
/** @brief 空定时器：`dpele_new(dptmr_init_type())`
 *  @ingroup dpapp_evp */
const dpele_type_t* dptmr_init_type();
/** @brief 空 CTC：`dpele_new(dpctc_init_type(), toid, detach)`
 *  @ingroup dpapp_evp */
const dpele_type_t* dpctc_init_type();

/** @} */

/** @brief 每个线程的公共属性
    count： worker 总数
    type_count：线程 type 数
    each_ids：各 type 的 worker id 列表
    each_count：各 type 的 worker 数 */
#define DPEVP_SET_MEMBER                                                            \
    int count;                                                                      \
    int type_count;                                                                 \
    int** each_ids;                                                                 \
    int* each_count;

#ifdef __cplusplus
}
#endif
