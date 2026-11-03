/* linker.ld — 内存排序与屏障验证裸机示例链接脚本
 *
 * QEMU -machine virt：RAM 从 0x40000000 开始，入口即该地址。
 */
ENTRY(_start)

SECTIONS
{
    . = 0x40000000;

    .text : {
        KEEP(*(.text.boot))      /* 启动代码放最前 */
        KEEP(*(.text.vector))    /* 向量表紧跟其后 */
        *(.text*)
    }

    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }
    .bss    : { *(.bss*) *(COMMON) }

    . = ALIGN(16);
    _stack_top = . + 0x10000;    /* 64KB 栈，栈顶（高地址）*/
}