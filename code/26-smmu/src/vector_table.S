/* vector_table.S — AArch64 异常向量表（EL1），覆盖全部 16 个入口
 *
 * 4 类异常 × 4 种来源 = 16 入口：
 *   +0x000 当前 EL + SP_EL0  (同步/IRQ/FIQ/SError)
 *   +0x200 当前 EL + SP_ELx  (同步/IRQ/FIQ/SError)
 *   +0x400 低 EL + AArch64   (同步/IRQ/FIQ/SError)
 *   +0x600 低 EL + AArch32   (同步/IRQ/FIQ/SError)
 *
 * 本示例运行在 EL1：
 *   SVC（同步）-> +0x200 的 current_el_sync_handler
 *   定时器 IRQ    -> +0x280 的 current_el_irq_handler
 *   FIQ/SError    -> +0x300/+0x380 对应 handler（触发条件见 README）
 * 其余入口（SP_EL0 组、低 EL 组）跳 unexpected_handler。
 */

.section .text.vector, "ax"
.balign 0x800
.global vector_table

/* 每个向量入口：跳转到 label，然后对齐到下一个 0x80 边界 */
.macro ventry label
    b       \label
    .balign 0x80
.endm

vector_table:
    /* +0x000 当前 EL + SP_EL0 */
    ventry  unexpected_handler     /* 0x000 同步 */
    ventry  unexpected_handler     /* 0x080 IRQ  */
    ventry  unexpected_handler     /* 0x100 FIQ  */
    ventry  unexpected_handler     /* 0x180 SError */

    /* +0x200 当前 EL + SP_ELx（本示例实际使用这组）*/
    ventry  current_el_sync_handler /* 0x200 同步 */
    ventry  current_el_irq_handler  /* 0x280 IRQ  */
    ventry  current_el_fiq_handler  /* 0x300 FIQ  */
    ventry  current_el_serror_handler /* 0x380 SError */

    /* +0x400 低 EL + AArch64 */
    ventry  unexpected_handler     /* 0x400 同步 */
    ventry  unexpected_handler     /* 0x480 IRQ  */
    ventry  unexpected_handler     /* 0x500 FIQ  */
    ventry  unexpected_handler     /* 0x580 SError */

    /* +0x600 低 EL + AArch32 */
    ventry  unexpected_handler     /* 0x600 同步 */
    ventry  unexpected_handler     /* 0x680 IRQ  */
    ventry  unexpected_handler     /* 0x700 FIQ  */
    ventry  unexpected_handler     /* 0x780 SError */

/* ============================================================
 * 现场保存/恢复宏
 * 中断是异步的，处理器不知道主程序正在用哪些寄存器，因此必须保存
 * 全部 caller-saved 寄存器（x0-x18）+ 帧指针 x29 + 返回地址 x30。
 * （x19-x28 是 callee-saved，由 C 函数自行保存。）
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
 * 当前 EL 同步异常处理器（SVC 等）
 * ============================================================ */
current_el_sync_handler:
    save_all

    mrs     x0, esr_el1
    mrs     x1, elr_el1
    mrs     x2, spsr_el1
    bl      svc_handler_c

    restore_all
    eret

/* ============================================================
 * 当前 EL IRQ 处理器
 * ============================================================ */
current_el_irq_handler:
    save_all

    bl      irq_handler_c

    restore_all
    eret

/* ============================================================
 * 当前 EL FIQ 处理器
 * ============================================================ */
current_el_fiq_handler:
    save_all

    bl      fiq_handler_c

    restore_all
    eret

/* ============================================================
 * 当前 EL SError 处理器
 * ============================================================ */
current_el_serror_handler:
    save_all

    bl      serror_handler_c

    restore_all
    eret

/* ============================================================
 * 未预期的向量（SP_EL0 组、低 EL 组）：打印提示后死循环
 * ============================================================ */
unexpected_handler:
    bl      unexpected_handler_c
hang_vector:
    wfe
    b       hang_vector
