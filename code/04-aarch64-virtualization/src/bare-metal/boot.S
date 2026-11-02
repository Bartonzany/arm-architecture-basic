/* boot.S — EL2 裸机启动：配置虚拟化 + 切入 EL1(guest)
 *
 * QEMU -machine virt,virtualization=on 使 PE 从 EL2 启动（hypervisor 层）。
 * 启动流程：
 *   1. 设 SP_EL2 / VBAR_EL2（hypervisor 自己的栈和向量表）
 *   2. 配 HCR_EL2：RW=1(EL1 用 AArch64)、TWI=1(trap WFI)
 *   3. 设 SP_EL1 / SPSR_EL2 / ELR_EL2，ERET 切入 EL1 运行 guest
 */

.section .text.boot, "ax"
.global _start

_start:
    // 1. 设置 hypervisor 栈（SP_EL2）
    //    当前 EL 的 SP 不能用 msr sp_el2 直接写（UNDEFINED），
    //    须 msr spsel 选中 SP_EL2 再用 mov sp。
    ldr     x0, =_stack_top
    msr     spsel, #1          // SPSel=1 -> 使用 SP_EL2
    mov     sp, x0             // 写 SP_EL2

    // 2. 设置 EL2 向量表（陷阱落到这里）
    ldr     x0, =vector_table_el2
    msr     vbar_el2, x0

    // 3. 配置 HCR_EL2
    //    bit31 RW  = 1：低 EL（EL0/1）使用 AArch64
    //    bit13 TWI = 1：EL0/EL1 执行 WFI 时陷阱到 EL2
    ldr     x0, =0x80002000
    msr     hcr_el2, x0

    // 4. 设置 guest 栈（SP_EL1）
    ldr     x1, =_stack_top
    msr     sp_el1, x1

    // 5. SPSR_EL2 = EL1h（EL1 + SP_EL1），且屏蔽 DAIF
    mov     x2, #0x3c5
    msr     spsr_el2, x2

    // 6. ELR_EL2 = guest 入口，ERET 即"进入 guest"
    ldr     x3, =guest_entry
    msr     elr_el2, x3

    // 7. ERET：切换到 EL1，执行 guest_entry
    eret

// hypervisor 不会回到这里（guest 末尾用 semihosting 退出）
hang:
    wfe
    b       hang