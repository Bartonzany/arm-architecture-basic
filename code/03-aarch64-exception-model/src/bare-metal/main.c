/* main.c — AArch64 异常模型裸机验证（EL0–EL3）
 *
 * 运行流程：
 *   _start(EL3) 逐级降级 EL3 -> EL2 -> EL1 -> EL0（boot.S，每级调用 *_report 打印）
 *   main(EL0) 依次验证各类异常：SVC / SVC-HVC-SMC 链条 / BRK / UDF / 定时器 IRQ
 *   每级处理器打印自己级别、异常类型、ELR/SPSR，印证“进入异常”与“ERET 返回”。
 *
 * 日志框架（工程化，参考 linux-5.15/shell_scripts/build.sh 的配色与排版）：
 *   - 开头横幅（蓝加粗）+ 键值对基本信息（对齐的 key: value）；
 *   - 用 [STEP n/总] 区分阶段（品红加粗），取代行号与①②③符号；
 *   - 统一行格式：[标签] 消息，标签固定 7 字符宽左对齐；
 *   - 分级配色（RGB 真彩色）：EL0 绿 / EL1 青 / EL2 品红 / EL3 金黄，SYNC 蓝 /
 *     ASYNC 橙 / IRQ 红 / OK 亮绿 / WARN 橙黄 / 寄存器与返回 暗灰；
 *   - 支持编译期 NO_COLOR（make NO_COLOR=1 关闭颜色）。
 * 颜色经 PL011 串口原样透传，由终端解释；裸机无法检测 TTY，默认开启颜色。
 */

// PL011 UART（QEMU virt 默认串口）数据寄存器
#define UART0_DR ((volatile unsigned int *)0x09000000)

// GICv2 寄存器（QEMU virt,gic-version=2）
#define GICD_CTLR      ((volatile unsigned int *)0x08000000)
#define GICD_IGROUPR   ((volatile unsigned int *)0x08000080)
#define GICD_ISENABLER ((volatile unsigned int *)0x08000100)
#define GICC_CTLR      ((volatile unsigned int *)0x08010000)
#define GICC_PMR       ((volatile unsigned int *)0x08010004)
#define GICC_IAR       ((volatile unsigned int *)0x0801000C)
#define GICC_EOIR      ((volatile unsigned int *)0x08010010)

#define TIMER_IRQ_ID   30              /* EL1 物理定时器 PPI 中断号 */

static volatile int g_irq_ok = 0;      /* 异步中断是否已处理（被 EL1 处理器置位） */

/* ================= ANSI 颜色（支持编译期 -DNO_COLOR 关闭） ================= */
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

/* 十进制输出（可选左补零到 width 位） */
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

/* 按标签前缀返回对应颜色（标签紧凑形式 [ELx]/[SYNC]/[OK] 等） */
static const char *tag_color(const char *tag) {
    switch (tag[1]) {
        case 'E':                              /* [ELx] 异常级别 */
            switch (tag[3]) {
                case '0': return CLR_GREEN;    /* EL0 应用 */
                case '1': return CLR_CYAN;     /* EL1 内核 */
                case '2': return CLR_MAGENTA;  /* EL2 hypervisor */
                case '3': return CLR_YELLOW;   /* EL3 固件 */
                default:  return CLR_RESET;
            }
        case 'R': return CLR_DIM;              /* [REG]/[RET] 寄存器/返回 */
        case 'S':
            if (tag[2] == 'E') return CLR_LRED; /* [SErr] 系统错误 */
            return CLR_BLUE;                    /* [SYNC] 同步异常（蓝） */
        case 'A': return CLR_ORANGE;           /* [ASYNC] 异步异常（橙） */
        case 'I':
            if (tag[2] == 'N') return CLR_CYAN; /* [INFO] 信息 */
            return CLR_RED;                     /* [IRQ] 中断（红） */
        case 'O': return CLR_LGREEN;           /* [OK] 成功 */
        case 'W': return CLR_LYELLOW;          /* [WARN] 警告 */
        case 'F': return CLR_LRED;             /* [FIQ] */
        default:  return CLR_RESET;
    }
}

/* 输出日志行首：[标签]（标签左对齐到 7 字符，对齐 build.sh 的 %-7s，消息从固定列开始） */
static void log_begin(const char *tag) {
    uart_puts(tag_color(tag));
    uart_puts(tag);
    uart_puts(CLR_RESET);
    int pad = 7 - uart_strlen(tag);
    while (pad-- > 0)
        uart_putc(' ');
    uart_putc(' ');
}

/* 单行日志 */
static void log_line(const char *tag, const char *msg) {
    log_begin(tag);
    uart_puts(msg);
    uart_puts("\n");
}

/* 键值对日志（key 加粗，左对齐到 18 列，对齐 build.sh 的 log_kv，无标签前缀） */
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

/* 寄存器键值对（寄存器名左对齐 + 16 位十六进制值） */
static void log_reg(const char *name, unsigned long val) {
    log_begin("[REG]");
    uart_puts(name);
    uart_puts(" = 0x");
    uart_puthex(val, 16);
    uart_puts("\n");
}

/* 阶段标题：[STEP n/total] 标题（品红加粗，前面空一行做分隔） */
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

/* 测试项标题：---------- 标题 ----------（青色加粗，前面空一行） */
static void log_test(const char *title) {
    uart_puts("\n");
    uart_puts(CLR_CYAN CLR_BOLD "---------- ");
    uart_puts(title);
    uart_puts(" ----------" CLR_RESET "\n");
}

/* 横幅分隔线（蓝色加粗） */
static void log_banner(const char *title) {
    uart_puts("\n");
    uart_puts(CLR_BLUE CLR_BOLD "========== ");
    uart_puts(title);
    uart_puts(" ==========" CLR_RESET "\n\n");
}

static const char *el_name(unsigned long level) {
    switch (level) {
        case 0: return "EL0";
        case 1: return "EL1";
        case 2: return "EL2";
        case 3: return "EL3";
        default: return "?";
    }
}

/* 异常级别对应的颜色（用于高亮消息里的 ELx，与标签配色一致） */
static const char *level_color(unsigned long level) {
    switch (level) {
        case 0: return CLR_GREEN;
        case 1: return CLR_CYAN;
        case 2: return CLR_MAGENTA;
        case 3: return CLR_YELLOW;
        default: return CLR_RESET;
    }
}

/* 打印"当前级别 ELx（描述）"：[ELx] 标签与消息里的 ELx 都用对应颜色高亮 */
static void log_current_level(unsigned long level, const char *desc) {
    static const char *tags[4] = { "[EL0]", "[EL1]", "[EL2]", "[EL3]" };
    if (level > 3)
        return;
    log_begin(tags[level]);
    uart_puts("当前级别 ");
    uart_puts(level_color(level));
    uart_puts(el_name(level));
    uart_puts(CLR_RESET);
    uart_puts(desc);
    uart_puts("\n");
}

/* ESR_ELx.EC 字段 -> 异常类型名 */
static const char *ec_name(unsigned int ec) {
    switch (ec) {
        case 0x15: return "SVC";
        case 0x16: return "HVC";
        case 0x17: return "SMC";
        case 0x3C: return "BRK";
        case 0x00: return "UNDEFINED";
        case 0x20: return "Insn abort (lower EL)";
        case 0x21: return "Insn abort (same EL)";
        case 0x24: return "Data abort (lower EL)";
        case 0x25: return "Data abort (same EL)";
        default:   return "Unknown";
    }
}

/* ================= 启动横幅 + 基本信息（boot.S 在 EL3 阶段调用） ================= */

void startup_banner(void) {
    log_banner("AArch64 异常模型验证 启动");
    log_kv("CPU", "cortex-a53");
    log_kv("Machine", "virt (secure=on, virtualization=on)");
    log_kv("Levels", "EL0 - EL3");
    log_kv("Tests", "5 项（SVC / 链条 / BRK / UDF / IRQ）");
    log_step(1, 2, "引导降级 EL3 -> EL2 -> EL1 -> EL0");
}

/* ================= 引导降级时各级别报告（boot.S 调用）================= */

void el3_report(void) {
    unsigned long scr, vbar;
    __asm__ volatile("mrs %0, scr_el3"  : "=r"(scr));
    __asm__ volatile("mrs %0, vbar_el3" : "=r"(vbar));
    log_current_level(3, "（Secure 固件）");
    log_reg("SCR_EL3  ", scr);
    log_reg("VBAR_EL3 ", vbar);
    log_line("[EL3]", "设 SCR_EL3(NS=1,RW=1,HCE=1)，ERET 到 EL2（切换安全状态）");
}

void el2_report(void) {
    unsigned long hcr, vbar;
    __asm__ volatile("mrs %0, hcr_el2"  : "=r"(hcr));
    __asm__ volatile("mrs %0, vbar_el2" : "=r"(vbar));
    log_current_level(2, "（Hypervisor）");
    log_reg("HCR_EL2  ", hcr);
    log_reg("VBAR_EL2 ", vbar);
    log_line("[EL2]", "设 HCR_EL2(RW=1)，ERET 到 EL1");
}

void el1_report(void) {
    unsigned long sctlr, vbar;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    __asm__ volatile("mrs %0, vbar_el1"  : "=r"(vbar));
    log_current_level(1, "（内核）");
    log_reg("VBAR_EL1 ", vbar);
    log_reg("SCTLR_EL1", sctlr);
    log_line("[EL1]", "设 SPSR_EL1=EL0t，ERET 到 EL0");
}

/* EL3 初始化（boot.S 在 Secure EL3 调用）：GIC 属平台级资源，由固件统一配置。
 * 关键点：GICD_IGROUPR 等寄存器是 Secure-only，必须在切到 Non-secure 之前写好。 */
void el3_setup(void) {
    *GICD_IGROUPR   = (1U << TIMER_IRQ_ID); /* PPI30 -> Group1(Non-secure) */
    *GICD_CTLR      = 3;                    /* 使能 distributor（Grp0 + Grp1） */
    *GICD_ISENABLER = (1U << TIMER_IRQ_ID); /* 使能定时器中断 30 */
    *GICC_CTLR      = 3;                    /* 使能 CPU interface（Grp0 + Grp1） */
    *GICC_PMR       = 0xFF;                 /* 优先级全放行 */
    log_line("[EL3]", "GIC 已配置（PPI30 -> Group1，distributor/CPU interface 使能）");
}

/* ================= 低 EL 同步异常分发（vector_table.S 调用）================= */
/* dispatch_sync(level, esr, elr, spsr) -> 返回 1 表示需把 ELR+4 跳过触发指令
 *
 * 链条逻辑：EL1 收到带 imm=0x42 的 SVC -> 转发 HVC 到 EL2；EL2 收到 HVC ->
 * 转发 SMC 到 EL3；EL3 收到 SMC -> 终点。嵌套异常依次返回即可逐级降回。 */
int dispatch_sync(unsigned long level, unsigned long esr,
                  unsigned long elr, unsigned long spsr) {
    unsigned int ec  = (unsigned int)((esr >> 26) & 0x3F);
    unsigned int imm = (unsigned int)(esr & 0xFFFF);
    int skip = 0;

    log_begin("[SYNC]");
    uart_puts("到达 ");
    uart_puts(level_color(level));
    uart_puts(el_name(level));
    uart_puts(CLR_RESET);
    uart_puts("：EC=0x");
    uart_puthex(ec, 2);
    uart_puts(" (");
    uart_puts(ec_name(ec));
    uart_puts(")  imm=0x");
    uart_puthex(imm, 2);
    uart_puts("\n");
    log_reg("ELR_ELx", elr);
    log_reg("SPSR_ELx", spsr);

    if (level == 1) {
        if (ec == 0x15) {                       /* SVC：EL0 -> EL1 */
            if (imm == 0x42) {                  /* 立即数标记“链条” */
                log_line("[EL1]", "转发 HVC 到 EL2 ...");
                __asm__ volatile("hvc #0");
                log_line("[RET]", "从 EL3 逐级返回到 EL1");
            } else {
                log_line("[EL1]", "普通 SVC，直接 ERET 返回 EL0");
            }
        } else if (ec == 0x3c) {                /* BRK 调试异常 */
            log_line("[EL1]", "BRK 调试异常，ELR+4 跳过该指令");
            skip = 1;
        } else if (ec == 0x00) {                /* 无效指令 */
            log_line("[EL1]", "无效指令，ELR+4 跳过该指令");
            skip = 1;
        } else if (ec == 0x24 || ec == 0x25) {  /* 数据中止 */
            log_line("[EL1]", "数据中止，ELR+4 跳过该指令");
            skip = 1;
        }
    } else if (level == 2) {
        if (ec == 0x16) {                       /* HVC：EL1 -> EL2 */
            log_line("[EL2]", "转发 SMC 到 EL3 ...");
            __asm__ volatile("smc #0");
            log_line("[RET]", "从 EL3 返回到 EL2");
        }
    } else if (level == 3) {
        if (ec == 0x17) {                       /* SMC：EL2 -> EL3 */
            log_line("[EL3]", "到达终点，开始逐级 ERET 返回");
        }
    }

    return skip;
}

/* ================= 异步异常处理器 ================= */

void irq_handler_c(void) {
    unsigned int iar = *GICC_IAR;          /* 中断确认：读中断 ID */
    unsigned int id  = iar & 0x3FF;

    log_begin("[IRQ]");
    uart_puts("GICC_IAR=0x");
    uart_puthex(iar, 3);
    uart_puts("  中断ID=0x");
    uart_puthex(id, 2);
    uart_puts(" (EL1 物理定时器)\n");
    *GICC_EOIR = iar;                      /* 中断结束 */
    /* 禁用定时器：电平触发中断，否则 EOI 后会立即重新断言 */
    __asm__ volatile("msr cntp_ctl_el0, %0" : : "r"(0));
    g_irq_ok = 1;
}

void fiq_handler_c(void)    { log_line("[FIQ]", "收到 FIQ 异常（本示例未主动触发）"); }
void serror_handler_c(void) { log_line("[SErr]", "收到系统错误异常"); }

void unexpected_handler_c(void) {
    unsigned long esr = 0, elr = 0;
    unsigned long level = 3;
    __asm__ volatile("mrs %0, currentel" : "=r"(level));
    level = (level >> 2) & 3;
    if (level == 3) {
        __asm__ volatile("mrs %0, esr_el3" : "=r"(esr));
        __asm__ volatile("mrs %0, elr_el3" : "=r"(elr));
    } else if (level == 2) {
        __asm__ volatile("mrs %0, esr_el2" : "=r"(esr));
        __asm__ volatile("mrs %0, elr_el2" : "=r"(elr));
    } else {
        __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
        __asm__ volatile("mrs %0, elr_el1" : "=r"(elr));
    }
    log_line("[WARN]", "未预期的向量入口");
    log_begin("[WARN]");
    uart_puts("EL=");
    uart_puts(el_name(level));
    uart_puts("\n");
    log_reg("ESR_ELx", esr);
    log_reg("ELR_ELx", elr);
}

/* ================= 定时器演示 ================= */

static void timer_irq_demo(void) {
    /* GIC 已由 Secure EL3（el3_setup）初始化；这里 EL0 直接启动定时器。
     * EL0 可通过 cntp_*_el0 访问 EL1 物理定时器（boot.S 已设 CNTKCTL_EL1.EL0PTEN=1）。 */
    __asm__ volatile("msr cntp_tval_el0, %0" : : "r"(0x20000));
    __asm__ volatile("msr cntp_ctl_el0, %0"  : : "r"(1));
    log_line("[ASYNC]", "等待异步中断（定时器 IRQ -> EL1 +0x480）...");
    while (!g_irq_ok) {
        /* 纯忙循环：g_irq_ok 由 EL1 的 IRQ 处理器置位 */
    }
}

/* ================= 主流程（运行在 EL0）================= */

void main(void) {
    log_current_level(0, "（应用）");

    log_step(2, 2, "异常演示");

    log_test("SVC 系统调用（EL0 -> EL1 简单往返）");
    __asm__ volatile("svc #0");
    log_line("[OK]", "SVC 往返完成，回到 EL0");

    log_test("SVC/HVC/SMC 链条（EL0 -> EL1 -> EL2 -> EL3）");
    __asm__ volatile("svc #0x42");
    log_line("[OK]", "链条完成，回到 EL0");

    log_test("BRK 调试异常");
    __asm__ volatile("brk #0");
    log_line("[OK]", "BRK 处理完成，回到 EL0");

    log_test("UDF 无效指令");
    __asm__ volatile("udf #0");
    log_line("[OK]", "无效指令处理完成，回到 EL0");

    log_test("IRQ 定时器中断（异步）");
    timer_irq_demo();
    log_line("[OK]", "IRQ 处理完成，回到 EL0");

    log_banner("全部验证完成 (EL0<->EL1<->EL2<->EL3、同步/异步异常)");
    while (1) {
        __asm__ volatile("wfe");
    }
}