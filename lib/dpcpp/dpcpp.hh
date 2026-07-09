/** @file dpcpp.hh
 *  @ingroup dpcpp
 *  @brief C++20 协程运行时，语义对齐 dpcwc。
 *
 *  异步 IO 经 `dpcpp::aexec` + `co_await await(ele)`（见 `dpcpp_asc.hh`）。
 *  Snowflake / server / endpoint 见 `dpcpp_aux.hh`；`dpele_t` RAII 见
 * `dpcpp_ele.hh`。 */
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "dpcpp/dpcpp_aco.hh"
#include "dpcpp/dpcpp_aux.hh"
#include "dpcpp/dpcpp_ele.hh"

#include "dpapp/dpapp.h"
#include "dpapp/dprbt.h"
#include "dpapp/which.h"

namespace dpcpp
{
/** @brief `dpret_t` 别名。 */
using ret = dpret_t;
/** @brief `dpv32_t` 别名。 */
using v32 = dpv32_t;
/** @brief `dpv64_t` 别名。 */
using v64 = dpv64_t;

/** @brief 每种线程类型：init / exit / step（对应 `dpapp_hdr_t`）。 */
struct app_hdr
{
    using init_func = std::function<aco_v64(dpv64_t, dpv64_t)>;
    using exit_func = std::function<void(dpv64_t)>;
    using step_func = std::function<dpret_t(dpv64_t)>;

    init_func init = nullptr; ///< 该类型 worker 启动时调用一次（协程）
    exit_func exit = nullptr; ///< 线程退出时调用
    step_func step = nullptr; ///< 周期 tick；返回值控制下次休眠

    dpv64_t init_arg1 = DPV64_NULL; ///< 传给 `init` 的第一参数
    dpv64_t init_arg2 = DPV64_NULL; ///< 传给 `init` 的第二参数

    /** @brief 默认构造，各回调为空。 */
    app_hdr() = default;
    /** @brief 指定 init/exit/step 及 init 入参。 */
    app_hdr(init_func init, exit_func exit = nullptr, step_func step = nullptr,
        dpv64_t init_arg1 = DPV64_NULL, dpv64_t init_arg2 = DPV64_NULL);
    /** @brief 空配置占位（全 nullptr）。 */
    app_hdr(std::nullptr_t);
};

/** @brief 加载 C++ 模块（`dpcpp__<name>`）并启动 dpapp。 */
dpret_t start(dpapp_arg_t* args);

/** @brief 当前 worker 线程用户数据。 */
dpv64_t usrdata();

/** @brief 限制 step 内部最长阻塞毫秒数。 */
void set_step_timeout(int ms);

/** @brief `co_await` 挂起直到 `ele` 完成；返回 `dpele_ret(ele)`。 */
aco_ret await(dpele_t* ele);

/** @brief 协程 sleep（秒）；完成返回 `DPE_TIME`。 */
aco_ret sleep(double sec);

/** @brief 一次性 timer；到期在协程中调用 `func(ele, arg)`。
 *  @return timer 元素指针；调用方负责 `dpele_del` 或 `cease`。 */
aco_ele timer(double sec, aco_callback func, dpv64_t arg);

/** @brief 中止 `ele` 并以 `ret` 恢复等待协程。 */
dpret_t cease(dpele_t* ele, int ret);

/** @brief 提交 `dpevp_add(ele, prep, …)`；`DPE_WAIT` 时 `co_await await(ele)`。 */
template <typename... Args>
aco_ret aexec(dpele_t* ele, const dpasc_t* prep, Args&&... args)
{
    dpret_t ret = dpevp_add(ele, prep, std::forward<Args>(args)...);
    if (ret == DPE_WAIT) {
        co_return co_await await(ele);
    }
    co_return ret;
}

} // namespace dpcpp
