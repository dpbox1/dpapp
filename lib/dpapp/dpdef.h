/** @file dpdef.h
 *  @ingroup dpapp_def
 *  @brief dpapp 公共基础类型与宏。
 *
 *  `dpv32_t` / `dpv64_t` 为通用 tagged union，承载指针、小整数、协程链路等；
 *  `DPV64_*` 构造常用值。`DPEVT_*` 对齐 poll(2) 就绪位。`dprole_e` 标注端点角色。 */

#pragma once

#include "dpapp/dpret.h"
#include <sys/poll.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include <float.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief 32 位 tagged union（小标量、浮点、状态码）。 */
typedef union
{
    uint32_t u32;
    int32_t s32;
    float f32;
    int ret;
    uint32_t opt;
    uint32_t evs;
    uint32_t flags;
    uint8_t bytes[sizeof(uint32_t)];
} dpv32_t;

/** @brief 64 位 tagged union（指针、I/O 标签、协程链路、错误码）。 */
typedef union
{
    void* ptr;
    const void* cptr;
    uint64_t u64;
    int64_t s64;
    double f64;
    uint8_t bytes[sizeof(void*)];
    struct
    {
        dpv32_t a32;
        dpv32_t b32;
    };
    struct
    {
        uint32_t utype : 8;
        uint32_t uflag : 24;
        uint32_t offset;
    };
} dpv64_t;

/** @brief 析构回调：接收 `dpv64_t` 并释放/消费其资源。 */
typedef void (*dpv64_del_f)(dpv64_t);

// clang-format off
/** @brief 构造 `.a32.s32 = err` 的 dpv64_t（状态/errno 风格）。 */
#define DPV64_RES(err) ((dpv64_t){.a32.s32 = (err)})
/** @brief 构造 .ptr 指针的 dpv64_t。 */
#define DPV64_PTR(ptr__) ((dpv64_t){.ptr = (void*)ptr__})
/** @brief 空指针 dpv64_t。 */
#define DPV64_NULL (dpv64_t){.ptr = NULL}

#if defined(__x86_64__) || defined(_M_X64)
/** @brief 将 `va_list` 打包为 dpv64_t（x86_64 直接存指针）。 */
#define DPV64_VA(va__) ((dpv64_t){.ptr = (void*)va__})
#elif defined(__aarch64__) || defined(__arm64__)
/** @brief 将 `va_list` 打包为 dpv64_t（aarch64 存 `va_list` 地址）。 */
#define DPV64_VA(va__) ((dpv64_t){.ptr = &va__})
#elif defined(__riscv) || defined(__riscv__)
/** @brief 将 `va_list` 打包为 dpv64_t（riscv 存 `va_list` 地址）。 */
#define DPV64_VA(va__) ((dpv64_t){.ptr = &va__})
#else
#error "Unsupported architecture for va_list"
#endif

/** @brief 构造 `.s32 = err` 的 dpv32_t。 */
#define DPV32_RES(err) (dpv32_t){.s32 = err}
/** @brief 零值 dpv32_t。 */
#define DPV32_NULL (dpv32_t){.s32 = 0}
// clang-format on

/** @name DPEVT 就绪事件掩码 */
/**@{*/
/**@{*/
#define DPEVT_NAN 0x00 ///< 无位
#define DPEVT_IN  0x01 ///< POLLIN：可读
#define DPEVT_PRI 0x02 ///< POLLPRI：优先可读
#define DPEVT_OUT 0x04 ///< POLLOUT：可写
#define DPEVT_ERR 0x08 ///< POLLERR
#define DPEVT_HUP 0x10 ///< POLLHUP
/**@}*/

/** @brief 全部输入相关事件。 */
#define DPEVT_AIN (DPEVT_IN | DPEVT_PRI)
/** @brief 全部错误/挂断事件。 */
#define DPEVT_AERR (DPEVT_ERR | DPEVT_HUP)
/** @brief 典型 I/O 就绪事件。 */
#define DPEVT_AIO (DPEVT_IN | DPEVT_OUT | DPEVT_PRI)
/** @brief dpapp 使用的完整 poll 掩码（低 5 位）。 */
#define DPEVT_ALL 0x1F

/** @brief 由 poll 事件掩码推导 dpret_t（HUP: @ref DPE_CONNRESET / ERR: @ref DPE_BADFD ）。 */
#define DPEVT_GET_ERR(evs__)                                                        \
    ({                                                                              \
        dpret_t ret = DPE_OK;                                                       \
        if ((evs__) & DPEVT_HUP) {                                                  \
            ret = DPE_CONNRESET;                                                    \
        } else if ((evs__) & DPEVT_ERR) {                                           \
            ret = DPE_BADFD;                                                        \
        }                                                                           \
        ret;                                                                        \
    })

/** @brief TCP/TLS/QUIC 端点角色。 */
typedef enum
{
    DPROLE_CLIENT = 1, ///< 客户端
    DPROLE_SERVER = 2, ///< 服务端
    DPROLE_UNSURE = 3, ///< 未确定
} dprole_e;

/** @brief `a` 与 `b` 的较大值（各求值一次）。 */
#define DP_MAX(a, b) ((a) > (b) ? (a) : (b))
/** @brief `a` 与 `b` 的较小值。 */
#define DP_MIN(a, b) ((a) < (b) ? (a) : (b))

/** @brief `free(ptr)` 并将非 NULL 的 `ptr` 置 NULL。 */
#define DP_FREE(ptr__)                                                              \
    if ((ptr__)) {                                                                  \
        free((ptr__));                                                              \
        (ptr__) = NULL;                                                             \
    }

/** @brief 若 `exp__` 为真，`free(ptr__)` 并清零。 */
#define DP_CHECK_FREE(exp__, ptr__)                                                 \
    if ((exp__) && (ptr__)) {                                                       \
        free((ptr__));                                                              \
        (ptr__) = NULL;                                                             \
    }

/** @brief 若 `exp__` 为真，`return err__`。 */
#define DP_CHECK_RETURN(exp__, err__)                                               \
    if ((exp__)) {                                                                  \
        return err__;                                                               \
    }

/** @brief 若 `exp__` 为真，执行 `proc__(...)` 后 `return err__`。 */
#define DP_CHECK_PROC_RETURN(exp__, err__, proc__, ...)                             \
    if ((exp__)) {                                                                  \
        proc__(__VA_ARGS__);                                                        \
        return err__;                                                               \
    }

#ifdef __cplusplus
}
#endif
