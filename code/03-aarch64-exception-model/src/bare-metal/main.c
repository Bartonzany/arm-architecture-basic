/* main.c — 裸机主程序：向量表 + SVC/IRQ 异常处理器（工程化日志）
 *
 * 运行流程：
 *   _start -> 设栈 + VBAR_EL1 -> main
 *   main 初始化（读 SCTLR/VBAR、配 GIC、配定时器、使能 IRQ）
 *       -> 触发 SVC(同步) -> 等待定时器 IRQ(异步)
 *   SVC  -> 向量表 +0x200 的 sync 处理器 -> 打印 EC/ELR/SPSR -> ERET
 *   定时器到期 -> GIC 断言 IRQ -> 向量表 +0x280 的 IRQ 处理器 -> 打印 IAR/ID -> EOI -> ERET
 *   FIQ/SError 处理器已实现但本示例不触发（触发条件见 docs/exception-notes.md）。
 */

// PL011 UART（QEMU virt 默认串口）数据寄存器
#define UART0_DR ((volatile unsigned int *)0x09000000)

// GICv2 寄存器（QEMU virt,gic-version=2）
#define GICD_CTLR      ((volatile unsigned int *)0x08000000)
#define GICD_ISENABLER ((volatile unsigned int *)0x08000100)
#define GICC_CTLR      ((volatile unsigned int *)0x08010000)
#define GICC_PMR       ((volatile unsigned int *)0x08010004)
#define GICC_IAR       ((volatile unsigned int *)0x0801000C)
#define GICC_EOIR      ((volatile unsigned int *)0x08010010)

#define TIMER_IRQ_ID   30              /* EL1 物理定时器 PPI 中断号 */

/* 验证状态（类似用户态 demo 的 g_checks/g_failures）
 * volatile：被异常处理器（异步上下文）修改、被 main 读取 */
static volatile int g_sync_ok = 0;   /* SVC 同步异常是否处理成功 */
static volatile int g_irq_ok  = 0;   /* IRQ 异步异常是否处理成功 */

/* ================= 轻量日志框架（无 stdio，UART 直出）================= */

static void uart_putc(char c) { *UART0_DR = (unsigned int)c; }

static void uart_puts(const char *s) {
    while (*s != '\0')
        uart_putc(*s++);
}

static void uart_puthex(unsigned long v, int digits) {
    static const char hex[] = "0123456789abcdef";
    for (int i = digits - 1; i >= 0; --i)
        uart_putc(hex[(v >> (i * 4)) & 0xF]);
}

/* 一行日志：[prefix] message */
static void log_line(const char *prefix, const char *msg) {
    uart_puts(prefix);
    uart_puts(msg);
    uart_puts("\n");
}

#define log_info(m)   log_line("[INFO] ", (m))
#define log_ok(m)     log_line("[ OK ] ", (m))
#define log_fail(m)   log_line("[FAIL] ", (m))

/* 打印寄存器： NAME = 0x... （16 位十六进制对齐） */
static void log_reg(const char *name, unsigned long val) {
    uart_puts("[REG ] ");
    uart_puts(name);
    uart_puts(" = 0x");
    uart_puthex(val, 16);
    uart_puts("\n");
}

/* ESR_EL1.EC 字段 → 人类可读的异常类型名 */
static const char *ec_name(unsigned int ec) {
    switch (ec) {
        case 0x00: return "Unknown reason";
        case 0x01: return "Trapped WFI/WFE";
        case 0x07: return "Trapped SIMD/FP";
        case 0x15: return "SVC";
        case 0x16: return "HVC";
        case 0x17: return "SMC";
        case 0x20: return "Insn abort (lower EL)";
        case 0x21: return "Insn abort (same EL)";
        case 0x24: return "Data abort (lower EL)";
        case 0x25: return "Data abort (same EL)";
        case 0x3C: return "BRK";
        default:   return "?";
    }
}

/* ================= 异常处理器（C 部分，由 vector_table.S 调用）================= */

void svc_handler_c(unsigned long esr, unsigned long elr, unsigned long spsr) {
    unsigned int ec  = (unsigned int)((esr >> 26) & 0x3F);
    unsigned int imm = (unsigned int)(esr & 0xFFFF);

    uart_puts("[SVC ] EC=0x");
    uart_puthex(ec, 2);
    uart_puts(" (");
    uart_puts(ec_name(ec));
    uart_puts(")  imm=0x");
    uart_puthex(imm, 2);
    uart_puts("\n");
    log_reg("ELR_EL1 ", elr);
    log_reg("SPSR_EL1", spsr);
    g_sync_ok = 1;
}

void irq_handler_c(void) {
    unsigned int iar = *GICC_IAR;          /* 中断确认：读中断 ID */
    unsigned int id  = iar & 0x3FF;

    uart_puts("[IRQ ] GICC_IAR=0x");
    uart_puthex(iar, 3);
    uart_puts("  中断ID=");
    uart_puthex(id, 2);
    uart_puts(" (EL1 物理定时器)\n");
    *GICC_EOIR = iar;                      /* 中断结束 */
    /* 禁用定时器：电平触发的中断，否则 EOI 后会立即重新断言 */
    __asm__ volatile("msr cntp_ctl_el0, %0" : : "r"(0));
    g_irq_ok = 1;
}

void fiq_handler_c(void)    { log_line("[FIQ ]", "收到 FIQ 异常"); }
void serror_handler_c(void) { log_line("[SErr]", "收到系统错误异常"); }
void unexpected_handler_c(void) { log_line("[WARN]", "未预期的向量入口"); }

/* ================= GIC / 定时器初始化 ================= */

static void gic_init(void) {
    *GICD_CTLR      = 1;                    /* 使能 distributor */
    *GICD_ISENABLER = (1U << TIMER_IRQ_ID); /* 使能定时器中断 30 */
    *GICC_CTLR      = 1;                    /* 使能 CPU interface */
    *GICC_PMR       = 0xFF;                 /* 优先级全放行 */
}

static void timer_init(void) {
    __asm__ volatile("msr cntp_tval_el0, %0" : : "r"(0x10000));
    __asm__ volatile("msr cntp_ctl_el0, %0"  : : "r"(1));
}

static void enable_irq(void) {
    __asm__ volatile("msr daifclr, #2");  /* 清 PSTATE.I，解除 IRQ 屏蔽 */
}

/* semihosting SYS_EXIT（0x18）—— v2 协议：x1=参数块指针，
 * 参数块[0]=0x20026(ADP_Stopped_ApplicationExit) 表示正常退出 */
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
    unsigned long sctlr, vbar;

    log_line("[====]", "AArch64 异常模型裸机验证");

    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    __asm__ volatile("mrs %0, vbar_el1"  : "=r"(vbar));
    log_reg("SCTLR_EL1", sctlr);
    log_reg("VBAR_EL1 ", vbar);

    gic_init();               /* 初始化 GIC */
    timer_init();             /* 配置 EL1 物理定时器 */
    enable_irq();             /* 使能 IRQ */
    log_info("GIC/定时器已初始化，IRQ 已使能");

    /* 1. 同步异常 SVC */
    log_line("[SYNC]", "触发同步异常 svc #0x42 ...");
    __asm__ volatile("svc #0x42");
    if (g_sync_ok)
        log_ok("SVC 处理器完成，ERET 返回");
    else
        log_fail("SVC 未正确处理");

    /* 2. 异步异常 IRQ */
    log_line("[ASYNC]", "等待异步中断（定时器 IRQ）...");
    while (!g_irq_ok) {
        /* 纯忙循环：g_irq_ok 由 IRQ 处理器置位。
         * 不用 wfe——wfe 被 IRQ 打断后 eret 会回到 wfe 再次休眠（无新事件）。 */
    }
    log_ok("IRQ 处理器完成（EOI 已写），ERET 返回");

    /* 汇总 */
    if (g_sync_ok && g_irq_ok)
        log_ok("全部验证通过（2/2）");
    else
        log_fail("存在未处理的异常");

    log_line("[EXIT]", "semihosting 正常退出");
    semihosting_exit();
    while (1) {                          /* 兜底 */
        __asm__ volatile("wfe");
    }
}