#include "dpaco/dpaco.h"
#include "dpaco/dpaco_arch.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// 测试协程函数
dpv64_t test_coroutine(dpv64_t arg)
{
    printf("Coroutine started, argument: %ld\n", (long)arg.s64);

    // 让出控制权
    dpv64_t yield_val = dpaco_yield((dpv64_t){.s64 = 100});
    printf("Coroutine resumed, received value: %ld\n", (long)yield_val.s64);

    // 再次让出控制权
    yield_val = dpaco_yield((dpv64_t){.s64 = 200});
    printf("Coroutine resumed again, received value: %ld\n", (long)yield_val.s64);

    return (dpv64_t){.s64 = 300};
}

// 嵌套协程函数
dpv64_t nested_coroutine(dpv64_t arg)
{
    printf("Nested coroutine started, argument: %ld\n", (long)arg.s64);

    // 创建子协程
    dpaco_t* sub_co = dpaco_create(test_coroutine, 4096);
    if (!sub_co) {
        printf("Failed to create sub-coroutine\n");
        return (dpv64_t){.s64 = -1};
    }

    printf("Sub-coroutine created successfully\n");

    // 恢复子协程
    dpv64_t sub_result = dpaco_resume(sub_co, (dpv64_t){.s64 = 50});
    printf("Sub-coroutine first resume, received: %ld\n", (long)sub_result.s64);

    // 再次恢复子协程
    sub_result = dpaco_resume(sub_co, (dpv64_t){.s64 = 60});
    printf("Sub-coroutine second resume, received: %ld\n", (long)sub_result.s64);

    // 最终恢复子协程
    sub_result = dpaco_resume(sub_co, (dpv64_t){.s64 = 70});
    printf("Sub-coroutine final resume, received: %ld\n", (long)sub_result.s64);

    // 清理子协程
    dpaco_delater(sub_co);

    // 从嵌套协程让出
    dpv64_t yield_val = dpaco_yield((dpv64_t){.s64 = 400});
    printf("Nested coroutine resumed, received value: %ld\n", (long)yield_val.s64);

    return (dpv64_t){.s64 = 500};
}

/* ── Test 4: delater (parent dies while child suspended) ────────── */

static dpaco_t* g_parent = NULL;
static dpaco_t* g_child = NULL;

static dpv64_t delater_child_fn(dpv64_t in)
{
    (void)in;
    dpv64_t out = DPV64_RES(111);
    dpaco_yield(out);
    return DPV64_RES(222);
}

static dpv64_t delater_parent_fn(dpv64_t in)
{
    (void)in;
    g_child = dpaco_create(delater_child_fn, 64 * 1024);
    assert(g_child != NULL);
    dpv64_t y1 = dpaco_resume(g_child, DPV64_RES(0));
    assert(y1.a32.s32 == 111);
    return DPV64_RES(1234);
}

int main()
{
    printf("=== Testing basic coroutine functionality ===\n");

    // 初始化协程环境
    if (!dpaco_thinit(0)) {
        printf("Failed to initialize coroutine environment\n");
        return -1;
    }

    printf("Coroutine environment initialized successfully\n");

    // 测试 1：基础协程
    printf("\n--- Test 1: Basic coroutine ---\n");
    dpaco_t* co = dpaco_create(test_coroutine, 8192);
    if (!co) {
        printf("Failed to create coroutine\n");
        dpaco_thfree();
        return -1;
    }

    printf("Coroutine created successfully\n");

    // 恢复协程
    dpv64_t result = dpaco_resume(co, (dpv64_t){.s64 = 10});
    printf("First resume, received value: %ld\n", (long)result.s64);

    // 再次恢复协程
    result = dpaco_resume(co, (dpv64_t){.s64 = 20});
    printf("Second resume, received value: %ld\n", (long)result.s64);

    // 最终恢复
    result = dpaco_resume(co, (dpv64_t){.s64 = 30});
    printf("Third resume, received value: %ld\n", (long)result.s64);

    // 检查协程状态
    dpaco_status_e status = dpaco_status(co);
    printf("Final coroutine status: %d\n", status);

    // 清理
    dpaco_delater(co);

    // 测试 2：嵌套协程
    printf("\n--- Test 2: Nested coroutine ---\n");
    dpaco_t* nested_co = dpaco_create(nested_coroutine, 8192);
    if (!nested_co) {
        printf("Failed to create nested coroutine\n");
        dpaco_thfree();
        return -1;
    }

    printf("Nested coroutine created successfully\n");

    // 恢复嵌套协程
    result = dpaco_resume(nested_co, (dpv64_t){.s64 = 1000});
    printf("Nested coroutine first resume, received value: %ld\n", (long)result.s64);

    // 再次恢复嵌套协程
    result = dpaco_resume(nested_co, (dpv64_t){.s64 = 2000});
    printf("Nested coroutine second resume, received value: %ld\n",
        (long)result.s64);

    // 检查嵌套协程状态
    status = dpaco_status(nested_co);
    printf("Final nested coroutine status: %d\n", status);

    // 清理
    dpaco_delater(nested_co);

    // 测试 3：多个协程
    printf("\n--- Test 3: Multiple coroutines ---\n");
    dpaco_t* co1 = dpaco_create(test_coroutine, 4096);
    dpaco_t* co2 = dpaco_create(test_coroutine, 4096);
    dpaco_t* co3 = dpaco_create(test_coroutine, 4096);

    if (!co1 || !co2 || !co3) {
        printf("Failed to create multiple coroutines\n");
        dpaco_thfree();
        return -1;
    }

    printf("Multiple coroutines created successfully\n");

    // 恢复所有协程各一次
    result = dpaco_resume(co1, (dpv64_t){.s64 = 1});
    printf("Co1 first resume: %ld\n", (long)result.s64);

    result = dpaco_resume(co2, (dpv64_t){.s64 = 2});
    printf("Co2 first resume: %ld\n", (long)result.s64);

    result = dpaco_resume(co3, (dpv64_t){.s64 = 3});
    printf("Co3 first resume: %ld\n", (long)result.s64);

    // 再次恢复所有协程
    result = dpaco_resume(co1, (dpv64_t){.s64 = 11});
    printf("Co1 second resume: %ld\n", (long)result.s64);

    result = dpaco_resume(co2, (dpv64_t){.s64 = 22});
    printf("Co2 second resume: %ld\n", (long)result.s64);

    result = dpaco_resume(co3, (dpv64_t){.s64 = 33});
    printf("Co3 second resume: %ld\n", (long)result.s64);

    // 最终恢复所有协程
    result = dpaco_resume(co1, (dpv64_t){.s64 = 111});
    printf("Co1 final resume: %ld\n", (long)result.s64);

    result = dpaco_resume(co2, (dpv64_t){.s64 = 222});
    printf("Co2 final resume: %ld\n", (long)result.s64);

    result = dpaco_resume(co3, (dpv64_t){.s64 = 333});
    printf("Co3 final resume: %ld\n", (long)result.s64);

    // 清理
    dpaco_delater(co1);
    dpaco_delater(co2);
    dpaco_delater(co3);

    printf("\n--- Test 4: Delater (parent dies while child suspended) ---\n");
    g_parent = dpaco_create(delater_parent_fn, 64 * 1024);
    assert(g_parent != NULL);
    dpv64_t r1 = dpaco_resume(g_parent, DPV64_RES(0));
    assert(r1.a32.s32 == 1234);
    assert(dpaco_status(g_parent) == DPACO_DEAD);
    assert(dpaco_status(g_child) == DPACO_SUSPEND);
    dpaco_delater(g_parent);
    g_parent = NULL;
    dpv64_t r2 = dpaco_resume(g_child, DPV64_RES(0));
    assert(r2.a32.s32 == 222);
    assert(dpaco_status(g_child) == DPACO_DEAD);
    dpaco_delater(g_child);
    g_child = NULL;

    dpaco_thfree();
    printf("test_coroutine ok\n");
    return 0;
}
