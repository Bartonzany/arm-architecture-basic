/* boot.S — 裸机启动代码（AArch64, EL1）
 *
 * 复位后从 EL1 开始执行（QEMU -machine virt 默认进入 EL1）：
 *   1. 设置 SP_EL1 栈指针
 *   2. 设置 VBAR_EL1 指向异常向量表
 *   3. 跳转 C 入口 main
 */

.section .text.boot, "ax"
.global _start

_start:
    // 设置栈指针（栈向低地址增长，SP 指向栈顶）
    ldr     x0, =_stack_top
    mov     sp, x0

    // 设置向量基址寄存器 VBAR_EL1 = vector_table
    ldr     x0, =vector_table
    msr     vbar_el1, x0

    // 进入 C 代码
    bl      main

// 主程序返回后死循环（本例 main 不会返回）
hang:
    wfe
    b       hang
