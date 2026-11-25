/* vector_table.S — 共享异常向量表（AArch64），EL1/EL2/EL3 共用
 *
 * 16 入口布局（每组 0x80 字节）：
 *   +0x000 当前 EL + SP_EL0   （同步/IRQ/FIQ/SError）
 *   +0x200 当前 EL + SP_ELx
 *   +0x400 低 EL + AArch64     <- SVC/HVC/SMC、BRK/UDF、定时器 IRQ 走这组
 *   +0x600 低 EL + AArch32
 *
 * 本示例所有异常都从低 EL 以 AArch64 触发：
 *   SVC(EL0->EL1)、HVC(EL1->EL2)、SMC(EL2->EL3)、BRK/UDF(EL0->EL1)
 *   均命中 +0x400 的 lower_el_sync_handler；
 *   定时器 IRQ 命中 +0x480 的 lower_el_irq_handler。
 * 三个级别共用一张表：VBAR_EL1 = VBAR_EL2 = VBAR_EL3 = vector_table。
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
    ventry  unexpected_handler     /* 0x100 FIQ  */
    ventry  unexpected_handler     /* 0x180 SError */

    /* +0x200 当前 EL + SP_ELx */
    ventry  unexpected_handler     /* 0x200 同步 */
    ventry  unexpected_handler     /* 0x280 IRQ  */
    ventry  unexpected_handler     /* 0x300 FIQ  */
    ventry  unexpected_handler     /* 0x380 SError */

    /* +0x400 低 EL + AArch64 */
    ventry  lower_el_sync_handler  /* 0x400 同步（SVC/HVC/SMC/BRK/UDF）*/
    ventry  lower_el_irq_handler   /* 0x480 IRQ（定时器）*/
    ventry  unexpected_handler     /* 0x500 FIQ  */
    ventry  unexpected_handler     /* 0x580 SError */

    /* +0x600 低 EL + AArch32 */
    ventry  unexpected_handler     /* 0x600 同步 */
    ventry  unexpected_handler     /* 0x680 IRQ  */
    ventry  unexpected_handler     /* 0x700 FIQ  */
    ventry  unexpected_handler     /* 0x780 SError */

/* 现场保存/恢复：中断是异步的，保存全部 caller-saved 寄存器 + fp/lr */
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

/* 低 EL 同步异常处理器（SVC/HVC/SMC/BRK/UDF 的公共入口）
 *
 * 进入时硬件已换到目标 EL 的 SP_ELx，并自动保存好 SPSR_ELx/ELR_ELx/ESR_ELx。
 * 这里按当前级别读对应寄存器，交给 C 的 dispatch_sync(level, esr, elr, spsr)。
 * dispatch_sync 返回 1 表示需要把 ELR_ELx +4（跳过 BRK/UDF 等触发指令），
 * 返回 0 表示 ELR 已指向下一条指令（SVC/HVC/SMC），直接 eret 即可。
 */
lower_el_sync_handler:
    save_all

    // x19 = 当前异常级别（callee-saved，跨 dispatch 调用不丢）
    mrs     x0, currentel
    lsr     x0, x0, #2
    and     x0, x0, #3
    mov     x19, x0

    // 按级别读 ESR / ELR / SPSR 到 x1/x2/x3
    cmp     x0, #3
    b.eq    1f
    cmp     x0, #2
    b.eq    2f
    /* EL1 */
    mrs     x1, esr_el1
    mrs     x2, elr_el1
    mrs     x3, spsr_el1
    b       3f
2:  /* EL2 */
    mrs     x1, esr_el2
    mrs     x2, elr_el2
    mrs     x3, spsr_el2
    b       3f
1:  /* EL3 */
    mrs     x1, esr_el3
    mrs     x2, elr_el3
    mrs     x3, spsr_el3
3:
    bl      dispatch_sync           // dispatch_sync(level=x0, esr=x1, elr=x2, spsr=x3)
                                    // 返回 x0 = 是否需要跳过触发指令

    cbz     x0, 4f
    // 需要跳过触发指令：把对应 ELR_ELx 加 4
    mov     x4, #4
    cmp     x19, #3
    b.eq    5f
    cmp     x19, #2
    b.eq    6f
    mrs     x5, elr_el1
    add     x5, x5, x4
    msr     elr_el1, x5
    b       4f
6:  mrs     x5, elr_el2
    add     x5, x5, x4
    msr     elr_el2, x5
    b       4f
5:  mrs     x5, elr_el3
    add     x5, x5, x4
    msr     elr_el3, x5
4:
    restore_all
    eret

/* 低 EL IRQ 处理器（定时器中断，EL0 触发 -> 送入 EL1） */
lower_el_irq_handler:
    save_all
    bl      irq_handler_c
    restore_all
    eret

/* 未预期向量：打印诊断信息后死循环（便于定位路由错误） */
unexpected_handler:
    bl      unexpected_handler_c
hang_vector:
    wfe
    b       hang_vector