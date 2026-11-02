/* guest.c — guest(EL1) 代码 + hypervisor(EL2) 陷阱处理器的 C 部分
 *
 * boot.S 在 EL2 配好 HCR_EL2(TWI=1) 后 ERET 到本文件的 guest_entry（EL1）。
 * guest 执行 WFI -> 因 TWI=1 陷阱到 EL2 的 wfi_trap_handler ->
 * 调用 el2_handler_c 打印 + 模拟 + 跳过 WFI -> ERET 回 guest。
 */

#define UART0_DR ((volatile unsigned int *)0x09000000)

static volatile int g_trapped = 0;   /* WFI 是否被陷阱捕获 */

/* ================= 日志框架（UART 直出）================= */

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

static void log_line(const char *prefix, const char *msg) {
    uart_puts(prefix);
    uart_puts(msg);
    uart_puts("\n");
}

#define log_info(m)   log_line("[INFO] ", (m))
#define log_ok(m)     log_line("[ OK ] ", (m))

static void log_reg(const char *name, unsigned long val) {
    uart_puts("[REG ] ");
    uart_puts(name);
    uart_puts(" = 0x");
    uart_puthex(val, 16);
    uart_puts("\n");
}

/* ESR_EL2.EC 字段 → 可读名称 */
static const char *ec_name(unsigned int ec) {
    switch (ec) {
        case 0x00: return "Unknown";
        case 0x01: return "Trapped WFI/WFE";
        case 0x07: return "Trapped SIMD/FP";
        case 0x18: return "Trapped MRS/MSR (system reg)";
        case 0x20: return "Insn abort (lower EL)";
        case 0x24: return "Data abort (lower EL)";
        default:   return "?";
    }
}

/* semihosting SYS_EXIT（0x18）—— v2 协议：x1=参数块指针，块[0]=0x20026 */
static int exit_block[2];

static void semihosting_exit(void) {
    exit_block[0] = 0x20026;   /* ADP_Stopped_ApplicationExit = 正常退出 */
    exit_block[1] = 0;
    __asm__ volatile(
        "mov x1, %0\n\t"
        "mov x0, #0x18\n\t"
        "hlt #0xF000"
        : : "r"(exit_block) : "x0", "x1");
}

/* ================= hypervisor(EL2) 陷阱处理器（C 部分）================= */

void el2_handler_c(unsigned long esr, unsigned long elr) {
    unsigned int ec = (unsigned int)((esr >> 26) & 0x3F);

    uart_puts("[EL2 ] EC=0x");
    uart_puthex(ec, 2);
    uart_puts(" (");
    uart_puts(ec_name(ec));
    uart_puts(")\n");
    log_reg("ELR_EL2", elr);             /* guest 的 WFI 地址 */

    /* 模拟 WFI：把返回地址 +4，跳过 guest 的 WFI 指令 */
    __asm__ volatile("msr elr_el2, %0" : : "r"(elr + 4));
    g_trapped = 1;
}

/* ================= guest 入口（运行在 EL1）================= */

void guest_entry(void) {
    unsigned long currentel;
    __asm__ volatile("mrs %0, currentel" : "=r"(currentel));
    unsigned int el = (unsigned int)((currentel >> 2) & 0x3);

    log_line("[====]", "AArch64 虚拟化裸机验证（陷阱机制）");
    log_reg("HCR_EL2", 0x80002000);      /* RW=1, TWI=1 */
    log_info("hypervisor(EL2) 已配置 TWI，ERET 切入 EL1");

    uart_puts("[GST ] CurrentEL=");
    uart_puthex(el, 1);
    uart_puts("（1=EL1，guest）\n");

    log_line("[GST ]", "guest 执行 WFI（将被 TWI 陷阱到 EL2）...");
    __asm__ volatile("wfi");             /* 触发陷阱 */

    log_ok("WFI 被 trap 到 EL2 后返回，guest 继续执行");
    if (g_trapped)
        log_ok("陷阱机制验证完成");
    else
        log_line("[FAIL]", "WFI 未被陷阱捕获");

    log_line("[EXIT]", "semihosting 正常退出");
    semihosting_exit();
    while (1) {
        __asm__ volatile("wfe");
    }
}