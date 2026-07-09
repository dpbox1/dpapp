#pragma once

/** @file dplua.h
 *  @ingroup dplua
 *  @brief Lua 绑定入口：`dplua_start` 与 hdr 配置类型。
 */

#include "dpapp/dpapp.h"
#include "dpapp_config.h"

#define DPLUA_HDR_NAME_MAX 64

typedef struct dplua_arg dplua_arg_t;

/** @brief 单线程类型的 hdr 配置（对应 dpapp_hdr_t）。 */
typedef struct
{
    char init_name[DPLUA_HDR_NAME_MAX]; ///< __initNN 函数名
    dpbuf_t arg_buf;                    ///< string.buffer.encode 二进制编码
} dplua_hdr_t;

/** @brief 模块整体 hdrs 配置（由 __main__ 填充而来）。 */
typedef struct
{
    int type_count;
    dplua_hdr_t hdrs[DPAPP_TYPE_MAX];
} dplua_config_t;

#ifdef __cplusplus
extern "C"
{
#endif

// clang-format off
/** Lua 特定选项的多行帮助字符串。 */
#define DPLUA_ARG_HELP                                                        \
"Lua external options:\n"                                                     \
"-e [ --emode ]                           Run in embedded mode\n"

// clang-format on

dpret_t dplua_start(dpapp_arg_t* args);

/** @brief Lua 绑定：`dpctc_submit` 固定参数入口（topic wire id + arg）。 */
dpret_t dplua_add_ctc_submit(dpele_t* ctc, int64_t topic_id, dpv64_t arg);

/** @brief Lua 绑定：`dpctc_init_type` 固定参数入口（toid + detach）。 */
dpele_t* dplua_new_ctc(int toid, int detach);

/** @brief Lua 绑定：`dptmr_timeout` 固定参数入口（cache id + arg）。 */
dpret_t dplua_add_tmr_timeout(dpele_t* tmr, double sec, int64_t cache_id,
    dpv64_t arg);

#ifdef __cplusplus
}
#endif
