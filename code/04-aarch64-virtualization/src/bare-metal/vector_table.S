/* vector_table.S — EL2 向量表 + WFI 陷阱处理器
 *
 * guest(EL1) 执行 WFI 时，因 HCR_EL2.TWI=1 被陷阱到 EL2。
 * 这是"来自更低 EL 的 AArch64 同步异常"，落在 VBAR_EL2+0x400。
 */

.section .text.vector, "ax"
.balign 0x800
.global vector_table_el2

.macro ventry label
    b       \label
    .balign 0x80
.endm

vector_table_el2:
    /* +0x000 当前 EL(EL2) + SP_EL0 */
    ventry  unexpected_handler     /* 0x000 同步 */
    ventry  unexpected_handler     /* 0x080 IRQ  */
    ventry  unexpected_handler     /* 0x100 FIQ  */
    ventry  unexpected_handler     /* 0x180 SError */

    /* +0x200 当前 EL(EL2) + SP_ELx */
    ventry  unexpected_handler     /* 0x200 同步 */
    ventry  unexpected_handler     /* 0x280 IRQ  */
    ventry  unexpected_handler     /* 0x300 FIQ  */
    ventry  unexpected_handler     /* 0x380 SError */

    /* +0x400 低 EL + AArch64 —— WFI 陷阱走这里 */
    ventry  wfi_trap_handler       /* 0x400 同步 */
    ventry  unexpected_handler     /* 0x480 IRQ  */
    ventry  unexpected_handler     /* 0x500 FIQ  */
    ventry  unexpected_handler     /* 0x580 SError */

    /* +0x600 低 EL + AArch32 */
    ventry  unexpected_handler     /* 0x600 同步 */
    ventry  unexpected_handler     /* 0x680 IRQ  */
    ventry  unexpected_handler     /* 0x700 FIQ  */
    ventry  unexpected_handler     /* 0x780 SError */

/* ============================================================
 * 现场保存/恢复（保存全部 caller-saved 寄存器 + fp/lr）
 * ============================================================ */
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

/* ============================================================
 * WFI 陷阱处理器（+0x400 低 EL AArch64 同步）
 * guest 的 WFI 因 HCR_EL2.TWI 被捕获到这里。
 * ============================================================ */
wfi_trap_handler:
    save_all

    mrs     x0, esr_el2          /* 异常原因（EC=0x01 Trapped WFI）*/
    mrs     x1, elr_el2          /* guest 的 WFI 地址 */
    bl      el2_handler_c        /* C 处理：打印 + 模拟 + 跳过 WFI */

    restore_all
    eret

/* ============================================================
 * 未预期的向量：死循环
 * ============================================================ */
unexpected_handler:
    wfe
    b       unexpected_handler