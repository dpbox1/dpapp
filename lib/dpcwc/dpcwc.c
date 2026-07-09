#include "dpaco/dpaco.h"
#include "dpapp/dpapp.h"
#include <dlfcn.h>
#include <libgen.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dpapp/dpdef.h"
#include "dpapp/dpevp.h"
#include "dpapp/os/dpevp_pri.h"
#include "dpapp/which.h"
#include "dpcwc/dpcwc.h"

typedef struct
{
    int step_toms;
    dpv64_t usrdata;
} dpapp_ctx_t;

static __thread dpapp_ctx_t _gCtx = {0, DPV64_NULL};

static dpv64_t _dpcwc_run_ctc(dpv64_t arg)
{
    dpctc_t* ctc = (dpctc_t*)arg.ptr;
    dpv64_t* data = (dpv64_t*)dpele_asc_data(ctc);
    dpcwc_call_f func = (dpcwc_call_f)data[0].ptr;

    dpret_t err = func(ctc, data[1]);
    dpevp_end_ctc_(ctc, err);
    return DPV64_NULL;
}

static dpv64_t _dpcwc_run_tmr(dpv64_t arg)
{
    dpele_t* tmr = (dpele_t*)arg.ptr;
    dpv64_t* data = (dpv64_t*)dpele_asc_data(tmr);
    dpcwc_call_f func = (dpcwc_call_f)data[0].ptr;

    dpret_t ret = func(tmr, data[1]);
    dpele_set_ret(tmr, ret);
    dpele_del(tmr); // 解引用，可能还有其他引用
    return DPV64_NULL;
}

static dpv64_t _dpcwc_run_app(dpv64_t arg)
{
    void** args = (void**)arg.ptr;
    dpapp_hdr_t* hdr = (dpapp_hdr_t*)args[0];
    dpapp_ctx_t* ctx = (dpapp_ctx_t*)args[1];
    ctx->usrdata = hdr->init(hdr->init_arg1, hdr->init_arg2);
    return DPV64_NULL;
}

static bool _dpcwc_thread_main_step(dpapp_ctx_t* ctx)
{
    dpele_t* ele = dpevp_pop(ctx->step_toms);
    ctx->step_toms = -1; // 一次 pop 后即无效
    if (ele == NULL) {
        return false;
    }
    dpaco_t* co = (dpaco_t*)dpele_cop(ele).ptr;
    dpret_t ret = dpele_ret(ele);

    switch (dpele_type(ele)->type) {
    case DPELE_TYPE_EFD:
    case DPELE_TYPE_USD: {
        if (co && dpaco_status(co) == DPACO_SUSPEND) {
            dpaco_resume(co, (dpv64_t){.a32.s32 = ret});
        }
        break;
    }
    case DPELE_TYPE_TMR: {
        dpv64_t* data = (dpv64_t*)dpele_asc_data(ele);
        if (data && data[0].ptr) {
            dpaco_wrap(_dpcwc_run_tmr, (dpv64_t){.ptr = ele}, 0);
        } else if (co && dpaco_status(co) == DPACO_SUSPEND) {
            dpaco_resume(co, (dpv64_t){.a32.s32 = ret});
        }
        break;
    }
    case DPELE_TYPE_CTC: {
        if (dpele_is_doing(ele) && _dpele_asc_type(ele) != NULL
            && _dpele_asc_type(ele)->post == NULL) {
            dpaco_wrap(_dpcwc_run_ctc, (dpv64_t){.ptr = ele}, 0);
        } else if (co && dpaco_status(co) == DPACO_SUSPEND) {
            dpaco_resume(co, (dpv64_t){.a32.s32 = ret});
        }
        break;
    }
    default: {
        break;
    }
    }
    return true;
}

typedef struct dpcwc_arg
{
    int stack_size;
    const dpapp_hdr_t* hdrs;
} dpcwc_arg_t;

static dpcwc_arg_t _gArg = {64 * 1024, NULL};

void dpcwc_set_default_stack_size(int size)
{
    _gArg.stack_size = size < 1024 ? 1024 : size;
}

static void _dpcwc_thread_main(void* start_arg)
{
    dpapp_ctx_t* ctx = &_gCtx;
    dpcwc_arg_t* arg = (dpcwc_arg_t*)start_arg;
    const dpapp_hdr_t* hdrs = arg->hdrs;
    if (!dpaco_thinit(arg->stack_size)) {
        dplog_alert("dpcwc", "Failed to initialize the coroutine environment");
        return;
    }

    dpret_t ret = dpssl_thrd_init();
    if (ret != DPE_OK && ret != DPE_UNSUPPORT) {
        dplog_alert("dpcwc", "Failed to initialize the ssl environment: %d", ret);
        dpaco_thfree();
        return;
    }

    ret = dpqic_thrd_init();
    if (ret != DPE_OK && ret != DPE_UNSUPPORT) {
        dplog_alert("dpcwc", "Failed to initialize the quic environment: %d", ret);
        dpaco_thfree();
        return;
    }

    dplog_info("dpcwc", "Thread %d (type %d) started", dpevp_id(), dpevp_type());

    const dpapp_hdr_t* hdr = &hdrs[dpevp_type()];
    if (hdr->init) {
        void* fargs[2] = {(void*)hdr, ctx};
        dpaco_wrap(_dpcwc_run_app, (dpv64_t){.ptr = fargs}, 0);
    }

    dpapp_hdr_step_f mstep = hdr->step;
    for (;;) {
        _dpcwc_thread_main_step(ctx);

        if (mstep) {
            ret = mstep(ctx->usrdata);
            if (ret >= 0) {
                ctx->step_toms = ret;
            } else {
                break;
            }
        }
    }

    if (ret < 0) {
        dplog_warn("dpcwc", "Thread %d (type %d) finished abnormally, step=%d",
            dpevp_id(), dpevp_type(), ret);
    } else {
        dplog_info("dpcwc", "Thread %d (type %d) finished, step=%d", dpevp_id(),
            dpevp_type(), ret);
    }

    if (hdr->exit) {
        hdr->exit(ctx->usrdata);
        ctx->usrdata = DPV64_NULL;
    }

    dpqic_thrd_exit();
    dpssl_thrd_exit();
    dpaco_thfree();
}

#define ARGUMENT_THROW(e)                                                           \
    errmsg = e;                                                                     \
    goto FINAL;

dpret_t dpcwc_start(dpapp_arg_t* args)
{
    if (args->argc <= 0 || args->argv == NULL || args->root_dir == NULL) {
        return DPE_INVAL;
    }

    dplog_init(args->log_file, args->log_level, args->log_tsacc);

    int mod_argc = args->argc;
    const char** mod_argv = args->argv;
    {
        int i = 0;
        for (; i < mod_argc; i++) {
            const char* arg = mod_argv[i];
            if (strcmp(arg, "-s") == 0 || strcmp(arg, "--stack_size") == 0) {
                if (++i >= mod_argc || mod_argv[i][0] == '-') {
                    dplog_error("dpcwc", "Error stack size argument");
                    return DPE_INVAL;
                }
                int stack_size = atoi(mod_argv[i]);
                if (stack_size <= 0) {
                    dplog_error("dpcwc", "Invalid stack size");
                    return DPE_INVAL;
                }
                _gArg.stack_size = stack_size * 1024;
            } else {
                break;
            }
        }
        mod_argv += i;
        mod_argc -= i;
    }

    if (mod_argc <= 0) {
        dplog_error("dpcwc", "No module library specified");
        return DPE_INVAL;
    }

    const char* libpath = mod_argv[0];
    size_t libpath_len = strlen(libpath);
    if (libpath_len < 3 || strcmp(libpath + libpath_len - 3, ".so") != 0) {
        dplog_info("dpcwc", "Not a valid cwc module: %s", libpath);
        return DPE_INVAL;
    }

    char _libpath_tmp1[PATH_MAX] = {0};
    snprintf(_libpath_tmp1, PATH_MAX, "%s/app/%s", args->root_dir, libpath);
    const char* libpaths[2] = {libpath, _libpath_tmp1};

    char libpathr[PATH_MAX] = {0};
    for (int i = 0; i < 2; i++) {
        if (is_file(libpaths[i])) {
            if (realpath(libpaths[i], libpathr) == NULL)
                continue;
            break;
        }
    }

    if (libpathr[0] == '\0') {
        dplog_error("dpcwc", "Invalid library path: %s", libpath);
        return DPE_INVAL;
    }

    char libpath2[PATH_MAX] = {0};
    strcpy(libpath2, libpathr);

    char* bname = basename(libpath2);
    char* ename = strstr(bname, ".so");
    if (ename == NULL) {
        dplog_error("dpcwc", "Invalid library path: %s", libpath);
        return DPE_INVAL;
    }
    if (strncmp(bname, "lib", 3) == 0) {
        bname += 3;
    }

    char sym[256];
    int sym_len = snprintf(sym, sizeof(sym), "dpcwc__%.*s", (int)(ename - bname),
        bname);
    if (sym_len >= (int)sizeof(sym)) {
        dplog_error("dpcwc", "Module name too long: %s", libpath);
        return DPE_INVAL;
    }

    typedef dpret_t (*cwc_load_func)(int, char**, dpapp_hdr_t*);
    void* handle = dlopen(libpathr, RTLD_LAZY);
    if (handle == NULL) {
        dplog_error("dpcwc", dlerror());
        return DPE_INVAL;
    }

    cwc_load_func load_func = (cwc_load_func)dlsym(handle, sym);
    if (load_func == NULL) {
        dplog_info("dpcwc", "The library has no valid entry function: %s", libpath);
        dlclose(handle);
        return DPE_INVAL;
    }

    // _gArg.hdrs 引用局部变量：安全，因 dpapp_start 是阻塞调用
    dpapp_hdr_t hdrs[DPAPP_TYPE_MAX];
    memset(hdrs, 0, sizeof(hdrs));
    int count = load_func(mod_argc, (char**)mod_argv, hdrs);
    if (count <= 0 || count > DPAPP_TYPE_MAX) {
        dplog_error("dpcwc",
            "Invalid thread type count from module, may be error code: %d", count);
        dlclose(handle);
        return DPE_INVAL;
    }

    dplog_info("dpcwc", "Loaded module: %s types=%d", libpath, count);

    args->type_count = count;
    _gArg.hdrs = hdrs;

    dpret_t ret = dpapp_start(args, _dpcwc_thread_main, &_gArg);

    dlclose(handle);
    return ret;
}

/// 核心 API
dpv64_t dpcwc_usrdata()
{
    return _gCtx.usrdata;
}

void dpcwc_set_step_timeout(int ms)
{
    _gCtx.step_toms = ms;
}

// clang-format off
#define DPV64_COP    (dpv64_t){.ptr = dpaco_running()}
// clang-format on

dpret_t dpcwc_await(dpele_t* ele, dpv64_t yield_msg_)
{
    dpret_t ret = DPE_OK;
    if (dpele_wait(ele, DPV64_COP)) {
        ret = dpaco_yield(yield_msg_).a32.s32;
    } else {
        ret = dpele_ret(ele);
    }
    return ret;
}

#define dpcwc_exec_await(opt_, ele_, ...)                                           \
    dpret_t ret = dpevp_add_##opt_(ele_, ##__VA_ARGS__);                            \
    if (ret == DPE_WAIT) {                                                          \
        ret = dpcwc_await(ele_, DPV64_NULL);                                        \
    }

#define dpcwc_exec_await_return(opt_, ele_, ...)                                    \
    dpcwc_exec_await(opt_, ele_, ##__VA_ARGS__);                                    \
    return ret;

dpret_t dpcwc_cease(dpele_t* ele, dpret_t ret)
{
    dpaco_t* co = dpele_cop(ele).ptr;
    dpret_t r = dpevp_end(ele, ret);
    if (dpret_isok(r) && co) {
        dpaco_resume(co, (dpv64_t){.a32.s32 = ret});
    }
    return r;
}

dpret_t dpcwc_timer(double sec, dpcwc_call_f func, dpele_t** ptmr_, dpv64_t arg_)
{
    if (func == NULL) {
        return DPE_INVAL;
    }

    dpele_t* timer = dpele_new(dptmr_init_type());
    if (timer == NULL) {
        return errno;
    }
    if (sec < 0) {
        sec = 4294967296.0f;
    }

    dpret_t ret = dpevp_add(timer, dptmr_timeout(), sec, DPV64_PTR(func), arg_);
    if (DPE_WAIT != ret) {
        dpele_del(timer);
        return ret;
    }

    if (ptmr_) {
        *ptmr_ = timer;
        dpele_ref(timer);
    }
    return DPE_OK;
}

dpret_t dpcwc_sleep(double sec, dpv64_t yield_msg_)
{
    dpele_t* timer = dpele_new(dptmr_init_type());
    if (timer == NULL) {
        return errno;
    }

    if (sec < 0) {
        sec = 4294967296.0f;
    }

    dpret_t ret = dpevp_add(timer, dptmr_timeout(), sec, DPV64_NULL, DPV64_NULL);
    if (ret != DPE_WAIT) {
        dpele_del(timer);
        return ret;
    }

    ret = dpcwc_await(timer, yield_msg_);
    dpele_del(timer);
    return ret;
}

dpret_t dpcwc_aexec(dpele_t* ele, const dpasc_t* asc, ...)
{
    va_list vargs;
    va_start(vargs, asc);
    dpret_t ret = dpevp_addv(ele, asc, vargs);
    va_end(vargs);
    if (ret == DPE_WAIT)
        ret = dpcwc_await(ele, DPV64_NULL);
    return ret;
}
