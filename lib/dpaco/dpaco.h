/** @file dpaco.h
 *  @ingroup dpaco
 *  @brief 供 dpcwc 使用的有栈非对称协程。
 *
 *  C 语言有栈非对称协程，支持延迟析构与 wrap 辅助函数；用法与 Lua 协程类似。 */
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "dpapp/dpdef.h"

/** @brief 不透明有栈协程控制块。 */
typedef struct dpaco dpaco_t;

/** @brief High-level scheduler state for a coroutine. */
typedef enum
{
    DPACO_CREATED, ///< 已分配，尚未进入
    DPACO_RUNNING, ///< 正在执行
    DPACO_SUSPEND, ///< yield 挂起等待恢复
    DPACO_DEAD,    ///< 已完成；可能等待析构
} dpaco_status_e;

/** @brief 入口函数：接收 resume 参数，返回 yield 值。 */
typedef dpv64_t (*dpaco_fun_t)(dpv64_t);

/** @brief 每个线程的协程运行时初始化。
 *  @param stack_size dpaco_create() 传入非正数时的默认栈大小（字节）。 */
bool dpaco_thinit(int stack_size);
void dpaco_thfree();

/** @brief 创建新协程；首次 `dpaco_resume` 即进入 `fun`。
 *  @param stack_size 栈大小（字节）；`<=0` 使用 `dpaco_thinit` 默认值。
 *  @return 句柄，失败返回 NULL。 */
dpaco_t* dpaco_create(dpaco_fun_t fun, int stack_size);
/** @brief 将控制转移到协程 `co`，参数为 `val`；返回 `co` yield 的值。 */
dpv64_t dpaco_resume(dpaco_t* co, dpv64_t val);
/** @brief 从当前运行协程 yield 回恢复者，传递值 `val`。 */
dpv64_t dpaco_yield(dpv64_t val);
dpaco_t* dpaco_running();
dpaco_status_e dpaco_status(dpaco_t* co);

/** @brief 延迟协程析构：CREATED/DEAD 状态立即释放；否则等协程结束后再释放。 */
void dpaco_delater(dpaco_t* co);

/** @brief 在临时协程中运行一次 `fun`（创建 + resume + 延迟析构）。 */
dpv64_t dpaco_wrap(dpaco_fun_t fun, dpv64_t val, int stack_size);

#ifdef __cplusplus
}
#endif
