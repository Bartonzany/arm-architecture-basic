/* boot.S — 裸机启动：从复位 EL 逐级降级到 EL0（EL3 -> EL2 -> EL1 -> EL0）
 *
 * QEMU -machine virt,secure=on,virtualization=on 复位进入 EL3（已验证）。
 * 降级靠 ERET：在每级先配置好
 *   1) 该级 VBAR_ELx        —— 指向同一张共享向量表
 *   2) 该级栈指针           —— 当前 EL 用 mov sp（就是 SP_ELx）；EL0 栈用 msr sp_el0
 *   3) 该级控制寄存器        —— SCR_EL3 / HCR_EL2 决定下一级是 AArch64 + Non-secure
 *   4) SPSR_ELx（目标级别+执行态）与 ELR_ELx（目标入口），然后 eret
 *
 * 注意：SP_EL1/SP_EL2 不能在本级用 msr 访问（其编码 op1=4 表示“访问更低 EL 的
 * banked 栈指针”，只允许在更高 EL 执行）。本级改栈统一用 mov sp；只有 EL0 的
 * 栈 SP_EL0 要从 EL1 用 msr sp_el0 提前设好。
 *
 * 本文件对复位 EL 做健壮分支：EL3/EL2/EL1 启动都能正确降到 EL0。
 */

.section .text.boot, "ax"
.global _start

_start:
    // x19 全程保存共享向量表地址（callee-saved，跨 bl 调用不丢失）
    ldr     x19, =vector_table

    // 读当前异常级别：CurrentEL 的 bits[3:2] = 级别（0=EL0,1=EL1,2=EL2,3=EL3）
    mrs     x0, currentel
    lsr     x0, x0, #2
    and     x0, x0, #3

    cmp     x0, #3
    b.eq    start_el3
    cmp     x0, #2
    b.eq    start_el2
    cmp     x0, #1
    b.eq    start_el1
    b       start_el0          // 复位即 EL0（非本场景，兜底）

/* ============ EL3（Secure 固件）============
 * 职责：设 SCR_EL3 使 EL2 为 AArch64 + Non-secure，ERET 到 EL2。
 */
start_el3:
    msr     vbar_el3, x19                    // EL3 向量表
    ldr     x2, =_stack_top_el3
    mov     sp, x2                          // EL3 异常栈（EL3 恒用 SP_EL3，即当前 sp）

    // SCR_EL3：
    //   bit0  NS  = 1 -> 下一级进入 Non-secure
    //   bit8  HCE = 1 -> 使能 HVC（EL1/EL0 可调用）
    //   bit10 RW  = 1 -> 下一级（EL2）为 AArch64
    // GICD_IGROUPR 等是 Secure-only 寄存器，必须在 Secure EL3 阶段配置，
    // 之后降级到 Non-secure 才能收到定时器中断（见 main.c 的 el3_setup）。
    mov     x2, #((1 << 0) | (1 << 8) | (1 << 10))
    msr     scr_el3, x2

    mov     x2, #0x3c9                      // SPSR: EL2h + AArch64 + DAIF 全屏蔽
    msr     spsr_el3, x2
    ldr     x2, =start_el2                  // ELR: 下一级入口
    msr     elr_el3, x2

    bl      startup_banner                  // 启动横幅 + 基本信息 + STEP 1 标题
    bl      el3_report                      // 打印本级别信息
    bl      el3_setup                       // Secure EL3 配置 GIC
    eret                                    // EL3 -> EL2

/* ============ EL2（Hypervisor）============
 * 职责：设 HCR_EL2 使 EL1 为 AArch64，ERET 到 EL1。
 */
start_el2:
    msr     vbar_el2, x19
    ldr     x2, =_stack_top_el2
    mov     sp, x2                          // EL2 异常栈（EL2h 下 sp 即 SP_EL2）

    // HCR_EL2：
    //   bit31 RW  = 1 -> 下一级（EL1）为 AArch64
    //   bit19 TSC = 0 -> SMC 不 trap 到 EL2（直通 EL3）
    //   bit29 HCD = 0 -> HVC 使能
    mov     x2, #(1 << 31)
    msr     hcr_el2, x2

    mov     x2, #0x3c5                      // SPSR: EL1h + AArch64 + DAIF 全屏蔽
    msr     spsr_el2, x2
    ldr     x2, =start_el1                  // ELR: 下一级入口
    msr     elr_el2, x2

    bl      el2_report
    eret                                    // EL2 -> EL1

/* ============ EL1（内核）============
 * 职责：设好 EL0 的栈与 SPSR，ERET 到 EL0。
 */
start_el1:
    msr     vbar_el1, x19
    ldr     x2, =_stack_top_el1
    mov     sp, x2                          // EL1 异常栈（EL1h 下 sp 即 SP_EL1）
    ldr     x2, =_stack_top_el0
    msr     sp_el0, x2                      // EL0 栈（SP_EL0 可从 EL1 访问）

    // CNTKCTL_EL1.EL0PTEN(bit9)=1：允许 EL0 访问 EL1 物理定时器
    // （否则 EL0 执行 msr cntp_ctl_el0/cntp_tval_el0 会 trap 到 EL1）
    mov     x2, #(1 << 9)
    msr     cntkctl_el1, x2

    mov     x2, #0x0                        // SPSR: EL0t + AArch64 + DAIF 不屏蔽
    msr     spsr_el1, x2
    ldr     x2, =main                       // ELR: EL0 入口
    msr     elr_el1, x2

    bl      el1_report
    eret                                    // EL1 -> EL0

/* ============ EL0（应用）============ */
start_el0:
    b       main

hang:
    wfe
    b       hang