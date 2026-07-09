    .text
    .p2align 2
    .globl cocontext_swap
    .type  cocontext_swap, @function

    // cocontext_swap(void* cur_ctx, void* next_ctx)
    // a0 = 当前上下文, a1 = 下一上下文
    //
    // 布局（须与 dpaco_arch.h RISC-V 定义一致）:
    // regs[0..11] -> s0..s11   (偏移 0 .. 88)
    // regs[12] -> ra            (偏移 96)
    // regs[13] -> sp            (偏移 104)
    // regs[14] -> a0            (偏移 112)
    // fregs[0..11] -> fs0..fs11 = f8,f9,f18..f27 (偏移 120..208)
    //
    // 上下文总大小 = 216 字节 (0..215)

cocontext_swap:
    // 保存被调用者保存整数寄存器 s0..s11（到 cur->regs）
    sd s0,   0(a0)
    sd s1,   8(a0)
    sd s2,  16(a0)
    sd s3,  24(a0)
    sd s4,  32(a0)
    sd s5,  40(a0)
    sd s6,  48(a0)
    sd s7,  56(a0)
    sd s8,  64(a0)
    sd s9,  72(a0)
    sd s10, 80(a0)
    sd s11, 88(a0)

    // 保存 ra、sp
    sd ra,  96(a0)
    sd sp, 104(a0)

    // 存入零到 cur->regs[a0]（恢复后 a0 = 0）
    sd x0, 112(a0)   // x0 是零寄存器

    // 保存被调用者保存浮点寄存器 fs0..fs11（RISC-V ABI 中的 f8,f9, f18..f27）
    fsd f8,  120(a0)
    fsd f9,  128(a0)
    fsd f18, 136(a0)
    fsd f19, 144(a0)
    fsd f20, 152(a0)
    fsd f21, 160(a0)
    fsd f22, 168(a0)
    fsd f23, 176(a0)
    fsd f24, 184(a0)
    fsd f25, 192(a0)
    fsd f26, 200(a0)
    fsd f27, 208(a0)

    // --- 从 next (a1) 恢复 ---
    ld s0,   0(a1)
    ld s1,   8(a1)
    ld s2,  16(a1)
    ld s3,  24(a1)
    ld s4,  32(a1)
    ld s5,  40(a1)
    ld s6,  48(a1)
    ld s7,  56(a1)
    ld s8,  64(a1)
    ld s9,  72(a1)
    ld s10, 80(a1)
    ld s11, 88(a1)

    // 恢复 ra、sp
    ld ra,  96(a1)
    ld sp, 104(a1)

    // 恢复 a0
    ld a0, 112(a1)

    // 恢复浮点 fs0..fs11（f8,f9, f18..f27）
    fld f8,  120(a1)
    fld f9,  128(a1)
    fld f18, 136(a1)
    fld f19, 144(a1)
    fld f20, 152(a1)
    fld f21, 160(a1)
    fld f22, 168(a1)
    fld f23, 176(a1)
    fld f24, 184(a1)
    fld f25, 192(a1)
    fld f26, 200(a1)
    fld f27, 208(a1)

    // 跳转到恢复的 ra（返回到 next）
    jalr x0, ra, 0

    .size cocontext_swap, . - cocontext_swap
