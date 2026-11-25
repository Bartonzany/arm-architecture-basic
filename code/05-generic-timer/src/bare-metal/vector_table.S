/* vector_table.S — AArch64 EL3 异常向量表（16 入口），FIQ 处理器
 *
 * 本示例全程运行在 EL3（Secure 世界），安全物理定时器（INTID 29）与非安全
 * EL1 物理定时器（INTID 30）都被配成 Group 0（FIQ），并由 SCR_EL3.FIQ=1
 * 路由到 EL3。EL3 恒用 SP_EL3，因此 FIQ 命中 +0x300（当前 EL + SP_ELx）。
 *
 * 参考 ARM 官方 generic_timer 示例（el3_vectors.s）的 FIQ 首层处理器。
 */

.section .text.vector, "ax"
.balign 0x800
.global vector_table

.macro ventry label
    b       \label
    .balign 0x80
.endm

vector_table:
    /* +0x000 当前 EL + SP_EL0 */
    ventry  unexpected_handler     /* 0x000 同步 */
    ventry  unexpected_handler     /* 0x080 IRQ  */
    ventry  fiq_handler            /* 0x100 FIQ  */
    ventry  unexpected_handler     /* 0x180 SError */

    /* +0x200 当前 EL + SP_ELx */
    ventry  unexpected_handler     /* 0x200 同步 */
    ventry  unexpected_handler     /* 0x280 IRQ  */
    ventry  fiq_handler            /* 0x300 FIQ（本示例走这里）*/
    ventry  unexpected_handler     /* 0x380 SError */

    /* +0x400 低 EL + AArch64 */
    ventry  unexpected_handler     /* 0x400 同步 */
    ventry  unexpected_handler     /* 0x480 IRQ  */
    ventry  fiq_handler            /* 0x500 FIQ  */
    ventry  unexpected_handler     /* 0x580 SError */

    /* +0x600 低 EL + AArch32 */
    ventry  unexpected_handler     /* 0x600 同步 */
    ventry  unexpected_handler     /* 0x680 IRQ  */
    ventry  fiq_handler            /* 0x700 FIQ  */
    ventry  unexpected_handler     /* 0x780 SError */

/* 现场保存/恢复：FIQ 是异步的，保存 caller-saved 寄存器 + fp/lr */
.macro save_all
    stp     x0,  x1,  [sp, #-16]!
    stp     x2,  x3,  [sp, #-16]!
    stp     x4,  x5,  [sp, #-16]!
    stp     x6,  x7,  [sp, #-16]!
    stp     x8,  x9,  [sp, #-16]!
    stp     x10, x11, [sp, #-16]!
    stp     x12, x13, [sp, #-16]!
    stp     x14, x15, [sp, #-16]!
    stp     x16, x17, [sp, #-16]!
    stp     x18, x29, [sp, #-16]!
    str     x30, [sp, #-16]!
.endm

.macro restore_all
    ldr     x30, [sp], #16
    ldp     x18, x29, [sp], #16
    ldp     x16, x17, [sp], #16
    ldp     x14, x15, [sp], #16
    ldp     x12, x13, [sp], #16
    ldp     x10, x11, [sp], #16
    ldp     x8,  x9,  [sp], #16
    ldp     x6,  x7,  [sp], #16
    ldp     x4,  x5,  [sp], #16
    ldp     x2,  x3,  [sp], #16
    ldp     x0,  x1,  [sp], #16
.endm

/* FIQ 处理器：保存现场 -> C 的 fiq_handler_c -> 恢复 -> eret */
fiq_handler:
    save_all
    bl      fiq_handler_c
    restore_all
    eret

/* 未预期向量：打印诊断后死循环 */
unexpected_handler:
    bl      unexpected_handler_c
hang_vector:
    wfe
    b       hang_vector
