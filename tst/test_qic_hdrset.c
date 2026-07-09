#include "dpapp/dpqic.h"
#include "dpapp/dpret.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 测试基础功能
void test_hdrset_basic()
{
    printf("=== Testing hdrset basic functionality ===\n");

    dpqic_hdrset_t* hdrset = dpqic_hdrset_new(2);
    dpret_t ret;
    const char* value;
    int count;

    assert(hdrset != NULL);

    // 测试设置头
    ret = dpqic_hdrset_set(hdrset, "Content-Type", "application/json");
    assert(ret == DPE_OK);

    ret = dpqic_hdrset_set(hdrset, "Content-Length", "1024");
    assert(ret == DPE_OK);

    ret = dpqic_hdrset_set(hdrset, "User-Agent", "dpapp/1.0");
    assert(ret == DPE_OK);

    // 测试获取头值
    value = dpqic_hdrset_get(hdrset, "Content-Type");
    assert(value != NULL);
    assert(strcmp(value, "application/json") == 0);

    value = dpqic_hdrset_get(hdrset, "Content-Length");
    assert(value != NULL);
    assert(strcmp(value, "1024") == 0);

    value = dpqic_hdrset_get(hdrset, "User-Agent");
    assert(value != NULL);
    assert(strcmp(value, "dpapp/1.0") == 0);

    // 测试不存在的头
    value = dpqic_hdrset_get(hdrset, "Non-Existent");
    assert(value == NULL);

    // 测试头计数
    count = dpqic_hdrset_count(hdrset);
    assert(count == 3);

    printf("✓ Basic functionality test passed\n");

    // 清理
    dpqic_hdrset_del(hdrset);
}

// Test index-based access
void test_hdrset_at()
{
    printf("=== Testing hdrset index-based access ===\n");

    dpqic_hdrset_t* hdrset = dpqic_hdrset_new(2);
    dpret_t ret;
    const char *name, *value;
    int count;

    assert(hdrset != NULL);

    // 设置多个头
    ret = dpqic_hdrset_set(hdrset, "Header1", "Value1");
    assert(ret == DPE_OK);

    ret = dpqic_hdrset_set(hdrset, "Header2", "Value2");
    assert(ret == DPE_OK);

    ret = dpqic_hdrset_set(hdrset, "Header3", "Value3");
    assert(ret == DPE_OK);

    count = dpqic_hdrset_count(hdrset);
    assert(count == 3);

    // 测试基于索引的访问
    value = dpqic_hdrset_at(hdrset, 0, &name);
    assert(value != NULL);
    assert(name != NULL);
    printf("Index 0: %s = %s\n", name, value);

    value = dpqic_hdrset_at(hdrset, 1, &name);
    assert(value != NULL);
    assert(name != NULL);
    printf("Index 1: %s = %s\n", name, value);

    value = dpqic_hdrset_at(hdrset, 2, &name);
    assert(value != NULL);
    assert(name != NULL);
    printf("Index 2: %s = %s\n", name, value);

    // 测试越界访问
    value = dpqic_hdrset_at(hdrset, 3, &name);
    assert(value == NULL);

    value = dpqic_hdrset_at(hdrset, -1, &name);
    assert(value == NULL);

    printf("✓ Index-based access test passed\n");

    // 清理
    dpqic_hdrset_del(hdrset);
}

// 测试头更新
void test_hdrset_update()
{
    printf("=== Testing hdrset header updates ===\n");

    dpqic_hdrset_t* hdrset = dpqic_hdrset_new(2);
    dpret_t ret;
    const char* value;
    int count;

    assert(hdrset != NULL);

    // 设置初始头
    ret = dpqic_hdrset_set(hdrset, "Test-Header", "Initial-Value");
    assert(ret == DPE_OK);

    count = dpqic_hdrset_count(hdrset);
    assert(count == 1);

    value = dpqic_hdrset_get(hdrset, "Test-Header");
    assert(value != NULL);
    assert(strcmp(value, "Initial-Value") == 0);

    // 更新相同的头
    ret = dpqic_hdrset_set(hdrset, "Test-Header", "Updated-Value");
    assert(ret == DPE_OK);

    count = dpqic_hdrset_count(hdrset);
    assert(count == 1); // 计数应保持不变

    value = dpqic_hdrset_get(hdrset, "Test-Header");
    assert(value != NULL);
    assert(strcmp(value, "Updated-Value") == 0);

    printf("✓ Header update test passed\n");

    // 清理
    dpqic_hdrset_del(hdrset);
}

// 测试大小写不敏感的头
void test_hdrset_case_insensitive()
{
    printf("=== Testing hdrset case insensitive headers ===\n");

    dpqic_hdrset_t* hdrset = dpqic_hdrset_new(2);
    dpret_t ret;
    const char* value;

    assert(hdrset != NULL);

    // 设置头（小写）
    ret = dpqic_hdrset_set(hdrset, "content-type", "application/json");
    assert(ret == DPE_OK);

    // 用大写获取
    value = dpqic_hdrset_get(hdrset, "CONTENT-TYPE");
    assert(value != NULL);
    assert(strcmp(value, "application/json") == 0);

    // 用混合大小写获取
    value = dpqic_hdrset_get(hdrset, "Content-Type");
    assert(value != NULL);
    assert(strcmp(value, "application/json") == 0);

    // 用大写设置，应更新已有头
    ret = dpqic_hdrset_set(hdrset, "CONTENT-TYPE", "text/html");
    assert(ret == DPE_OK);

    value = dpqic_hdrset_get(hdrset, "content-type");
    assert(value != NULL);
    assert(strcmp(value, "text/html") == 0);

    printf("✓ Case insensitive test passed\n");

    // 清理
    dpqic_hdrset_del(hdrset);
}

// 测试边界情况
void test_hdrset_edge_cases()
{
    printf("=== Testing hdrset edge cases ===\n");

    dpqic_hdrset_t* hdrset = NULL;
    dpret_t ret;
    const char* value;
    int count;

    // 测试空头名称（NULL hdrset 也应无效）
    ret = dpqic_hdrset_set(NULL, "", "value");
    assert(ret == DPE_INVAL);

    // 测试 NULL 头名称
    ret = dpqic_hdrset_set(NULL, NULL, "value");
    assert(ret == DPE_INVAL);

    // 测试 NULL hdrset 指针
    value = dpqic_hdrset_get(NULL, "test");
    assert(value == NULL);

    count = dpqic_hdrset_count(NULL);
    assert(count == DPE_INVAL);

    // 现在创建 hdrset 进行后续测试
    hdrset = dpqic_hdrset_new(2);
    assert(hdrset != NULL);

    // 测试空值
    ret = dpqic_hdrset_set(hdrset, "Test-Header", "");
    assert(ret == DPE_OK);

    value = dpqic_hdrset_get(hdrset, "Test-Header");
    assert(value != NULL);
    assert(strcmp(value, "") == 0);

    printf("✓ Edge cases test passed\n");

    // 清理
    if (hdrset) {
        dpqic_hdrset_del(hdrset);
    }
}

// 测试大量头
void test_hdrset_large()
{
    printf("=== Testing hdrset with large number of headers ===\n");

    dpqic_hdrset_t* hdrset = dpqic_hdrset_new(2);
    dpret_t ret;
    const char* value;
    int count;
    char name[32], value_str[32];

    assert(hdrset != NULL);

    // 添加大量头
    for (int i = 0; i < 100; i++) {
        snprintf(name, sizeof(name), "Header-%d", i);
        snprintf(value_str, sizeof(value_str), "Value-%d", i);

        ret = dpqic_hdrset_set(hdrset, name, value_str);
        assert(ret == DPE_OK);
    }

    count = dpqic_hdrset_count(hdrset);
    assert(count == 100);

    // 验证部分头
    for (int i = 0; i < 10; i++) {
        snprintf(name, sizeof(name), "Header-%d", i);
        snprintf(value_str, sizeof(value_str), "Value-%d", i);

        value = dpqic_hdrset_get(hdrset, name);
        assert(value != NULL);
        assert(strcmp(value, value_str) == 0);
    }

    printf("✓ Large number of headers test passed\n");

    // 清理
    dpqic_hdrset_del(hdrset);
}

// 测试头删除（设值为 NULL）
void test_hdrset_delete()
{
    printf("=== Testing hdrset header deletion ===\n");

    dpqic_hdrset_t* hdrset = dpqic_hdrset_new(2);
    dpret_t ret;
    const char* value;
    int count;

    assert(hdrset != NULL);

    // 添加多个头
    ret = dpqic_hdrset_set(hdrset, "Header1", "Value1");
    assert(ret == DPE_OK);

    ret = dpqic_hdrset_set(hdrset, "Header2", "Value2");
    assert(ret == DPE_OK);

    ret = dpqic_hdrset_set(hdrset, "Header3", "Value3");
    assert(ret == DPE_OK);

    count = dpqic_hdrset_count(hdrset);
    assert(count == 3);

    // 通过设值为 NULL 删除 Header2
    ret = dpqic_hdrset_set(hdrset, "Header2", NULL);
    assert(ret == DPE_OK);

    count = dpqic_hdrset_count(hdrset);
    assert(count == 2);

    // 验证 Header2 已删除
    value = dpqic_hdrset_get(hdrset, "Header2");
    assert(value == NULL);

    // 验证其他头仍然存在
    value = dpqic_hdrset_get(hdrset, "Header1");
    assert(value != NULL);
    assert(strcmp(value, "Value1") == 0);

    value = dpqic_hdrset_get(hdrset, "Header3");
    assert(value != NULL);
    assert(strcmp(value, "Value3") == 0);

    // 删除 Header1
    ret = dpqic_hdrset_set(hdrset, "Header1", NULL);
    assert(ret == DPE_OK);

    count = dpqic_hdrset_count(hdrset);
    assert(count == 1);

    // 验证仅剩 Header3
    value = dpqic_hdrset_get(hdrset, "Header3");
    assert(value != NULL);
    assert(strcmp(value, "Value3") == 0);

    // 删除最后一个头（无需交换）
    ret = dpqic_hdrset_set(hdrset, "Header3", NULL);
    assert(ret == DPE_OK);

    count = dpqic_hdrset_count(hdrset);
    assert(count == 0);

    // 测试删除单元素 hdrset
    ret = dpqic_hdrset_set(hdrset, "SingleHeader", "SingleValue");
    assert(ret == DPE_OK);

    count = dpqic_hdrset_count(hdrset);
    assert(count == 1);

    ret = dpqic_hdrset_set(hdrset, "SingleHeader", NULL);
    assert(ret == DPE_OK);

    count = dpqic_hdrset_count(hdrset);
    assert(count == 0);

    // 测试删除后内存复用
    printf("Testing memory reuse after deletion...\n");
    ret = dpqic_hdrset_set(hdrset, "NewHeader1", "NewValue1");
    assert(ret == DPE_OK);

    ret = dpqic_hdrset_set(hdrset, "NewHeader2", "NewValue2");
    assert(ret == DPE_OK);

    count = dpqic_hdrset_count(hdrset);
    assert(count == 2);

    value = dpqic_hdrset_get(hdrset, "NewHeader1");
    assert(value != NULL);
    assert(strcmp(value, "NewValue1") == 0);

    value = dpqic_hdrset_get(hdrset, "NewHeader2");
    assert(value != NULL);
    assert(strcmp(value, "NewValue2") == 0);

    printf("✓ Header deletion test passed\n");

    // 清理
    dpqic_hdrset_del(hdrset);
}

int main()
{
    printf("Starting dpqic hdrset tests...\n\n");

    test_hdrset_basic();
    test_hdrset_at();
    test_hdrset_update();
    test_hdrset_case_insensitive();
    test_hdrset_edge_cases();
    test_hdrset_large();
    test_hdrset_delete();

    printf("\nAll dpqic hdrset tests passed!\n");
    return 0;
}
