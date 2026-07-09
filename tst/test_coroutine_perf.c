#include "dpaco/dpaco.h"
#include "dpaco/dpaco_arch.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

// 性能测试协程函数
dpv64_t perf_coroutine(dpv64_t arg)
{
    int count = (int)arg.s64;
    // 第一次 yield 返回参数值
    dpv64_t yield_val = dpaco_yield(arg);

    for (int i = 0; i < count; i++) {
        // 做些少量工作以模拟真实场景
        volatile int dummy = (int)yield_val.s64;
        dummy = dummy * 2 + 1;

        // yield 当前迭代值
        yield_val = dpaco_yield((dpv64_t){.s64 = i});
    }
    return (dpv64_t){.s64 = count};
}

// 获取当前微秒时间
long long get_time_us()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

// 测试单协程切换性能
void test_single_coroutine_perf(int iterations)
{
    printf("Testing single coroutine switching performance (%d iterations)...\n",
        iterations);

    long long start_time = get_time_us();

    // 初始化协程环境
    if (!dpaco_thinit(0)) {
        printf("Failed to initialize coroutine environment\n");
        return;
    }

    // 创建协程
    dpaco_t* co = dpaco_create(perf_coroutine, 8192);
    if (!co) {
        printf("Failed to create coroutine\n");
        dpaco_thfree();
        return;
    }

    // 首次恢复：以迭代次数启动协程
    dpv64_t result = dpaco_resume(co, (dpv64_t){.s64 = iterations});
    if ((int)result.s64 != iterations) {
        printf("Error: first resume expected %d, got %ld\n", iterations,
            (long)result.s64);
        dpaco_delater(co);
        dpaco_thfree();
        return;
    }

    // 多次恢复协程以获取迭代值
    for (int i = 0; i < iterations; i++) {
        result = dpaco_resume(co, (dpv64_t){.s64 = 0}); // dummy value, not used
        if ((int)result.s64 != i) {
            printf("Error: iteration %d expected %d, got %ld\n", i, i,
                (long)result.s64);
            break;
        }
    }

    long long end_time = get_time_us();
    long long total_time = end_time - start_time;

    // 清理
    dpaco_delater(co);
    dpaco_thfree();

    printf("Single coroutine test completed:\n");
    printf("  Total time: %lld us\n", total_time);
    printf("  Total switches: %d\n", iterations);
    printf("  Average time per switch: %.2f us\n", (double)total_time / iterations);
    printf("  Switches per second: %.0f\n",
        (double)iterations * 1000000 / total_time);
}

// 测试多协程切换性能
void test_multiple_coroutines_perf(int num_coroutines, int switches_per_coroutine)
{
    printf("Testing multiple coroutines switching performance (%d coroutines, %d "
           "switches each)...\n",
        num_coroutines, switches_per_coroutine);

    long long start_time = get_time_us();

    // 初始化协程环境
    if (!dpaco_thinit(0)) {
        printf("Failed to initialize coroutine environment\n");
        return;
    }

    // 创建多个协程
    dpaco_t** coroutines = malloc(num_coroutines * sizeof(dpaco_t*));
    if (!coroutines) {
        printf("Failed to allocate memory for coroutines\n");
        dpaco_thfree();
        return;
    }

    for (int i = 0; i < num_coroutines; i++) {
        coroutines[i] = dpaco_create(perf_coroutine, 4096);
        if (!coroutines[i]) {
            printf("Failed to create coroutine %d\n", i);
            goto cleanup;
        }
    }

    // 首次恢复：以 switches_per_coroutine 启动每个协程
    for (int i = 0; i < num_coroutines; i++) {
        dpv64_t result = dpaco_resume(coroutines[i],
            (dpv64_t){.s64 = switches_per_coroutine});
        if ((int)result.s64 != switches_per_coroutine) {
            printf("Error: coroutine %d first resume expected %d, got %ld\n", i,
                switches_per_coroutine, (long)result.s64);
            goto cleanup;
        }
    }

    // 在协程之间切换
    for (int round = 0; round < switches_per_coroutine; round++) {
        for (int i = 0; i < num_coroutines; i++) {
            dpv64_t result = dpaco_resume(coroutines[i],
                (dpv64_t){.s64 = 0}); // dummy value, not used
            if ((int)result.s64 != round) {
                printf("Error: coroutine %d round %d expected %d, got %ld\n", i,
                    round, round, (long)result.s64);
                goto cleanup;
            }
        }
    }

    long long end_time = get_time_us();
    long long total_time = end_time - start_time;
    int total_switches = num_coroutines * switches_per_coroutine;

cleanup:
    // 清理
    for (int i = 0; i < num_coroutines; i++) {
        if (coroutines[i]) {
            dpaco_delater(coroutines[i]);
        }
    }
    free(coroutines);
    dpaco_thfree();

    printf("Multiple coroutines test completed:\n");
    printf("  Total time: %lld us\n", total_time);
    printf("  Total switches: %d\n", total_switches);
    printf("  Average time per switch: %.2f us\n",
        (double)total_time / total_switches);
    printf("  Switches per second: %.0f\n",
        (double)total_switches * 1000000 / total_time);
}

// 测试嵌套协程性能
void test_nested_coroutine_perf(int iterations)
{
    printf("Testing nested coroutine performance (%d iterations)...\n", iterations);

    long long start_time = get_time_us();

    // 初始化协程环境
    if (!dpaco_thinit(0)) {
        printf("Failed to initialize coroutine environment\n");
        return;
    }

    // 创建会创建子协程的嵌套协程
    dpaco_t* nested_co = dpaco_create(perf_coroutine, 8192);
    if (!nested_co) {
        printf("Failed to create nested coroutine\n");
        dpaco_thfree();
        return;
    }

    // 首次恢复：以迭代次数启动嵌套协程
    dpv64_t result = dpaco_resume(nested_co, (dpv64_t){.s64 = iterations});
    if ((int)result.s64 != iterations) {
        printf("Error: first resume expected %d, got %ld\n", iterations,
            (long)result.s64);
        dpaco_delater(nested_co);
        dpaco_thfree();
        return;
    }

    // 多次恢复嵌套协程以获取迭代值
    for (int i = 0; i < iterations; i++) {
        result = dpaco_resume(nested_co,
            (dpv64_t){.s64 = 0}); // dummy value, not used
        if ((int)result.s64 != i) {
            printf("Error: iteration %d expected %d, got %ld\n", i, i,
                (long)result.s64);
            break;
        }
    }

    long long end_time = get_time_us();
    long long total_time = end_time - start_time;

    // 清理
    dpaco_delater(nested_co);
    dpaco_thfree();

    printf("Nested coroutine test completed:\n");
    printf("  Total time: %lld us\n", total_time);
    printf("  Total switches: %d\n", iterations);
    printf("  Average time per switch: %.2f us\n", (double)total_time / iterations);
    printf("  Switches per second: %.0f\n",
        (double)iterations * 1000000 / total_time);
}

int main()
{
    printf("=== Coroutine Performance Tests ===\n\n");

    // 测试 1：单协程性能
    test_single_coroutine_perf(1000000);
    printf("\n");

    // 测试 2：多协程性能
    test_multiple_coroutines_perf(10, 100000);
    printf("\n");

    // 测试 3：嵌套协程性能
    test_nested_coroutine_perf(500000);
    printf("\n");

    printf("=== Performance tests completed ===\n");
    return 0;
}
