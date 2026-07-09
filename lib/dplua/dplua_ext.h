/** @file dplua_ext.h
 *  @ingroup dplua_ext
 *  @brief Lua 绑定 C 扩展：Snowflake ID 与任务规格。
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#include "dpapp/dpdef.h"

struct dpele;
typedef struct dpele dpele_t;

/** 与 dpcwc_ext.h 一致：生成 Snowflake ID；out_str 非 NULL 时写入短字符串。 */
uint64_t dpid_next(char* out_str);
uint64_t dpid_2u64(const char* str);
void dpid_2str(uint64_t id, char* out_str);

/** @brief 通用任务规格（CTC payload 等）。 */
struct dptask_comspec
{
    char* info;
    char* body;
    char* args;
    char* result;
    int ok;
    int reto_node;
    int reto_name;
};
typedef struct dptask_comspec dptask_comspec_t;

dptask_comspec_t* dptask_comspec_new(const char* info, const char* body,
    const char* args, int reto_node, int reto_name);
void dptask_comspec_set_result(dptask_comspec_t* comspec, int ok,
    const char* result);
void dptask_comspec_del(dptask_comspec_t* comspec);
void dptask_comspec_del2(void* comspec);

#ifdef __cplusplus
}
#endif
