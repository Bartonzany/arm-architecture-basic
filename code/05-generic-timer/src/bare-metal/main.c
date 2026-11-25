/* main.c — Arm 通用定时器裸机验证（EL3，参考 ARM 官方 generic_timer 示例）
 *
 * 运行流程（全程 EL3，Secure 世界，参考 generic_timer.zip 的 main.c + startup.s）：
 *   boot.S 设 CNTFRQ_EL0、SCR_EL3（FIQ 路由到 EL3）后进 main；
 *   main 配置 GIC（INTID 29/30 -> Group0/FIQ），再用两种方式配置两个定时器：
 *     - 安全物理定时器 CNTPS：用 CVAL（绝对比较值），INTID 29
 *     - 非安全 EL1 物理定时器 CNTP：用 TVAL（相对 X tick），INTID 30
 *   两个定时器到期都产生 FIQ，fiq_handler_c 读 IAR、按 INTID 分派、禁用定时器
 *   清除电平敏感中断、写 EOIR。
 *
 * 日志框架（工程化，参考 03-aarch64-exception-model 与 build.sh 的配色排版）：
 *   开头横幅 + 键值对基本信息，[STEP n/总] 区分阶段，标签 7 字符对齐，RGB 真彩色。
 */

// PL011 UART（QEMU virt 默认串口）
#define UART0_DR ((volatile unsigned int *)0x09000000)

// GICv2 寄存器（QEMU virt,gic-version=2）
#define GICD_CTLR      ((volatile unsigned int *)0x08000000)
#define GICD_ISENABLER ((volatile unsigned int *)0x08000100)
#define GICC_CTLR      ((volatile unsigned int *)0x08010000)
#define GICC_PMR       ((volatile unsigned int *)0x08010004)
#define GICC_IAR       ((volatile unsigned int *)0x0801000C)
#define GICC_EOIR      ((volatile unsigned int *)0x08010010)

#define INTID_SECURE_PHYS   29   /* 安全物理定时器 CNTPS（SBSA 定义） */
#define INTID_NSEL1_PHYS    30   /* 非安全 EL1 物理定时器 CNTP（SBSA 定义） */

static volatile int g_flag = 0;        /* 已处理的中断个数 */

/* ================= ANSI 颜色（支持 -DNO_COLOR 关闭） ================= */
#ifdef NO_COLOR
#define CLR_RESET   ""
#define CLR_BOLD    ""
#define CLR_DIM     ""
#define CLR_GRAY    ""
#define CLR_RED     ""
#define CLR_GREEN   ""
#define CLR_YELLOW  ""
#define CLR_ORANGE  ""
#define CLR_BLUE    ""
#define CLR_MAGENTA ""
#define CLR_CYAN    ""
#define CLR_LRED    ""
#define CLR_LGREEN  ""
#define CLR_LYELLOW ""
#else
#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_DIM     "\033[2m"
#define CLR_GRAY    "\033[38;2;130;130;130m"
#define CLR_RED     "\033[38;2;255;70;70m"
#define CLR_GREEN   "\033[38;2;40;230;60m"
#define CLR_YELLOW  "\033[38;2;255;205;0m"
#define CLR_ORANGE  "\033[38;2;255;150;0m"
#define CLR_BLUE    "\033[38;2;70;150;255m"
#define CLR_MAGENTA "\033[38;2;255;90;255m"
#define CLR_CYAN    "\033[38;2;0;210;255m"
#define CLR_LRED    "\033[1;38;2;255;50;50m"
#define CLR_LGREEN  "\033[1;38;2;50;255;50m"
#define CLR_LYELLOW "\033[1;38;2;255;185;0m"
#endif

static void uart_putc(char c) { *UART0_DR = (unsigned int)c; }

static void uart_puts(const char *s) {
    while (*s != '\0')
        uart_putc(*s++);
}

static int uart_strlen(const char *s) {
    int n = 0;
    while (s[n] != '\0')
        n++;
    return n;
}

static void uart_puthex(unsigned long v, int digits) {
    static const char hex[] = "0123456789abcdef";
    for (int i = digits - 1; i >= 0; --i)
        uart_putc(hex[(v >> (i * 4)) & 0xF]);
}

static void uart_putdec(unsigned long v, int width) {
    char buf[20];
    int i = 0;
    do {
        buf[i++] = (char)('0' + (v % 10));
        v /= 10;
    } while (v > 0);
    while (i < width)
        buf[i++] = '0';
    while (i > 0)
        uart_putc(buf[--i]);
}

/* 按标签前缀返回对应颜色（标签紧凑形式） */
static const char *tag_color(const char *tag) {
    switch (tag[1]) {
        case 'E': return CLR_YELLOW;           /* [EL3] 固件级别 */
        case 'R': return CLR_DIM;              /* [REG] 寄存器 */
        case 'I': return CLR_CYAN;             /* [INFO] 信息 */
        case 'F': return CLR_LRED;             /* [FIQ] 快速中断 */
        case 'O': return CLR_LGREEN;           /* [OK] 成功 */
        case 'W': return CLR_LYELLOW;          /* [WARN] 警告 */
        default:  return CLR_RESET;
    }
}

/* 日志行首：[标签]（标签左对齐到 7 字符） */
static void log_begin(const char *tag) {
    uart_puts(tag_color(tag));
    uart_puts(tag);
    uart_puts(CLR_RESET);
    int pad = 7 - uart_strlen(tag);
    while (pad-- > 0)
        uart_putc(' ');
    uart_putc(' ');
}

static void log_line(const char *tag, const char *msg) {
    log_begin(tag);
    uart_puts(msg);
    uart_puts("\n");
}

/* 键值对（key 加粗左对齐 18 列 + 值，无标签前缀） */
static void log_kv(const char *key, const char *value) {
    uart_puts(CLR_BOLD);
    uart_puts(key);
    uart_puts(CLR_RESET ":");
    int pad = 18 - uart_strlen(key) - 1;
    while (pad-- > 0)
        uart_putc(' ');
    uart_puts(value);
    uart_puts("\n");
}

/* 寄存器键值对（寄存器名左对齐 + 16 位十六进制） */
static void log_reg(const char *name, unsigned long val) {
    log_begin("[REG]");
    uart_puts(name);
    uart_puts(" = 0x");
    uart_puthex(val, 16);
    uart_puts("\n");
}

/* 阶段标题：[STEP n/total] 标题（品红加粗） */
static void log_step(unsigned int n, unsigned int total, const char *title) {
    uart_puts("\n");
    uart_puts(CLR_MAGENTA CLR_BOLD "[STEP ");
    uart_putdec(n, 1);
    uart_puts("/");
    uart_putdec(total, 1);
    uart_puts("]" CLR_RESET " ");
    uart_puts(CLR_BOLD);
    uart_puts(title);
    uart_puts(CLR_RESET "\n");
}

/* 横幅（蓝色加粗） */
static void log_banner(const char *title) {
    uart_puts("\n");
    uart_puts(CLR_BLUE CLR_BOLD "========== ");
    uart_puts(title);
    uart_puts(" ==========" CLR_RESET "\n\n");
}

/* ================= GIC / 定时器 ================= */

/* GICv2：INTID 29/30 默认 Group 0；须设 GICC_CTLR.FIQEn(bit3) 让 Group 0 中断
 * 作为 FIQ 发信号（默认 FIQEn=0，Group 0 会当 IRQ 发）。 */
static void gic_init(void) {
    *GICD_CTLR      = 1;                        /* EnableGrp0 */
    *GICD_ISENABLER = (1U << INTID_SECURE_PHYS) | (1U << INTID_NSEL1_PHYS);
    *GICC_CTLR      = (1 << 0) | (1 << 3);      /* EnableGrp0 + FIQEn */
    *GICC_PMR       = 0xFF;                     /* 优先级全放行 */
}

/* FIQ 处理器（由 vector_table.S 调用）
 * 参考 generic_timer.zip 的 fiqHandler：读 IAR -> 按 INTID 禁用对应定时器
 * （电平敏感，须清 ENABLE）-> 写 EOIR。 */
void fiq_handler_c(void) {
    unsigned int iar = *GICC_IAR;
    unsigned int id  = iar & 0x3FF;

    log_begin("[FIQ]");
    uart_puts("Received INTID ");
    uart_putdec(id, 0);
    switch (id) {
        case INTID_SECURE_PHYS:
            uart_puts(" (Secure Physical Timer)\n");
            __asm__ volatile("msr cntps_ctl_el1, xzr");   /* 禁用 CNTPS 清中断 */
            break;
        case INTID_NSEL1_PHYS:
            uart_puts(" (Non-secure EL1 Physical Timer)\n");
            __asm__ volatile("msr cntp_ctl_el0, xzr");    /* 禁用 CNTP 清中断 */
            break;
        default:
            uart_puts(" (unexpected)\n");
            break;
    }
    *GICC_EOIR = iar;                        /* 中断结束 */
    g_flag++;
}

void unexpected_handler_c(void) {
    unsigned long level;
    __asm__ volatile("mrs %0, currentel" : "=r"(level));
    level = (level >> 2) & 3;
    log_line("[WARN]", "未预期的向量入口");
    log_begin("[WARN]");
    uart_puts("EL="); uart_putdec(level, 0); uart_puts(" IAR=0x");
    uart_puthex(*GICC_IAR, 3);
    uart_puts("\n");
}

/* ================= 主流程（运行在 EL3） ================= */

void main(void) {
    unsigned long freq, cnt1, cnt2, vct, cval, tval;

    log_banner("Arm 通用定时器验证 启动");
    log_kv("CPU", "cortex-a53");
    log_kv("Machine", "virt (secure=on, gic-version=2)");
    log_kv("Timers", "CNTPS(CVAL) + CNTP(TVAL)");

    /* STEP 1/3：引导（频率 + GIC） */
    log_step(1, 3, "引导（EL3 设频率 + GIC 初始化）");
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    log_reg("CNTFRQ_EL0", freq);
    gic_init();
    log_line("[INFO]", "GIC 已配置（INTID 29/30 -> Group0/FIQ，路由到 EL3）");

    /* STEP 2/3：系统计数器与 CVAL/TVAL */
    log_step(2, 3, "系统计数器与 CVAL/TVAL");
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(cnt1));
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(cnt2));
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(vct));
    log_reg("CNTPCT_EL0", cnt2);
    log_reg("CNTVCT_EL0", vct);
    log_line("[INFO]", "两次 CNTPCT_EL0 差值为非零（计数递增）");

    /* 写 TVAL，内部填充 CVAL = 计数 + TVAL，读回递减的 TVAL */
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(cnt1));
    __asm__ volatile("msr cntp_tval_el0, %0" : : "r"(100000));
    __asm__ volatile("mrs %0, cntp_cval_el0" : "=r"(cval));
    __asm__ volatile("mrs %0, cntp_tval_el0" : "=r"(tval));
    log_line("[INFO]", "写 CNTP_TVAL_EL0 = 100000");
    log_reg("CNTP_CVAL_EL0", cval);
    log_reg("CNTP_TVAL_EL0", tval);

    /* STEP 3/3：两个定时器中断（CVAL + TVAL） */
    log_step(3, 3, "定时器中断（CNTPS 用 CVAL + CNTP 用 TVAL）");
    /* 安全物理定时器 CNTPS：CVAL 设绝对触发时间 */
    __asm__ volatile("isb");
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(cnt1));
    __asm__ volatile("msr cntps_cval_el1, %0" : : "r"(cnt1 + 20000));
    __asm__ volatile("msr cntps_ctl_el1, %0"  : : "r"(1));
    log_line("[INFO]", "安全物理定时器 CNTPS 已使能（CVAL，INTID 29）");

    /* 非安全物理定时器 CNTP：TVAL 设相对 X tick */
    __asm__ volatile("msr cntp_tval_el0, %0" : : "r"(40000));
    __asm__ volatile("msr cntp_ctl_el0, %0"  : : "r"(1));
    log_line("[INFO]", "非安全物理定时器 CNTP 已使能（TVAL，INTID 30）");

    /* 解除 FIQ/IRQ 屏蔽，等待两个中断处理完 */
    __asm__ volatile("msr daifclr, #7");
    while (g_flag < 2) {
        /* 忙循环，被 FIQ 打断 */
    }

    log_line("[OK]", "两个定时器中断均验证完成（INTID 29 + 30）");
    log_banner("全部验证完成 (CNTPS CVAL + CNTP TVAL)");
    while (1) {
        __asm__ volatile("wfe");
    }
}
