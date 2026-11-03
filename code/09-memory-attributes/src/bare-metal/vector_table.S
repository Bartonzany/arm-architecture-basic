/* vector_table.S — AArch64 异常向量表（EL1）
 *
 * 本示例聚焦 MMU 内存属性引发的同步异常：
 *   - 权限故障（读只读/写非法）  -> 数据中止
 *   - 执行权限故障（跳 PXN 页）  -> 指令中止
 *   - 对齐故障（未对齐访问）     -> 数据中止
 *   - 访问标志故障（AF=0）       -> 数据中止
 * 全部走 +0x200 的 current_el_sync_handler。
 *
 * 同步处理器从 C 函数 sync_handler_c(esr, elr, far) 拿到修复后的返回地址，
 * 写入 ELR_EL1 后 ERET 回到主流程继续。
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

    /* +0x200 当前 EL + SP_ELx（本示例实际使用这组）*/
    ventry  current_el_sync_handler /* 0x200 同步 */
    ventry  unexpected_handler      /* 0x280 IRQ  */
    ventry  unexpected_handler      /* 0x300 FIQ  */
    ventry  unexpected_handler      /* 0x380 SError */

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
 * 现场保存/恢复宏（保存 caller-saved 寄存器 + x29 + x30）
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
 * 当前 EL 同步异常处理器
 *   x0 = ESR_EL1, x1 = ELR_EL1, x2 = FAR_EL1
 *   C 函数返回修复后的 ELR（x0），写回 ELR_EL1 后 ERET
 * ============================================================ */
current_el_sync_handler:
    save_all

    mrs     x0, esr_el1
    mrs     x1, elr_el1
    mrs     x2, far_el1
    bl      sync_handler_c
    msr     elr_el1, x0          // x0 = 修复后的返回地址

    restore_all
    eret

/* ============================================================
 * 未预期的向量：打印提示后死循环
 * ============================================================ */
unexpected_handler:
    bl      unexpected_handler_c
hang_vector:
    wfe
    b       hang_vector
