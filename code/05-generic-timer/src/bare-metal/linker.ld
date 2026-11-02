/* linker.ld — QEMU -machine virt 裸机链接脚本
 *
 * QEMU virt 机器 RAM 从 0x40000000 开始，入口即该地址。
 */
ENTRY(_start)

SECTIONS
{
    . = 0x40000000;

    .text : {
        *(.text.boot)      /* 启动代码放最前 */
        *(.text.vector)    /* 向量表紧跟其后 */
        *(.text*)
    }

    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }
    .bss    : { *(.bss*) }

    . = ALIGN(16);
    /* 64KB 栈，_stack_top 指向栈顶（高地址）*/
    _stack_top = . + 0x10000;
}
