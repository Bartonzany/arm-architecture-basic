/* main.c — 裸机主程序：AArch64 内存系统、排序与屏障验证
 *
 * 围绕《AArch64 内存系统、排序与屏障》的可验证知识点，在 QEMU virt 的
 * EL1 裸机环境逐一演示（单核）：
 *
 *   [A] LDAR/STLR 对齐访问正常（§3 对齐规则）
 *   [B] 非对齐 LDAR   → 对齐故障（§3：LDAR 必须对齐到元素大小）
 *   [C] 非对齐 LDAXP  → 对齐故障（§3：独占对必须对齐到 2×元素大小）
 *   [D] 非对齐 STLR   → 对齐故障
 *   [E] 上下文同步：CPACR_EL1.FPEN + ISB 控制 FP 指令捕获（§4）
 *
 * 跨观察者（多核）的排序语义（§1、§2.1、§3 消息传递）在 QEMU TCG 单核
 * 下无法复现重排序，故在 docs/ordering-barriers-notes.md 里结合汇编说明，
 * 本演示聚焦可确定性验证的部分：屏障指令的**对齐要求**与 **ISB 上下文同步**。
 */

/* PL011 UART（QEMU virt 默认串口）数据寄存器 */
#define UART0_DR ((volatile unsigned int *)0x09000000)

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

/* ================= 测试状态机 ================= */
enum {
    T_NONE = 0,
    T_ALIGN,   /* 预期对齐故障（FSC=0x21）*/
    T_FP,      /* 预期 FP/SIMD 捕获（EC=0x07）*/
};

static volatile int g_test = T_NONE;
static volatile int g_fault_ok = 0;

/* LDAR/STLR 需要一个 8 字节对齐的地址 */
static volatile unsigned long g_shared = 0;
/* LDAXP 需要一个 16 字节对齐的缓冲区 */
static volatile unsigned long g_pair[2] __attribute__((aligned(16)));

/* ================= 同步异常处理器（C 部分）================= */

__attribute__((noinline)) static void fault_hang(void) {
    for (;;) __asm__ volatile("wfe");
}

unsigned long sync_handler_c(unsigned long esr, unsigned long elr, unsigned long far) {
    unsigned long ec  = (esr >> 26) & 0x3F;
    unsigned long fsc = esr & 0x3F;
    unsigned long ret = elr + 4;

    uart_puts("[SYNC] EC=0x"); uart_puthex(ec, 2);
    uart_puts("  FSC=0x");      uart_puthex(fsc, 2);
    uart_puts("\n");
    log_reg("  ELR_EL1", elr);
    log_reg("  FAR_EL1", far);

    switch (g_test) {
    case T_ALIGN:
        g_fault_ok = 1; g_test = T_NONE;
        break;
    case T_FP:
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

static void test_a_aligned_acquire_release(void) {
    unsigned long v = 0;
    log_line("[ A ]", "LDAR/STLR 对齐访问（§3 对齐规则）");

    g_shared = 0;
    __asm__ volatile("stlr %0, [%1]" : : "r"(1UL), "r"((unsigned long)&g_shared));
    __asm__ volatile("ldar %0, [%1]" : "=r"(v)  : "r"((unsigned long)&g_shared));
    if (v == 1) log_ok("对齐的 STLR/LDAR 正常完成（无故障）");
    else        log_fail("对齐的 LDAR 返回值错误");
}

static void test_b_unaligned_ldar(void) {
    log_line("[ B ]", "非对齐 LDAR → 对齐故障（LDAR 必须对齐到元素大小）");
    g_test = T_ALIGN; g_fault_ok = 0;
    unsigned long v;
    __asm__ volatile("ldar %0, [%1]" : "=r"(v) : "r"((unsigned long)&g_shared + 1));
    if (g_fault_ok) log_ok("非对齐 LDAR 触发对齐故障（FSC=0x21）");
    else            log_fail("非对齐 LDAR 未触发故障");
}

static void test_c_unaligned_ldaxp(void) {
    log_line("[ C ]", "非对齐 LDAXP → 对齐故障（独占对必须对齐到 2×元素大小）");
    g_test = T_ALIGN; g_fault_ok = 0;
    unsigned long a, b;
    __asm__ volatile("ldaxp %0, %1, [%2]" : "=r"(a), "=r"(b) : "r"((unsigned long)&g_pair[1]));
    if (g_fault_ok) log_ok("非对齐 LDAXP（8B 对齐非 16B）触发对齐故障");
    else            log_fail("非对齐 LDAXP 未触发故障");
}

static void test_d_unaligned_stlr(void) {
    log_line("[ D ]", "非对齐 STLR → 对齐故障");
    g_test = T_ALIGN; g_fault_ok = 0;
    __asm__ volatile("stlr %0, [%1]" : : "r"(1UL), "r"((unsigned long)&g_shared + 1));
    if (g_fault_ok) log_ok("非对齐 STLR 触发对齐故障（FSC=0x21）");
    else            log_fail("非对齐 STLR 未触发故障");
}

static void test_e_context_sync_isb(void) {
    unsigned long cpacr;
    log_line("[ E ]", "上下文同步：CPACR_EL1.FPEN + ISB（§4）");

    __asm__ volatile("mrs %0, cpacr_el1" : "=r"(cpacr));
    log_reg("CPACR_EL1(初始)", cpacr);

    /* 1. QEMU 复位后 FPEN=0（FP 禁用），FADD 触发 FP/SIMD 捕获 */
    g_test = T_FP; g_fault_ok = 0;
    __asm__ volatile("fadd s0, s1, s2");
    if (g_fault_ok) log_ok("FPEN=0：FADD 触发 FP/SIMD 捕获（EC=0x07）");
    else            log_fail("FPEN=0：FADD 未捕获");

    /* 2. 写 CPACR_EL1.FPEN=3 使能 FP，经 ISB 同步上下文后 FADD 生效 */
    __asm__ volatile("msr cpacr_el1, %0" : : "r"(3UL << 20));
    __asm__ volatile("isb");
    __asm__ volatile("fadd s0, s1, s2");
    log_ok("写 CPACR + ISB 后，FADD 恢复正常执行");
    log_info("§4：写系统寄存器后需 ISB 同步；无 ISB 时后续指令可能仍看到旧上下文");
}

/* semihosting SYS_EXIT（0x18）—— v2 协议：x1=参数块指针 */
static int exit_block[2];
static void semihosting_exit(void) {
    exit_block[0] = 0x20026;
    exit_block[1] = 0;
    __asm__ volatile(
        "mov x1, %0\n\t"
        "mov x0, #0x18\n\t"
        "hlt #0xF000"
        : : "r"(exit_block) : "x0", "x1");
}

/* ================= 主流程 ================= */

void main(void) {
    unsigned long el;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(el));
    el = (el >> 2) & 3;

    log_line("[====]", "AArch64 内存排序与屏障 裸机验证");
    log_reg("CurrentEL", el);

    test_a_aligned_acquire_release();
    test_b_unaligned_ldar();
    test_c_unaligned_ldaxp();
    test_d_unaligned_stlr();
    test_e_context_sync_isb();

    log_line("[====]", "验证结束");
    semihosting_exit();
    for (;;) __asm__ volatile("wfe");
}