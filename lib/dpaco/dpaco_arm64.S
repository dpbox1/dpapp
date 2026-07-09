    .text
    .arch   armv8-a
    .p2align 4
    .globl  cocontext_swap
    .type   cocontext_swap, %function

    // cocontext_swap(void* cur_ctx, void* next_ctx)
    // x0 = 当前上下文, x1 = 下一上下文
    // 布局（须与 dpaco_arch.h 一致）:
    // regs[0..10] -> x19..x29  (偏移 0 .. 80)
    // regs[11] -> lr           (偏移 88)
    // regs[12] -> sp           (偏移 96)
    // regs[13] -> x0           (偏移 104)
    // vregs[0..7] -> q8..q15   (偏移 112,128,...,224)
cocontext_swap:
    stp x19, x20, [x0, #0]      // x19->0, x20->8
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    str x29,      [x0, #80]
    // 剥离 LR 的 PAC，使恢复的上下文可在不同栈上返回（PAC 绑定 SP）。
    // 编码 0xd50320ff = XPACLRI（无 FEAT_PAuth 的 CPU 上为 NOP）。
    .inst 0xd50320ff
    str lr,       [x0, #88]

    mov x9, sp
    str x9,       [x0, #96]

    mov x9, xzr
    str x9,       [x0, #104]

    stp q8,  q9,  [x0, #112]
    stp q10, q11, [x0, #144]
    stp q12, q13, [x0, #176]
    stp q14, q15, [x0, #208]

    ldp x19, x20, [x1, #0]
    ldp x21, x22, [x1, #16]
    ldp x23, x24, [x1, #32]
    ldp x25, x26, [x1, #48]
    ldp x27, x28, [x1, #64]
    ldr x29,      [x1, #80]
    ldr lr,       [x1, #88]

    ldr x9,       [x1, #96]
    mov sp, x9

    ldr x0,       [x1, #104]

    ldp q8,  q9,  [x1, #112]
    ldp q10, q11, [x1, #144]
    ldp q12, q13, [x1, #176]
    ldp q14, q15, [x1, #208]

    ret

    .size cocontext_swap, . - cocontext_swap
