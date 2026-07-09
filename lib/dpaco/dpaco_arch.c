#include "dpaco/dpaco_arch.h"
#include <string.h>

void cocontext_init(cocontext_t* ctx, void* stack, int size, void* func, void* arg)
{
    memset(ctx, 0, sizeof(cocontext_t));

#ifdef DPACO_ARCH_AMD64
    char* sp = stack + size - sizeof(void*);
    sp = (char*)((intptr_t)sp & -16LL);
    *(void**)sp = (void*)func;
    ctx->regs[DPACO_RSP] = sp;
    ctx->regs[DPACO_RDI] = arg;
#elif DPACO_ARCH_ARM64
    void* sp = (void*)((uintptr_t)(stack + size) & ~(uintptr_t)0xF);
    ctx->regs[DPACO_REG_SP] = (uint64_t)sp;
    ctx->regs[DPACO_REG_X0] = (uint64_t)arg;
    ctx->regs[DPACO_REG_LR] = (uint64_t)func;
#elif DPACO_ARCH_RISCV
    void* sp = (void*)((uintptr_t)(stack + size) & ~(uintptr_t)0xFULL);
    ctx->regs[DPACO_REG_SP] = (uint64_t)sp;
    ctx->regs[DPACO_REG_A0] = (uint64_t)arg;
    ctx->regs[DPACO_REG_RA] = (uint64_t)func;
#endif
}
