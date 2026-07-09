#pragma once

#include "dpapp/dpdef.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define DPACO_ARCH_AMD64 1
#elif defined(__aarch64__) || defined(__arm64__)
#define DPACO_ARCH_ARM64 1
#elif defined(__riscv) || defined(__riscv__)
#define DPACO_ARCH_RISCV 1
#else
#error "Unsupported architecture for coroutine context switching"
#endif

#if DPACO_ARCH_AMD64
enum
{
    DPACO_R15 = 0,
    DPACO_R14,
    DPACO_R13,
    DPACO_R12,
    DPACO_R9,
    DPACO_R8,
    DPACO_RBP,
    DPACO_RDI,
    DPACO_RSI,
    DPACO_RDX,
    DPACO_RCX,
    DPACO_RBX,
    DPACO_RSP,
};
#define DPACO_REG_COUNT 13

typedef struct cocontext
{
    void* regs[DPACO_REG_COUNT];
} cocontext_t;

#elif DPACO_ARCH_ARM64
enum
{
    DPACO_X19 = 0,
    DPACO_X20,
    DPACO_X21,
    DPACO_X22,
    DPACO_X23,
    DPACO_X24,
    DPACO_X25,
    DPACO_X26,
    DPACO_X27,
    DPACO_X28,
    DPACO_X29,    // 帧指针
    DPACO_REG_LR, /* x30 链接寄存器 */
    DPACO_REG_SP,
    DPACO_REG_X0,
};
#define DPACO_REG_COUNT  14
#define DPACO_VREG_COUNT 8

typedef struct cocontext
{
    uint64_t regs[DPACO_REG_COUNT];      /* x19..x29, lr, sp, x0 */
    uint8_t vregs[DPACO_VREG_COUNT][16]; /* q8..q15（每个 128 位） */
} cocontext_t;

#elif DPACO_ARCH_RISCV
enum
{

    DPACO_REG_S0 = 0,
    DPACO_REG_S1,
    DPACO_REG_S2,
    DPACO_REG_S3,
    DPACO_REG_S4,
    DPACO_REG_S5,
    DPACO_REG_S6,
    DPACO_REG_S7,
    DPACO_REG_S8,
    DPACO_REG_S9,
    DPACO_REG_S10,
    DPACO_REG_S11,
    DPACO_REG_RA,
    DPACO_REG_SP,
    DPACO_REG_A0,
};

#define DPACO_REG_COUNT  15
#define DPACO_FREG_COUNT 12

typedef struct cocontext_rv
{
    uint64_t regs[DPACO_REG_COUNT];   /* s0..s11, ra, sp, a0 */
    uint64_t fregs[DPACO_FREG_COUNT]; /* fs0..fs11（双精度） */
} cocontext_t;
#endif

extern void cocontext_swap(cocontext_t* curr, cocontext_t* next);

extern void cocontext_init(cocontext_t* ctx, void* stack, int size, void* func,
    void* arg);

#ifdef __cplusplus
}
#endif
