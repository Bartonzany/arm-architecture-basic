/* main.c — 通用定时器验证：频率 / 计数 / CVAL vs TVAL / 中断
 *
 * 验证文档《通用定时器》的核心知识点：
 *   §2.1 CNTFRQ_EL0（频率）、CNTPCT_EL0（物理计数）、CNTVCT_EL0（虚拟计数）
 *   §2.4 CVAL vs TVAL（写 TVAL 内部填充 CVAL = 计数 + TVAL；读回 TVAL 递减）
 *   §2.5 定时器中断（ENABLE/IMASK，INTID 30，电平敏感）
 */

#define UART0_DR ((volatile unsigned int *)0x09000000)

/* GICv2 寄存器（QEMU virt,gic-version=2）*/
#define GICD_CTLR      ((volatile unsigned int *)0x08000000)
#define GICD_ISENABLER ((volatile unsigned int *)0x08000100)
#define GICC_CTLR      ((volatile unsigned int *)0x08010000)
#define GICC_PMR       ((volatile unsigned int *)0x08010004)
#define GICC_IAR       ((volatile unsigned int *)0x0801000C)
#define GICC_EOIR      ((volatile unsigned int *)0x08010010)

#define TIMER_IRQ_ID   30              /* EL1 物理定时器 PPI（SBSA 定义）*/

static volatile int g_irq_ok = 0;

/* ================= 日志框架 ================= */

static void uart_putc(char c) { *UART0_DR = (unsigned int)c; }
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }

static void uart_puthex(unsigned long v, int digits) {
    static const char hex[] = "0123456789abcdef";
    for (int i = digits - 1; i >= 0; --i)
        uart_putc(hex[(v >> (i * 4)) & 0xF]);
}

static void log_line(const char *p, const char *m) { uart_puts(p); uart_puts(m); uart_puts("\n"); }
#define log_info(m) log_line("[INFO] ", (m))
#define log_ok(m)   log_line("[ OK ] ", (m))

static void log_reg(const char *name, unsigned long val) {
    uart_puts("[REG ] "); uart_puts(name); uart_puts(" = 0x");
    uart_puthex(val, 16); uart_puts("\n");
}

static int exit_block[2];
static void semihosting_exit(void) {
    exit_block[0] = 0x20026; exit_block[1] = 0;
    __asm__ volatile("mov x1, %0\n\tmov x0, #0x18\n\thlt #0xF000"
                     : : "r"(exit_block) : "x0", "x1");
}

/* ================= 异常处理器（由 vector_table.S 调用）================= */

void irq_handler_c(void) {
    unsigned int iar = *GICC_IAR;
    unsigned int id  = iar & 0x3FF;
    uart_puts("[IRQ ] 定时器到期，GICC_IAR=0x");
    uart_puthex(iar, 3);
    uart_puts(" INTID=");
    uart_puthex(id, 2);
    uart_puts("（EL1 物理定时器）\n");
    *GICC_EOIR = iar;
    /* 清除中断：禁用定时器（ENABLE=0）。电平敏感，否则会反复触发 */
    __asm__ volatile("msr cntp_ctl_el0, xzr");
    g_irq_ok = 1;
}

void svc_handler_c(unsigned long esr, unsigned long elr, unsigned long spsr) { (void)esr; (void)elr; (void)spsr; }
void fiq_handler_c(void)    {}
void serror_handler_c(void) {}
void unexpected_handler_c(void) {}

/* ================= GIC 初始化 ================= */

static void gic_init(void) {
    *GICD_CTLR      = 1;
    *GICD_ISENABLER = (1U << TIMER_IRQ_ID);
    *GICC_CTLR      = 1;
    *GICC_PMR       = 0xFF;
}

/* ================= 主流程 ================= */

void main(void) {
    unsigned long freq, cnt1, cnt2, vct, cval, tval;

    log_line("[====]", "AArch64 通用定时器裸机验证");

    /* §2.1 频率与计数 */
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    log_reg("CNTFRQ_EL0", freq);        /* 系统计数频率 */

    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(cnt1));
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(cnt2));
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(vct));
    log_reg("CNTPCT_EL0", cnt2);
    log_reg("CNTVCT_EL0", vct);         /* 无 EL2 时 = 物理计数 */
    uart_puts("[INFO] 两次 CNTPCT_EL0 差值 = ");
    uart_puthex(cnt2 - cnt1, 8);
    uart_puts("（计数在递增）\n");

    /* §2.4 CVAL vs TVAL：写 TVAL，内部填充 CVAL = 计数 + TVAL */
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(cnt1));
    __asm__ volatile("msr cntp_tval_el0, %0" : : "r"(100000));   /* X tick 后触发 */
    __asm__ volatile("mrs %0, cntp_cval_el0" : "=r"(cval));       /* 读回 CVAL */
    __asm__ volatile("mrs %0, cntp_tval_el0" : "=r"(tval));       /* 读回 TVAL */
    log_info("写 CNTP_TVAL_EL0 = 100000");
    log_reg("CNTP_CVAL_EL0", cval);      /* 应 ≈ cnt1 + 100000 */
    log_reg("CNTP_TVAL_EL0", tval);      /* 读回递减值 */
    uart_puts("[INFO] CVAL - 原计数 = ");
    uart_puthex(cval - cnt1, 8);
    uart_puts("（≈ 100000，因执行期间计数仍在走）\n");

    /* §2.5 生成中断：ENABLE=1 + IMASK=0，INTID 30 */
    gic_init();
    __asm__ volatile("msr cntp_tval_el0, %0" : : "r"(0x8000));    /* 较短倒计时 */
    __asm__ volatile("msr cntp_ctl_el0, %0" : : "r"(1));          /* ENABLE=1, IMASK=0 */
    __asm__ volatile("msr daifclr, #2");                          /* 解除 IRQ 屏蔽 */
    log_info("定时器已使能（ENABLE=1, IMASK=0），等待 IRQ ...");
    while (!g_irq_ok) { /* 忙循环，被定时器 IRQ 打断 */ }
    log_ok("定时器中断验证完成（INTID 30）");

    log_line("[EXIT]", "semihosting 正常退出");
    semihosting_exit();
    while (1) { __asm__ volatile("wfe"); }
}