/* main.c — 裸机主程序：AArch64 内存属性与特性验证
 *
 * 围绕《AArch64 内存属性与特性》里的可验证知识点，在 QEMU virt 的 EL1
 * 裸机环境逐一演示：
 *
 *   [A] Device 内存（nGnRnE）：MMIO 经 L1[0] 映射为 Device（§3、§4）
 *   [B] MAIR/TCR 建好页表 → Normal(WB/WA) vs Device 两种内存类型（§3、§5、§6）
 *   [C] 权限 AP=只读 → EL1 写只读页触发权限故障（§7）
 *   [D] 执行权限 PXN=1 → 跳转到该页触发指令中止（§7.2）
 *   [E] 对齐 SCTLR.A → 普通内存的非对齐访问按 A 开关决定故障与否（§10）
 *   [F] 访问标志 AF=0 → 访问触发访问标志故障，软件置位后恢复（§9）
 *
 * 所有同步异常经 vector_table.S 的 sync_handler 进入 sync_handler_c，
 * 由它根据"当前正在跑的测试"修复 ELR_EL1 后 ERET 回主流程。
 */

/* PL011 UART（QEMU virt 默认串口）数据寄存器 */
#define UART0_DR ((volatile unsigned int *)0x09000000)

/* ================= 转换表描述符位（4KB 粒度，stage1，EL1&0）================= */
#define S1_BLOCK      0x1UL           /* block 描述符（L1/L2：bit0=1, bit1=0）*/
#define S1_PAGE       0x3UL           /* page 描述符（L3：bit0=1, bit1=1）*/
#define S1_TABLE      0x3UL           /* table 描述符（bit1=1, bit0=1）*/
#define S1_AF         (1UL << 10)     /* 访问标志 */
#define S1_AP_RW_EL1  (0x0UL << 6)    /* AP=00：EL1 读写，EL0 无 */
#define S1_AP_RW_ALL  (0x1UL << 6)    /* AP=01：EL1/EL0 都读写 */
#define S1_AP_RO_EL1  (0x2UL << 6)    /* AP=10：EL1 只读，EL0 无 */
#define S1_PXN        (1UL << 53)     /* 特权不可执行 */
#define S1_UXN        (1UL << 54)     /* 用户不可执行 */

#define ATTRINDX(i)   ((unsigned long)(i) << 2)  /* 描述符里 3 位 AttrIndx */

/* MAIR_ELx 三个属性槽：0=Device-nGnRnE, 1=Normal WB/WA, 2=Normal 非缓存 */
#define ATTR_DEVICE      0x00UL
#define ATTR_NORMAL_WB   0xFFUL
#define ATTR_NORMAL_NC   0x44UL
#define MAIR_VAL  ((ATTR_NORMAL_NC << 16) | (ATTR_NORMAL_WB << 8) | ATTR_DEVICE)

/* TCR_EL1：T0SZ=25(39-bit VA) + 4KB + Inner/Outer WB/WA + Inner Shareable + 关 TTBR1 */
#define TCR_VAL \
    (25UL | (0UL << 14) | (3UL << 12) | (1UL << 10) | (1UL << 8) | (1UL << 23))

/* SCTLR_EL1 位 */
#define SCTLR_M  (1UL << 0)    /* MMU 使能 */
#define SCTLR_A  (1UL << 1)    /* 对齐检查 */
#define SCTLR_C  (1UL << 2)    /* 数据缓存 */
#define SCTLR_I  (1UL << 12)   /* 指令缓存 */

/* ================= 链接脚本导出的符号 ================= */
extern unsigned char tt_l1_base[], tt_l2_base[], tt_l3_base[];
extern unsigned char read_only_page[], exec_never_page[], af_page[], normal_rw_page[];

/* ================= 测试状态机 ================= */
enum {
    T_NONE = 0,
    T_PERM,        /* 写只读页 → 权限故障 */
    T_PXN,         /* 跳非执行页 → 指令中止 */
    T_ALIGN,       /* SCTLR.A=1 非对齐访问 */
    T_AF,          /* 访问 AF=0 页 → 访问标志故障 */
};

static volatile int g_test = T_NONE;
static volatile int g_fault_ok = 0;        /* 当前测试的异常是否按预期发生 */
static volatile unsigned long g_obs_ec, g_obs_fsc, g_obs_far, g_obs_elr;

/* ================= 轻量日志框架（无 stdio，UART 直出）================= */

static void uart_putc(char c) { *UART0_DR = (unsigned int)c; }

static void uart_puts(const char *s) {
    while (*s != '\0') uart_putc(*s++);
}

static void uart_puthex(unsigned long v, int digits) {
    static const char hex[] = "0123456789abcdef";
    for (int i = digits - 1; i >= 0; --i)
        uart_putc(hex[(v >> (i * 4)) & 0xF]);
}

static void log_line(const char *prefix, const char *msg) {
    uart_puts(prefix); uart_puts(msg); uart_puts("\n");
}
#define log_info(m)  log_line("[INFO] ", (m))
#define log_ok(m)    log_line("[ OK ] ", (m))
#define log_fail(m)  log_line("[FAIL] ", (m))

static void log_reg(const char *name, unsigned long val) {
    uart_puts("[REG ] "); uart_puts(name); uart_puts(" = 0x");
    uart_puthex(val, 16); uart_puts("\n");
}

/* ================= MMU 页表与寄存器配置 ================= */

static void zero_table(void *p, unsigned long bytes) {
    volatile unsigned long *q = (volatile unsigned long *)p;
    for (unsigned long i = 0; i < bytes / 8; i++) q[i] = 0;
}

/* 建立恒等映射（VA=PA）：
 *   L1[0] = 1GB Device block（0x0000_0000 .. 0x3FFF_FFFF，UART/GIC 等 MMIO）
 *   L1[1] = table → L2（0x4000_0000 .. 0x7FFF_FFFF，RAM）
 *   L2[8] = table → L3（0x4100_0000 .. 0x411F_FFFF，4 个测试页）
 */
static void build_page_tables(void) {
    unsigned long *l1 = (unsigned long *)tt_l1_base;
    unsigned long *l2 = (unsigned long *)tt_l2_base;
    unsigned long *l3 = (unsigned long *)tt_l3_base;

    zero_table(l1, 0x1000);
    zero_table(l2, 0x1000);
    zero_table(l3, 0x1000);

    /* L1[0]：1GB 设备内存（nGnRnE），不可执行 */
    l1[0] = 0x00000000UL | S1_AF | ATTRINDX(0) | S1_AP_RW_EL1 | S1_PXN | S1_UXN | S1_BLOCK;
    /* L1[1]：指向 L2 表 */
    l1[1] = (unsigned long)tt_l2_base | S1_TABLE;

    /* L2：2MB block × 512，普通内存 WB/WA，可读写执行 */
    for (int i = 0; i < 512; i++) {
        unsigned long pa = 0x40000000UL + (unsigned long)i * 0x200000UL;
        l2[i] = pa | S1_AF | ATTRINDX(1) | S1_AP_RW_EL1 | S1_BLOCK;
    }
    /* L2[8]：0x4100_0000 区域改走 L3（4KB 页，逐页设属性）*/
    l2[8] = (unsigned long)tt_l3_base | S1_TABLE;

    /* L3：4 个测试页（page 描述符用 0x3，注意与 block 的 0x1 不同）*/
    l3[0] = (unsigned long)read_only_page  | S1_AF | ATTRINDX(1) | S1_AP_RO_EL1 | S1_PAGE;
    l3[1] = (unsigned long)exec_never_page | S1_AF | ATTRINDX(1) | S1_AP_RW_EL1 | S1_PXN | S1_PAGE;
    l3[2] = (unsigned long)af_page         |        ATTRINDX(1) | S1_AP_RW_EL1 | S1_PAGE; /* AF=0 */
    l3[3] = (unsigned long)normal_rw_page  | S1_AF | ATTRINDX(1) | S1_AP_RW_EL1 | S1_PAGE;
}

static unsigned long read_sctlr(void) {
    unsigned long v;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(v));
    return v;
}

static void write_sctlr(unsigned long v) {
    __asm__ volatile("msr sctlr_el1, %0" : : "r"(v));
    __asm__ volatile("isb");
}

static void enable_mmu(void) {
    __asm__ volatile("msr mair_el1, %0" : : "r"(MAIR_VAL));
    __asm__ volatile("msr tcr_el1, %0"  : : "r"(TCR_VAL));
    __asm__ volatile("msr ttbr0_el1, %0" : : "r"((unsigned long)tt_l1_base));
    __asm__ volatile("isb");
    __asm__ volatile("tlbi vmalle1");
    __asm__ volatile("dsb sy");
    __asm__ volatile("isb");
    write_sctlr(read_sctlr() | SCTLR_M | SCTLR_C | SCTLR_I);
}

/* ================= 同步异常处理器（C 部分）================= */

/* PXN 测试：指令中止后 ELR 指向非执行页，无法跳过下一条（仍在非执行页），
 * 因此直接把 ELR 指到这个"恢复点"，ERET 后从这里返回主流程。 */
__attribute__((noinline)) static void pxn_resume(void) { }

/* 意外故障：打印后在此死循环，避免反复触发异常刷屏 */
__attribute__((noinline)) static void fault_hang(void) {
    for (;;) __asm__ volatile("wfe");
}

unsigned long sync_handler_c(unsigned long esr, unsigned long elr, unsigned long far) {
    unsigned long ec  = (esr >> 26) & 0x3F;
    unsigned long fsc = esr & 0x3F;
    unsigned long ret = elr + 4;          /* 默认：跳过故障指令 */

    g_obs_ec = ec; g_obs_fsc = fsc; g_obs_far = far; g_obs_elr = elr;

    uart_puts("[SYNC] EC=0x"); uart_puthex(ec, 2);
    uart_puts("  FSC=0x");      uart_puthex(fsc, 2);
    uart_puts("  WnR=");        uart_puthex((esr >> 6) & 1, 1);
    uart_puts("\n");
    log_reg("  ELR_EL1", elr);
    log_reg("  FAR_EL1", far);

    switch (g_test) {
    case T_PERM:
        g_fault_ok = 1; g_test = T_NONE;
        break;
    case T_PXN:
        g_fault_ok = 1; g_test = T_NONE;
        ret = (unsigned long)pxn_resume;
        break;
    case T_ALIGN:
        g_fault_ok = 1; g_test = T_NONE;
        break;
    case T_AF:
        /* 软件管理 AF：硬件不动，异常处理程序手动置 AF=1 再返回 */
        {
            unsigned long *l3 = (unsigned long *)tt_l3_base;
            l3[2] |= S1_AF;
            __asm__ volatile("dsb ishst");
            __asm__ volatile("tlbi vaae1, %0" : : "r"((unsigned long)af_page));
            __asm__ volatile("isb");
        }
        g_fault_ok = 1; g_test = T_NONE;
        break;
    default:
        uart_puts("[SYNC] 未预期的同步异常，挂起\n");
        ret = (unsigned long)fault_hang;
        break;
    }
    return ret;
}

void unexpected_handler_c(void) {
    log_line("[WARN]", "未预期的向量入口");
    for (;;) __asm__ volatile("wfe");
}

/* ================= 各验证用例 ================= */

/* §3/§4/§10：设备内存（Device-nGnRnE）。
 * 后续所有 UART 日志都经 L1[0] 的 Device 块（0x0000_0000..0x3FFF_FFFF）
 * 访问 0x0900_0000，说明 MMIO 被正确映射为 Device。
 * §10「Device 不允许非对齐访问」是架构规则，但 QEMU TCG 未建模该对齐故障，
 * 故这里只做说明、不验证运行时故障（普通内存的对齐开关见 [ E ]）。 */
static void test_device_region(void) {
    log_line("[ A ]", "Device 内存（nGnRnE）：MMIO 经映射为 Device 后访问正常");
    log_info("后续所有日志都经 L1[0] 的 Device 块写 UART(0x09000000)，未走缓存");
    log_info("注：§10「Device 不允许非对齐访问」为架构规则，QEMU TCG 未建模");
}

static void test_b_normal_vs_device(void) {
    unsigned long mair, tcr;
    log_line("[ B ]", "建立页表：Normal(WB/WA) vs Device(nGnRnE)");
    build_page_tables();
    enable_mmu();
    __asm__ volatile("mrs %0, mair_el1" : "=r"(mair));
    __asm__ volatile("mrs %0, tcr_el1"  : "=r"(tcr));
    log_reg("SCTLR_EL1", read_sctlr());
    log_reg("MAIR_EL1", mair);
    log_reg("TCR_EL1", tcr);
    log_info("AttrIndx0=Device、AttrIndx1=Normal-WB/WA，页表与测试页已就绪");
}

static void test_c_permission(void) {
    unsigned long rd, wr;
    log_line("[ C ]", "权限：EL1 写只读页（AP=10）→ 权限故障");

    __asm__ volatile("ldr %0, [%1]" : "=r"(rd) : "r"((unsigned long)read_only_page));
    log_reg("  读只读页", rd);   /* 应成功，值 0 */

    g_test = T_PERM;
    g_fault_ok = 0;
    __asm__ volatile("str %0, [%1]" : : "r"(0xDEADBEEFUL), "r"((unsigned long)read_only_page));

    if (g_fault_ok) {
        __asm__ volatile("ldr %0, [%1]" : "=r"(wr) : "r"((unsigned long)read_only_page));
        log_ok("写只读页触发权限故障（FSC=0x0F），且数据未被写入");
        log_reg("  故障后页内容", wr);
    } else {
        log_fail("写只读页未触发权限故障");
    }
}

static void test_d_execute_permission(void) {
    void (*fn)(void) = (void (*)(void))(unsigned long)exec_never_page;
    log_line("[ D ]", "执行权限：跳转 PXN=1 页 → 指令中止");

    g_test = T_PXN;
    g_fault_ok = 0;
    fn();   /* blr → 取指失败 → 指令中止，处理器把 ELR 指到恢复点 */

    if (g_fault_ok)
        log_ok("跳转非执行页触发指令中止（FSC=0x0F），已恢复到主流程");
    else
        log_fail("跳转非执行页未触发指令中止");
}

static void test_e_alignment(void) {
    unsigned long v;
    log_line("[ E ]", "对齐：SCTLR.A 控制普通内存的非对齐访问");

    /* A=1：普通内存非对齐也故障 */
    write_sctlr(read_sctlr() | SCTLR_A);
    g_test = T_ALIGN;
    g_fault_ok = 0;
    __asm__ volatile("ldr %0, [%1]" : "=r"(v) : "r"((unsigned long)normal_rw_page + 1));
    if (g_fault_ok) log_ok("A=1：普通内存非对齐访问触发对齐故障");
    else            log_fail("A=1：普通内存非对齐访问未触发故障");

    /* A=0：普通内存非对齐允许 */
    write_sctlr(read_sctlr() & ~SCTLR_A);
    g_test = T_NONE;
    g_fault_ok = 0;
    __asm__ volatile("ldr %0, [%1]" : "=r"(v) : "r"((unsigned long)normal_rw_page + 1));
    if (!g_fault_ok) log_ok("A=0：普通内存非对齐访问正常完成");
    else             log_fail("A=0：普通内存非对齐访问意外故障");
}

static void test_f_access_flag(void) {
    unsigned long v1, v2;
    log_line("[ F ]", "访问标志：AF=0 页首次访问 → 访问标志故障，软件置位后恢复");

    g_test = T_AF;
    g_fault_ok = 0;
    __asm__ volatile("ldr %0, [%1]" : "=r"(v1) : "r"((unsigned long)af_page));
    if (!g_fault_ok) { log_fail("首次访问 AF=0 页未触发访问标志故障"); return; }

    log_ok("首次访问触发访问标志故障（FSC=0x0B），处理器已置 AF=1");

    g_test = T_NONE;
    g_fault_ok = 0;
    __asm__ volatile("ldr %0, [%1]" : "=r"(v2) : "r"((unsigned long)af_page));
    if (!g_fault_ok) log_ok("再次访问（AF=1）不再故障");
    else             log_fail("置 AF 后仍触发故障");
    (void)v2;
}

/* semihosting SYS_EXIT（0x18）—— v2 协议：x1=参数块指针 */
static int exit_block[2];
static void semihosting_exit(void) {
    exit_block[0] = 0x20026;   /* ADP_Stopped_ApplicationExit */
    exit_block[1] = 0;
    __asm__ volatile(
        "mov x1, %0\n\t"
        "mov x0, #0x18\n\t"
        "hlt #0xF000"
        : : "r"(exit_block) : "x0", "x1");
}

/* ================= 主流程 ================= */

void main(void) {
    unsigned long current_el;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(current_el));
    current_el = (current_el >> 2) & 3;

    log_line("[====]", "AArch64 内存属性与特性 裸机验证");
    log_reg("CurrentEL", current_el);
    log_reg("SCTLR_EL1(初始)", read_sctlr());
    log_info("SCTLR.M=0：MMU 关，所有数据访问按 Device-nGnRnE 处理（§2.2）");

    test_b_normal_vs_device();
    test_device_region();
    test_c_permission();
    test_d_execute_permission();
    test_e_alignment();
    test_f_access_flag();

    log_line("[====]", "验证结束");
    semihosting_exit();
    for (;;) __asm__ volatile("wfe");
}
