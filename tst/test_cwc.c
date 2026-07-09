#include "dpapp/dpapp.h"
#include "dpapp/dplog.h"
#include "dpcwc/dpcwc.h"
#include "dpcwc/dpcwc_aux.h"
#include <stdatomic.h>
#include <stdlib.h>

#define CTC_ONCE_SELF_RET   101
#define CTC_ONCE_WORKER_RET 102

typedef struct
{
    atomic_int once_self;
    atomic_int each_type0;
    atomic_int detach_self;
    atomic_int once_worker;
    atomic_int each_worker;
    atomic_int detach_worker;
    atomic_int detach_reply;
    atomic_int failed;
} ctc_test_ctx_t;

static ctc_test_ctx_t* g_ctx = NULL;

static void _ctc_test_fail(const char* name, dpret_t got, dpret_t expect)
{
    atomic_fetch_add(&g_ctx->failed, 1);
    dplog_error("test", "%s: expect %d, got %d", name, expect, got);
}

static void _ctc_test_ok(const char* name)
{
    dplog_notice("test", "%s: ok", name);
}

static dpret_t _ctc_once_self(dpele_t* task, dpv64_t arg)
{
    (void)task;
    ctc_test_ctx_t* ctx = (ctc_test_ctx_t*)arg.ptr;
    atomic_fetch_add(&ctx->once_self, 1);
    dplog_notice("test", "ctc_once self (id=%d)", dpevp_id());
    return CTC_ONCE_SELF_RET;
}

static dpret_t _ctc_each_type0(dpele_t* task, dpv64_t arg)
{
    (void)task;
    ctc_test_ctx_t* ctx = (ctc_test_ctx_t*)arg.ptr;
    atomic_fetch_add(&ctx->each_type0, 1);
    dplog_notice("test", "ctc_each type0 (id=%d)", dpevp_id());
    return DPE_OK;
}

static dpret_t _ctc_detach_self(dpele_t* task, dpv64_t arg)
{
    (void)task;
    ctc_test_ctx_t* ctx = (ctc_test_ctx_t*)arg.ptr;
    atomic_fetch_add(&ctx->detach_self, 1);
    dplog_notice("test", "ctc_detach self (id=%d)", dpevp_id());
    return DPE_OK;
}

static dpret_t _ctc_each_worker(dpele_t* task, dpv64_t arg)
{
    (void)task;
    ctc_test_ctx_t* ctx = (ctc_test_ctx_t*)arg.ptr;
    atomic_fetch_add(&ctx->each_worker, 1);
    dplog_notice("test", "ctc_each worker (id=%d)", dpevp_id());
    return DPE_OK;
}

static dpret_t _ctc_once_worker(dpele_t* task, dpv64_t arg)
{
    (void)task;
    ctc_test_ctx_t* ctx = (ctc_test_ctx_t*)arg.ptr;
    atomic_fetch_add(&ctx->once_worker, 1);
    dplog_notice("test", "ctc_once worker (id=%d)", dpevp_id());
    return CTC_ONCE_WORKER_RET;
}

static dpret_t _ctc_detach_reply(dpele_t* task, dpv64_t arg)
{
    (void)task;
    ctc_test_ctx_t* ctx = (ctc_test_ctx_t*)arg.ptr;
    atomic_fetch_add(&ctx->detach_reply, 1);
    dplog_notice("test", "ctc_detach reply on main (id=%d)", dpevp_id());
    return DPE_OK;
}

static dpret_t _ctc_detach_worker(dpele_t* task, dpv64_t arg)
{
    (void)task;
    ctc_test_ctx_t* ctx = (ctc_test_ctx_t*)arg.ptr;
    atomic_fetch_add(&ctx->detach_worker, 1);
    dplog_notice("test", "ctc_detach worker (id=%d)", dpevp_id());
    dpcwc_ctc_detach(0, _ctc_detach_reply, DPV64_PTR(ctx));
    return DPE_OK;
}

static void _ctc_test_local(ctc_test_ctx_t* ctx)
{
    dpret_t ret;

    ret = dpcwc_ctc_once(0, _ctc_once_self, DPV64_PTR(ctx));
    if (ret != CTC_ONCE_SELF_RET) {
        _ctc_test_fail("ctc_once self", ret, CTC_ONCE_SELF_RET);
    } else if (atomic_load(&ctx->once_self) != 1) {
        _ctc_test_fail("ctc_once self count", atomic_load(&ctx->once_self), 1);
    } else {
        _ctc_test_ok("ctc_once self");
    }

    ret = dpcwc_ctc_each(0, _ctc_each_type0, DPV64_PTR(ctx));
    if (!dpret_isok(ret)) {
        _ctc_test_fail("ctc_each type0", ret, DPE_OK);
    } else if (atomic_load(&ctx->each_type0) != 1) {
        _ctc_test_fail("ctc_each type0 count", atomic_load(&ctx->each_type0), 1);
    } else {
        _ctc_test_ok("ctc_each type0");
    }

    ret = dpcwc_ctc_detach(0, _ctc_detach_self, DPV64_PTR(ctx));
    if (!dpret_isok(ret)) {
        _ctc_test_fail("ctc_detach self dispatch", ret, DPE_OK);
    }
    dpcwc_sleep(0.05, DPV64_NULL);
    if (atomic_load(&ctx->detach_self) != 1) {
        _ctc_test_fail("ctc_detach self count", atomic_load(&ctx->detach_self), 1);
    } else {
        _ctc_test_ok("ctc_detach self");
    }
}

static void _ctc_test_cross(ctc_test_ctx_t* ctx, const dpapp_info_t* info)
{
    int wc = info->each_count[1];
    int* wids = info->each_ids[1];
    if (wc <= 0 || wids == NULL) {
        dplog_notice("test", "skip cross-thread CTC (no type1 worker)");
        return;
    }

    dpret_t ret;

    ret = dpcwc_ctc_each(1, _ctc_each_worker, DPV64_PTR(ctx));
    if (!dpret_isok(ret)) {
        _ctc_test_fail("ctc_each type1", ret, DPE_OK);
    } else if (atomic_load(&ctx->each_worker) != wc) {
        _ctc_test_fail("ctc_each type1 count", atomic_load(&ctx->each_worker), wc);
    } else {
        _ctc_test_ok("ctc_each type1");
    }

    ret = dpcwc_ctc_once(wids[0], _ctc_once_worker, DPV64_PTR(ctx));
    if (ret != CTC_ONCE_WORKER_RET) {
        _ctc_test_fail("ctc_once worker", ret, CTC_ONCE_WORKER_RET);
    } else if (atomic_load(&ctx->once_worker) != 1) {
        _ctc_test_fail("ctc_once worker count", atomic_load(&ctx->once_worker), 1);
    } else {
        _ctc_test_ok("ctc_once worker");
    }

    ret = dpcwc_ctc_detach(wids[0], _ctc_detach_worker, DPV64_PTR(ctx));
    if (!dpret_isok(ret)) {
        _ctc_test_fail("ctc_detach worker dispatch", ret, DPE_OK);
    }
    dpcwc_sleep(0.1, DPV64_NULL);
    if (atomic_load(&ctx->detach_worker) != 1) {
        _ctc_test_fail("ctc_detach worker count", atomic_load(&ctx->detach_worker),
            1);
    } else if (atomic_load(&ctx->detach_reply) != 1) {
        _ctc_test_fail("ctc_detach worker reply", atomic_load(&ctx->detach_reply),
            1);
    } else {
        _ctc_test_ok("ctc_detach worker");
    }
}

static dpv64_t init00(dpv64_t arg1, dpv64_t arg2)
{
    (void)arg1;
    (void)arg2;

    ctc_test_ctx_t* ctx = calloc(1, sizeof(ctc_test_ctx_t));
    if (ctx == NULL) {
        dplog_alert("test", "alloc test ctx failed");
        return DPV64_NULL;
    }
    g_ctx = ctx;

    dplog_notice("test", "=== CTC test begin (main type=%d id=%d) ===", dpevp_type(),
        dpevp_id());

    _ctc_test_local(ctx);
    _ctc_test_cross(ctx, dpapp_info());

    int failed = atomic_load(&ctx->failed);
    if (failed == 0) {
        dplog_notice("test", "=== CTC test ALL PASSED ===");
    } else {
        dplog_error("test", "=== CTC test FAILED: %d ===", failed);
    }

    return DPV64_PTR(ctx);
}

static dpv64_t init01(dpv64_t arg1, dpv64_t arg2)
{
    (void)arg1;
    (void)arg2;
    dplog_notice("test", "worker ready (type=%d id=%d)", dpevp_type(), dpevp_id());
    return DPV64_NULL;
}

extern dpret_t dpcwc__test_cwc(int argc, char** argv, dpapp_hdr_t* hdrs)
{
    (void)argc;
    (void)argv;

    hdrs[0].init = init00;
    hdrs[1].init = init01;
    return 2;
}
