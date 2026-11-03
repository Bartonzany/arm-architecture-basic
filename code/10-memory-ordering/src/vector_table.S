/* vector_table.S — AArch64 异常向量表（EL1）
 *
 * 本示例聚焦两类同步异常：
 *   - 对齐故障：非对齐的 LDAR/LDAXP/STLR（§3 对齐规则）
 *   - FP/SIMD 捕获：CPACR_EL1.FPEN=0 时的 FP 指令（§4 上下文同步）
 * 都走 +0x200 的 current_el_sync_handler，由 C 函数 sync_handler_c
 * 按"当前测试"决定修复后的 ELR 并 ERET。
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
    ventry  unexpected_handler
    ventry  unexpected_handler
    ventry  unexpected_handler
    ventry  unexpected_handler

    /* +0x200 当前 EL + SP_ELx */
    ventry  current_el_sync_handler
    ventry  unexpected_handler
    ventry  unexpected_handler
    ventry  unexpected_handler

    /* +0x400 低 EL + AArch64 */
    ventry  unexpected_handler
    ventry  unexpected_handler
    ventry  unexpected_handler
    ventry  unexpected_handler

    /* +0x600 低 EL + AArch32 */
    ventry  unexpected_handler
    ventry  unexpected_handler
    ventry  unexpected_handler
    ventry  unexpected_handler

/* ============================================================
 * 现场保存/恢复宏
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
    msr     elr_el1, x0

    restore_all
    eret

unexpected_handler:
    bl      unexpected_handler_c
hang_vector:
    wfe
    b       hang_vector