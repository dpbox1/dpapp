.globl cocontext_swap
.type  cocontext_swap, @function

cocontext_swap:
    endbr64
    movq %rsp, 96(%rdi)
    movq %rbx, 88(%rdi)
    movq %rcx, 80(%rdi)
    movq %rdx, 72(%rdi)
    movq %rsi, 64(%rdi)
    movq %rdi, 56(%rdi)
    movq %rbp, 48(%rdi)
    movq %r8, 40(%rdi)
    movq %r9, 32(%rdi)
    movq %r12, 24(%rdi)
    movq %r13, 16(%rdi)
    movq %r14, 8(%rdi)
    movq %r15, (%rdi)

    movq (%rsi), %r15
    movq 8(%rsi), %r14
    movq 16(%rsi), %r13
    movq 24(%rsi), %r12
    movq 32(%rsi), %r9
    movq 40(%rsi), %r8
    movq 48(%rsi), %rbp
    movq 56(%rsi), %rdi
    movq 72(%rsi), %rdx
    movq 80(%rsi), %rcx
    movq 88(%rsi), %rbx
    movq 96(%rsi), %rsp
    movq 64(%rsi), %rsi

    ret

# 标记栈不可执行，指示 IBT 兼容性。
# CET 影子栈不支持：需要在 cocontext_swap 中保存/恢复 SSP，
# 并为 cocontext_t 添加 SSP 槽位。
    .section .note.GNU-stack, "", @progbits
