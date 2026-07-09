/** @file dpcpp_aco.hh
 *  @ingroup dpcpp_aco
 *  @brief C++20 协程桥接（ucoro + dpapp `dpele_t`）。
 *
 *  `aco_ele_awaitable` 封装 `dpele_wait` / `dpele_ret`；`resume_from_ele` 从
 *  `dpele_cop` 恢复句柄。`aco_callback` 用于 timer/CTC/server 连接入口。 */
#pragma once

#include <coroutine>
#include <memory>
#include <ucoro/awaitable.hpp>

// C API imported from dpapp
extern "C"
{
#include "dpapp/dpbuf.h"
#include "dpapp/dpdef.h"
#include "dpapp/dpevp.h"
#include "dpapp/dpret.h"
}

namespace dpcpp
{

// ucoro awaitable 别名（协程返回类型）
/** @brief ucoro 协程任务模板别名。 */
template <typename T>
using aco_task = ucoro::awaitable<T>;

/** @brief 多数 IO / aexec 包装返回类型。 */
using aco_ret = aco_task<dpret_t>;
/** @brief 无返回值协程任务。 */
using aco_void = aco_task<void>;
/** @brief 返回 `void*` 的协程任务。 */
using aco_vptr = aco_task<void*>;
/** @brief 模块 init 协程返回类型。 */
using aco_v64 = aco_task<dpv64_t>;
/** @brief timer 等返回元素指针的协程任务。 */
using aco_ele = aco_task<dpele_t*>;
/** @brief endpoint 工厂协程返回类型。 */
using aco_efd = aco_task<dpele_t*>;
/** @brief CTC 协程返回类型。 */
using aco_ctc = aco_task<dpele_t*>;
/** @brief 定时器协程返回类型。 */
using aco_tmr = aco_task<dpele_t*>;
/** @brief 异步 syscall 协程返回类型。 */
using aco_asc = aco_task<dpele_t*>;
/** @brief dpbuf 相关协程返回类型。 */
using aco_buf = aco_task<dpbuf_t*>;

/** @brief 启动独立协程（ucoro::coro_start 包装）。 */
template <typename Awaitable>
auto aco_start(Awaitable&& coro)
{
    return coro_start(std::forward<Awaitable>(coro));
}

/** @brief 启动协程并绑定线程局部上下文 `Local`。 */
template <typename Awaitable, typename Local>
auto aco_start(Awaitable&& coro, Local&& local)
{
    return coro_start(std::forward<Awaitable>(coro), std::forward<Local>(local));
}

/** @brief 启动协程，绑定上下文并在完成时调用 `completer`。 */
template <typename Awaitable, typename Local, typename CompleteFunction>
auto aco_start(Awaitable&& coro, Local&& local, CompleteFunction completer)
{
    return coro_start(std::forward<Awaitable>(coro), std::forward<Local>(local),
        completer);
}

/** @brief 当前挂起协程的线程局部槽位（调试 / 工具使用）。 */
class aco_context
{
public:
    /** @brief 当前线程的协程上下文单例。 */
    static aco_context* current()
    {
        return &instance();
    }

    /** @brief 标记上下文已初始化（worker 启动时调用）。 */
    static void init()
    {
        auto& ctx = instance();
        ctx.initialized_ = true;
    }

    /** @brief 清理上下文（worker 退出时调用）。 */
    static void cleanup()
    {
        auto& ctx = instance();
        ctx.current_coro_handle_ = nullptr;
        ctx.initialized_ = false;
    }

    /** @brief 上下文是否已初始化。 */
    bool initialized() const
    {
        return initialized_;
    }
    /** @brief 当前挂起协程的 `coroutine_handle`。 */
    std::coroutine_handle<> current_coro_handle() const
    {
        return current_coro_handle_;
    }
    /** @brief 记录当前挂起协程句柄（调试/工具）。 */
    void set_current_coro_handle(std::coroutine_handle<> h)
    {
        current_coro_handle_ = h;
    }

private:
    static thread_local aco_context instance_;
    static aco_context& instance()
    {
        return instance_;
    }

    bool initialized_ = false;
    std::coroutine_handle<> current_coro_handle_ = nullptr;
};

/** @brief `co_await` 桥接：`dpele_wait` + 元素完成时恢复。 */
class aco_ele_awaitable
{
public:
    /** @brief 构造 awaitable；元素已完成时走快速路径。 */
    explicit aco_ele_awaitable(dpele_t* ele) : ele_(ele)
    {
        // 快速路径：元素已完成（对应 C 层 `dpele_wait` 的快速返回）。
        if (ele_ && !dpele_is_doing(ele_)) {
            // 本地缓存返回值
            completed_ = true;
            result_ = dpele_ret(ele_);
        }
    }

    /** @brief 元素已完成则无需挂起。 */
    bool await_ready() const noexcept
    {
        // 已捕获结果时跳过挂起。
        return completed_;
    }

    /** @brief 注册 `dpele_wait` 并挂起协程。
     *  @return false 表示快速完成，无需挂起。 */
    bool await_suspend(std::coroutine_handle<> h) noexcept
    {
        // 将协程句柄编码到 dpele 中（与 dpcwc_await 约定一致）。
        if (ele_ && !completed_) {
            dpv64_t cop = DPV64_PTR(h.address());
            bool need_wait = dpele_wait(ele_, cop);
            if (!need_wait) {
                // 防御：元素在 await_ready 和 suspend 之间完成。
                completed_ = true;
                result_ = dpele_ret(ele_);
                return false;
            }

            // 记录句柄供调试/工具使用
            aco_context::current()->set_current_coro_handle(h);
            return true;
        }
        return false;
    }

    /** @brief 恢复后返回 `dpele_ret(ele)`。 */
    dpret_t await_resume() noexcept
    {
        aco_context::current()->set_current_coro_handle(nullptr);
        // 与 C 层一致的可观察行为：始终以 dpele_ret(ele) 结束。
        if (completed_) {
            return result_;
        } else {
            return dpele_ret(ele_);
        }
    }

private:
    dpele_t* ele_;
    bool completed_ = false;
    dpret_t result_ = DPE_OK;
};

/** @brief 从 `dpele_cop(ele)` 恢复挂起的协程。 */
inline void resume_from_ele(dpele_t* ele)
{
    dpv64_t cop = dpele_cop(ele);
    if (cop.ptr) {
        std::coroutine_handle<> h = std::coroutine_handle<>::from_address(cop.ptr);
        if (h.address() != nullptr && h) {
            if (!h.done()) {
                h.resume();
            }
        }
    }
}

/** @brief timer/CTC/server 协程回调：`ele` 为 timer 或 peer，`arg`
 * 为创建时传入参数。 */
using aco_callback = aco_ret (*)(dpele_t*, dpv64_t);

} // namespace dpcpp
