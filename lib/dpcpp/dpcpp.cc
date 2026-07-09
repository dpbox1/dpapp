#include "dpcpp/dpcpp.hh"
#include "dpapp/dpssl.h"
#include "dpapp/os/dpevp_pri.h"
#include "dpcpp/dpcpp_aco.hh"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std::filesystem;

namespace dpcpp
{

app_hdr::app_hdr(init_func init, exit_func exit, step_func step, dpv64_t init_arg1,
    dpv64_t init_arg2)
    : init(init), exit(exit), step(step), init_arg1(init_arg1), init_arg2(init_arg2)
{}

app_hdr::app_hdr(std::nullptr_t)
{}

// 应用上下文（线程局部）
struct app_ctx
{
    int step_timeout_ms = -1;
    dpv64_t usrdata = DPV64_NULL;
};

thread_local app_ctx tCtx;

// 在协程中运行定时器回调的辅助函数
aco_void _run_tmr(dpele_t* tmr)
{
    dpv64_t* data = (dpv64_t*)dpele_asc_data(tmr);
    aco_callback fun = (aco_callback)data[0].ptr;
    if (fun) {
        dpret_t ret = co_await fun(tmr, data[1]);
        dpele_set_ret(tmr, ret);
    }
    dpele_del(tmr);
    co_return;
}

// 在协程中运行 CTC 回调的辅助函数
aco_void _run_ctc(dpele_t* ctc)
{
    dpv64_t* data = (dpv64_t*)dpele_asc_data(ctc);
    aco_callback fun = (aco_callback)data[0].ptr;
    if (fun) {
        dpret_t err = co_await fun(ctc, data[1]);
        dpevp_end_ctc_(ctc, err);
    }
    co_return;
}

// 从事件循环处理一个事件
bool _thread_main_step(app_ctx* ctx)
{
    dpele_t* ele = dpevp_pop(ctx->step_timeout_ms);
    ctx->step_timeout_ms = -1; // 一次 pop 后即无效

    if (!ele) {
        return false;
    }

    // 从 dpele 获取协程句柄并恢复
    switch (dpele_type(ele)->type) {
    case DPELE_TYPE_EFD:
    case DPELE_TYPE_USD: {
        // 异步 syscall / fd 元素或手动控制 —— 恢复等待协程
        resume_from_ele(ele);
        break;
    }
    case DPELE_TYPE_TMR: {
        dpv64_t* data = (dpv64_t*)dpele_asc_data(ele);
        if (data && data[0].ptr) {
            aco_start(_run_tmr(ele));
        } else {
            resume_from_ele(ele);
        }
        break;
    }
    case DPELE_TYPE_CTC: {
        if (dpele_is_doing(ele) && _dpele_asc_type(ele) != NULL
            && _dpele_asc_type(ele)->post == NULL) {
            aco_start(_run_ctc(ele));
        } else {
            resume_from_ele(ele);
        }
        break;
    }
    default:
        break;
    }
    return true;
}

// 线程主函数
void thread_main(void* start_arg)
{
    app_ctx* ctx = &tCtx;
    const app_hdr* hdrs = (const app_hdr*)start_arg;
    // 初始化协程上下文
    aco_context::init();

    dpret_t ret = dpssl_thrd_init();
    if (ret != DPE_OK && ret != DPE_UNSUPPORT) {
        dplog_alert("dpcpp", "Failed to initialize the ssl environment: %d", ret);
        aco_context::cleanup();
        return;
    }

    // 初始化 QUIC
    ret = dpqic_thrd_init();
    if (ret != DPE_OK && ret != DPE_UNSUPPORT) {
        dplog_alert("dpcpp", "Failed to initialize the quic environment: %d", ret);
        aco_context::cleanup();
        return;
    }

    dplog_info("dpcpp", "Thread %d (type %d) started", dpevp_id(), dpevp_type());

    // 获取线程类型
    int thread_type = dpevp_type();
    const app_hdr& hdr = hdrs[thread_type];

    // 在协程中初始化应用
    // 注意：aco_start 将协程同步运行到其首次挂起点（或结束），因此此处栈引用 ctx/hdr
    // 保持有效。
    if (hdr.init) {
        aco_start([](app_ctx* ctx_, const app_hdr& hdr_) -> aco_void {
            ctx_->usrdata = co_await hdr_.init(hdr_.init_arg1, hdr_.init_arg2);
            co_return;
        }(ctx, hdr));
    }

    // 主事件循环
    app_hdr::step_func step_func = hdr.step;
    for (;;) {
        // 处理一个事件
        _thread_main_step(ctx);

        // 调用 step 函数
        if (step_func) {
            ret = step_func(ctx->usrdata);
            if (ret >= 0) {
                ctx->step_timeout_ms = ret;
            } else {
                break;
            }
        }
    }

    if (ret < 0) {
        dplog_warn("dpcpp", "Thread %d (type %d) finished abnormally, step=%d",
            dpevp_id(), dpevp_type(), ret);
    } else {
        dplog_info("dpcpp", "Thread %d (type %d) finished, step=%d", dpevp_id(),
            dpevp_type(), ret);
    }

    // 清理 —— exit 为协程，以 detached 任务启动
    if (hdr.exit) {
        hdr.exit(ctx->usrdata);
        ctx->usrdata = DPV64_NULL;
    }

    dpqic_thrd_exit();
    dpssl_thrd_exit();
    aco_context::cleanup();
}

dpret_t start(dpapp_arg_t* args)
{
    if (args->argc <= 0 || args->argv == nullptr) {
        return DPE_INVAL;
    }

    dplog_init(args->log_file, args->log_level, args->log_tsacc);

    std::string libpath = args->argv[0];
    if (libpath.empty()) {
        dplog_error("dpcpp", "Invalid library path: null");
        return DPE_INVAL;
    }

    if (libpath.size() < 3 || libpath.compare(libpath.size() - 3, 3, ".so") != 0) {
        dplog_info("dpcpp", "Not a valid cpp module: %s", libpath.c_str());
        return DPE_INVAL;
    }

    path libpath1 = libpath;
    path libpath2 = path(args->root_dir) / "app" / libpath;

    const char* found_path = nullptr;
    if (is_regular_file(libpath1)) {
        found_path = libpath1.c_str();
    } else if (is_regular_file(libpath2)) {
        found_path = libpath2.c_str();
    }

    if (!found_path) {
        dplog_error("dpcpp", "Invalid library path: %s", libpath.c_str());
        return DPE_INVAL;
    }

    char* real_path = realpath(found_path, nullptr);
    if (!real_path) {
        dplog_error("dpcpp", "Failed to resolve library path: %s", found_path);
        return DPE_INVAL;
    }

    void* handle = dlopen(real_path, RTLD_LAZY);
    free(real_path);

    if (!handle) {
        dplog_error("dpcpp", "%s", dlerror());
        return DPE_INVAL;
    }

    std::string libname = found_path;
    size_t last_slash = libname.find_last_of('/');
    if (last_slash != std::string::npos) {
        libname = libname.substr(last_slash + 1);
    }
    if (libname.size() >= 3 && libname.substr(0, 3) == "lib") {
        libname = libname.substr(3);
    }
    size_t dot_pos = libname.find(".so");
    if (dot_pos != std::string::npos) {
        libname = libname.substr(0, dot_pos);
    }

    using cpp_load_func = dpret_t (*)(int, const char**, app_hdr*);
    char sym[256];
    std::snprintf(sym, sizeof(sym), "dpcpp__%s", libname.c_str());
    cpp_load_func load_func = (cpp_load_func)dlsym(handle, sym);
    if (load_func == nullptr) {
        dlclose(handle);
        dplog_info("dpcpp", "Module entry not found in %s", libpath.c_str());
        return DPE_INVAL;
    }

    app_hdr hdrs[DPAPP_TYPE_MAX];
    int count = load_func(args->argc, args->argv, hdrs);
    if (count <= 0 || count > DPAPP_TYPE_MAX) {
        dlclose(handle);
        dplog_error("dpcpp", "Invalid thread type count from module %s: %d",
            libpath.c_str(), count);
        return DPE_INVAL;
    }

    dplog_info("dpcpp", "Loaded module: %s types=%d", libpath.c_str(), count);

    args->type_count = count;
    dpret_t ret = dpapp_start(args, thread_main, (void*)hdrs);

    dlclose(handle);
    return ret;
}

// 核心 API 实现
dpv64_t usrdata()
{
    return tCtx.usrdata;
}

void set_step_timeout(int ms)
{
    tCtx.step_timeout_ms = ms;
}

aco_ret await(dpele_t* ele)
{
    co_return co_await aco_ele_awaitable{ele};
}

aco_ret sleep(double sec)
{
    if (sec < 0) {
        sec = 4294967296.0;
    }

    dpele_t* timer = dpele_new(dptmr_init_type());
    if (!timer) {
        co_return errno;
    }

    dpret_t ret = dpevp_add(timer, dptmr_timeout(), sec, DPV64_NULL, DPV64_NULL);
    if (ret != DPE_WAIT) {
        dpele_del(timer);
        co_return ret;
    }

    ret = co_await await(timer);
    dpele_del(timer);
    co_return ret;
}

aco_ele timer(double sec, aco_callback func, dpv64_t arg)
{
    if (!func) {
        co_return nullptr;
    }

    if (sec < 0) {
        sec = 4294967296.0;
    }

    dpele_t* timer = dpele_new(dptmr_init_type());
    if (!timer) {
        co_return nullptr;
    }

    dpret_t ret = dpevp_add(timer, dptmr_timeout(), sec, DPV64_PTR(func), arg);
    if (ret != DPE_WAIT) {
        dpele_del(timer);
        co_return nullptr;
    }

    co_return timer;
}

dpret_t cease(dpele_t* ele, int ret)
{
    dpret_t r = dpevp_end(ele, ret);
    if (dpret_isok(r)) {
        resume_from_ele(ele);
    }
    return r;
}

// template <typename... Args>
// aco_ret aexec(dpefd_t* efd, dpasc_type_f prep, Args&&... args)
// {
//     dpret_t ret = dpevp_add_ioc(efd, prep, std::forward<Args>(args)...);
//     if (ret == DPE_WAIT) {
//         co_return co_await await(efd);
//     }
//     co_return ret;
// }

// template <typename... Args>
// aco_ret aexec(dpele_t* ele, dpasc_type_f prep, Args&&... args)
// {
//     dpret_t ret = dpevp_add(ele, prep, std::forward<Args>(args)...);
//     if (ret == DPE_WAIT) {
//         co_return co_await await(ele);
//     }
//     co_return ret;
// }

} // namespace dpcpp
