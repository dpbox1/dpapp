/** @file dpcwc.h
 *  @ingroup dpcwc
 *  @brief C 协程运行时（libdpaco + libdpapp）。
 *
 *  线程内以有栈协程运行用户逻辑；异步 IO 经 `dpcwc_aexec`（见 `dpcwc_asc.h`）提交
 *  `dpevp_add`，必要时 `dpcwc_await` 挂起。模块通过 `dpapp_hdr_t` 注册
 * init/exit/step。
 *
 *  扩展能力（Snowflake ID、多 listener server、endpoint 工厂）见 `dpcwc_aux.h`。 */
#pragma once

#include "dpaco/dpaco.h"
#include "dpapp/dpapp.h"
#include "dpapp/dpdef.h"
#include "dpapp/dprbt.h"
#include "dpapp/which.h"

#ifdef __cplusplus
extern "C"
{
#endif

// clang-format off
/** @brief CWC 模块额外命令行帮助（`--help` 时追加到 dpapp 帮助后）。 */
#define DPCWC_ARG_HELP                                                 \
"CWC external option:\n"                                               \
"-s [ --stack_size ] arg (64K)            Coroutine stack size(KB)\n"
// clang-format on

/** @brief 模块 init 协程入口；参数来自 `dpapp_hdr_t::init_arg1/2`。 */
typedef dpv64_t (*dpapp_hdr_init_f)(dpv64_t, dpv64_t);
/** @brief 模块 exit 回调；参数为 init 返回值。 */
typedef void (*dpapp_hdr_exit_f)(dpv64_t);
/** @brief step 返回值：<0 停止事件循环，否则为下次 step 前最长休眠毫秒数。 */
typedef dpret_t (*dpapp_hdr_step_f)(dpv64_t);

/** @brief 每种线程类型一行：生命周期 + 协作式 step。 */
typedef struct
{
    dpapp_hdr_init_f init; ///< 该类型 worker 启动时调用一次（在协程中）
    dpapp_hdr_exit_f exit; ///< 线程退出时调用
    dpapp_hdr_step_f step; ///< 周期 tick；返回值控制下次休眠
    dpv64_t init_arg1;     ///< 传给 `init` 的第一参数
    dpv64_t init_arg2;     ///< 传给 `init` 的第二参数
} dpapp_hdr_t;

/** @brief 设置默认协程栈大小（KB）；`dpcwc_start` 前生效。 */
void dpcwc_set_default_stack_size(int size);

/** @brief 加载 CWC 模块（`dpcwc__<name>`）并启动 dpapp。
 *  @param args `argv[0]` 为 `.so` 路径，其余传给模块入口。
 *  @return 0 成功，否则为负 `dpret_t`。 */
dpret_t dpcwc_start(dpapp_arg_t* args);

/** @brief 当前 worker 线程用户数据。 */
dpv64_t dpcwc_usrdata();

/** @brief 限制协作式 step 在运行时内部的最长阻塞毫秒数（嵌入 GUI 等场景）。 */
void dpcwc_set_step_timeout(int ms);

/** @brief CTC / timer 回调：`ele` 为 CTC 或 timer 元素，`arg` 为创建时传入的
 * `dpv64_t`。 */
typedef dpret_t (*dpcwc_call_f)(dpele_t* ele, dpv64_t arg);

/** @brief 挂起当前协程，直到 `ele` 完成。
 *  @param yield_msg_ 传给 `dpaco_yield` 的恢复消息；通常传 `DPV64_NULL`。
 *  @return `dpele_ret(ele)` 或 yield 侧写入的 `dpret_t`。 */
dpret_t dpcwc_await(dpele_t* ele, dpv64_t yield_msg_);

/** @brief 中止 `ele` 上未完成的操作，并以 `ret` 唤醒等待协程。
 *  @return `dpevp_end` 的返回值。 */
dpret_t dpcwc_cease(dpele_t* ele, dpret_t ret);

/** @brief 延迟 `sec` 秒后调用 `func(ele, arg_)`；回调数据经 `dptmr_timeout` asc
 * scratch 传递。
 *  @param ptmr_ 非 NULL 时输出 timer 引用（调用方 `dpele_del` 或 `dpcwc_cease`）。
 *  @param arg_  原样传给 `func` 第二参数。 */
dpret_t dpcwc_timer(double sec, dpcwc_call_f func, dpele_t** ptmr_, dpv64_t arg_);

/** @brief 协程 sleep；超时完成返回 `DPE_TIME`。
 *  @param yield_msg_ 传给 `dpaco_yield` 的恢复消息。 */
dpret_t dpcwc_sleep(double sec, dpv64_t yield_msg_);

/** @brief 提交异步操作：`dpevp_addv(ele, asc, …)`，返回 `DPE_WAIT` 时内部
 * `dpcwc_await`。
 *  @param asc `dpasc.h` 中 `dpfoo()` 返回的 prep 描述符。
 *  @see dpcwc_asc.h 内联包装。 */
dpret_t dpcwc_aexec(dpele_t* ele, const dpasc_t* asc, ...);

#ifdef __cplusplus
}
#endif
