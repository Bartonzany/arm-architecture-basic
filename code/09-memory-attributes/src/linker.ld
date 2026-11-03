/* linker.ld — MMU 内存属性验证示例链接脚本
 *
 * QEMU -machine virt：RAM 从 0x40000000 开始，入口即该地址。
 *
 * 布局要点：
 *   - 内核代码/数据/栈放在首个 2MB（0x4000_0000 .. 0x401F_FFFF，L2 block 映射）
 *   - 三级页表（L1/L2/L3）各 4KB，紧随栈之后、4KB 对齐
 *   - 测试页固定放在 0x4100_0000（2MB 对齐），由 L3 页表逐个 4KB 映射，
 *     以便给每个测试页挂不同的 AP/PXN/AF 属性
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

    /* ---- 三级转换表，各 4KB，4KB 对齐 ---- */
    . = ALIGN(0x1000);
    tt_l1_base = .; . += 0x1000;
    tt_l2_base = .; . += 0x1000;
    tt_l3_base = .; . += 0x1000;

    /* ---- 测试页：固定 0x4100_0000 起，每页 4KB ---- */
    . = 0x41000000;
    read_only_page  = .; . += 0x1000;   /* 0x4100_0000  AP=只读   */
    exec_never_page = .; . += 0x1000;   /* 0x4100_1000  PXN=1     */
    af_page         = .; . += 0x1000;   /* 0x4100_2000  AF=0      */
    normal_rw_page  = .; . += 0x1000;   /* 0x4100_3000  正常读写   */
    _test_end       = .;
}
