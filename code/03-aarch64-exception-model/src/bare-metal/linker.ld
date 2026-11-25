/* linker.ld — QEMU -machine virt 裸机链接脚本
 *
 * QEMU virt 机器 RAM 从 0x40000000 开始，复位入口即该地址。
 * 为 EL0/EL1/EL2/EL3 各分配一个 4KB 栈（依次递增，互不重叠）。
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
    . += 0x1000; _stack_top_el3 = .;
    . += 0x1000; _stack_top_el2 = .;
    . += 0x1000; _stack_top_el1 = .;
    . += 0x1000; _stack_top_el0 = .;
}