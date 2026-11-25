/* boot.S — 裸机启动：EL3 引导，配置系统计数器频率与异常路由
 *
 * QEMU -machine virt,secure=on 复位进入 EL3（Secure 世界）。
 * 参考 ARM 官方 generic_timer 示例（startup.s），全程在 EL3 运行：
 *   1) 设 SP_EL3 栈、VBAR_EL3
 *   2) 设 CNTFRQ_EL0（固件在最高 EL 填系统计数频率）
 *   3) 设 SCR_EL3：ST=1（S.EL1 可访问 CNTPS）、RW=1（EL1 为 AArch64）、
 *      EA/FIQ/IRQ=1（SError/FIQ/IRQ 路由到 EL3）
 * 之后跳 C 入口 main（GIC 与定时器在 main 里配置）。
 */

.section .text.boot, "ax"
.global _start

_start:
    // 向量表基址
    ldr     x19, =vector_table
    msr     vbar_el3, x19

    // EL3 异常栈（EL3 恒用 SP_EL3，即当前 sp）
    ldr     x2, =_stack_top
    mov     sp, x2

    // CNTFRQ_EL0 = 62.5MHz（QEMU virt 默认频率）
    // 文档 §2.1：CNTFRQ 硬件不自动填，由固件在 EL3 写入，其他 EL 只读
    ldr     x2, =62500000
    msr     cntfrq_el0, x2

    // SCR_EL3：
    //   bit11 ST  = 1 -> S.EL1 可访问 CNTPS_*（安全物理定时器）
    //   bit10 RW  = 1 -> 下一级 EL1 为 AArch64
    //   bit3  EA  = 1 -> SError 路由到 EL3
    //   bit2  FIQ = 1 -> FIQ 路由到 EL3
    //   bit1  IRQ = 1 -> IRQ 路由到 EL3
    mov     x2, #((1 << 11) | (1 << 10) | (1 << 3) | (1 << 2) | (1 << 1))
    msr     scr_el3, x2

    bl      main

hang:
    wfe
    b       hang
