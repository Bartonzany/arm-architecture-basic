/* linker.ld — EL2 裸机虚拟化示例链接脚本（QEMU virt）*/
ENTRY(_start)

SECTIONS
{
    . = 0x40000000;

    .text : {
        *(.text.boot)      /* EL2 启动代码 */
        *(.text.vector)    /* EL2 向量表 */
        *(.text*)
    }

    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }
    .bss    : { *(.bss*) }

    . = ALIGN(16);
    _stack_top = . + 0x10000;   /* 64KB 栈，EL1/EL2 共用栈底 */
}