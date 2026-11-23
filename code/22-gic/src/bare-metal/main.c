/* main.c — GICv3 裸机验证：识别 / 初始化 / 定时器 PPI / SGI
 *
 * 验证文档《22 - GICv3 和 v4 概述》的核心知识点：
 *   §7   三大编程接口：分发器(GICD_*)、再分发器(GICR_*)、CPU 接口(ICC_*_ELn 系统寄存器)
 *   §8.1 全局设置：GICD_CTLR（亲和性路由 + 各分组使能）
 *   §8.2 每 PE 设置：GICR_WAKER 唤醒、ICC_SRE/PMR/BPR1/IGRPEN1/CTLR
 *   §8.3 每 INTID 设置：优先级 + 分组(IGROUP/IGRPMOD) + 使能(ISENABLER)
 *   §9.2 接受中断：读 ICC_IAR1_EL1 拿 INTID
 *   §9.4 运行优先级：ICC_RPR_EL1（确认后=中断优先级，EOI 后回 idle 0xFF）
 *   §9.5 中断结束：EOImode=0 写 ICC_EOIR1_EL1 一步完成"优先级下降+撤销激活"
 *   §9.6 查询：ICC_HPPIR1_EL1（最高优先级挂起中断）
 *   §10   SGI：写 ICC_SGI1R_EL1 给自己发 SGI（INTID 1）
 */

#define UART0_DR ((volatile unsigned int *)0x09000000)

/* ---- GICv3 内存映射（QEMU virt, gic-version=3）----
 * 分发器 GICD  : 0x08000000
 * 再分发器 GICR : 0x080A0000（每个 PE 一份：RD 帧 + SGI 帧，各 64KB）
 *   PE0: RD 帧  @ 0x080A0000        （GICR_CTLR/WAKER）
 *        SGI 帧 @ 0x080B0000        （SGI/PPI 管理寄存器）
 */
#define GICD_BASE       0x08000000u
#define GICD_CTLR      (*(volatile unsigned int *)(GICD_BASE + 0x0000))
#define GICD_TYPER     (*(volatile unsigned int *)(GICD_BASE + 0x0004))
#define GICD_IIDR      (*(volatile unsigned int *)(GICD_BASE + 0x0008))

#define GICR_BASE       0x080A0000u
#define GICR_WAKER      (*(volatile unsigned int *)(GICR_BASE + 0x0000 + 0x0014))
#define GICR_IGROUPR0   (*(volatile unsigned int *)(GICR_BASE + 0x10000 + 0x0080))
#define GICR_IGRPMODR0  (*(volatile unsigned int *)(GICR_BASE + 0x10000 + 0x0D00))
#define GICR_ISENABLER0 (*(volatile unsigned int *)(GICR_BASE + 0x10000 + 0x0100))
#define GICR_IPRIORITY_BYTE(x) (*(volatile unsigned char *)(GICR_BASE + 0x10000 + 0x0400 + (x)))

#define TIMER_IRQ_ID   30              /* EL1 物理定时器 PPI（SBSA 定义）*/
#define SGI_ID         1               /* 自测用 SGI 的 INTID */

static volatile int g_timer_ok = 0;
static volatile int g_sgi_ok    = 0;

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
    uart_puthex(val, 8); uart_puts("\n");
}

static int exit_block[2];
static void semihosting_exit(void) {
    exit_block[0] = 0x20026; exit_block[1] = 0;
    __asm__ volatile("mov x1, %0\n\tmov x0, #0x18\n\thlt #0xF000"
                     : : "r"(exit_block) : "x0", "x1");
}

/* ============ 关键 ICC_* 系统寄存器读写（逐行对应汇编 MRS/MSR）============ */

static unsigned long rd_icc_iar1(void) { unsigned long v; __asm__ volatile("mrs %0, icc_iar1_el1"  : "=r"(v)); return v; }
static void wr_icc_eoir1(unsigned long v){ __asm__ volatile("msr icc_eoir1_el1, %0" : : "r"(v)); }
static unsigned long rd_icc_rpr(void)   { unsigned long v; __asm__ volatile("mrs %0, icc_rpr_el1"   : "=r"(v)); return v; }
static unsigned long rd_icc_hppir1(void){ unsigned long v; __asm__ volatile("mrs %0, icc_hppir1_el1": "=r"(v)); return v; }
static void wr_icc_sgi1r(unsigned long v){ __asm__ volatile("msr icc_sgi1r_el1, %0" : : "r"(v)); }

static void wr_icc_sre(unsigned long v)    { __asm__ volatile("msr icc_sre_el1, %0"     : : "r"(v)); }
static void wr_icc_pmr(unsigned long v)    { __asm__ volatile("msr icc_pmr_el1, %0"     : : "r"(v)); }
static void wr_icc_bpr1(unsigned long v)   { __asm__ volatile("msr icc_bpr1_el1, %0"    : : "r"(v)); }
static void wr_icc_igrpen1(unsigned long v){ __asm__ volatile("msr icc_igrpen1_el1, %0" : : "r"(v)); }
static void wr_icc_ctlr(unsigned long v)   { __asm__ volatile("msr icc_ctlr_el1, %0"    : : "r"(v)); }

/* ================= 异常处理器（由 vector_table.S 调用）================= */

void irq_handler_c(void) {
    unsigned long iar, id, rpr;

    /* §9.2 读 IAR 确认中断，拿到 INTID */
    iar = rd_icc_iar1();
    id  = iar & 0xFFFFFFu;

    /* §9.4 确认后运行优先级 = 中断优先级 */
    rpr = rd_icc_rpr();

    uart_puts("[IRQ ] ICC_IAR1_EL1=0x");
    uart_puthex(iar, 8);
    uart_puts(" INTID=0x");
    uart_puthex(id, 2);
    uart_puts("  ICC_RPR_EL1=0x");
    uart_puthex(rpr, 2);
    uart_puts("\n");

    if (id == TIMER_IRQ_ID) {
        /* 电平敏感中断：先写 EOI，再清定时器 ENABLE，否则反复触发 */
        wr_icc_eoir1(iar);                       /* §9.5 EOImode=0：一步下降+撤销激活 */
        __asm__ volatile("msr cntp_ctl_el0, xzr");  /* 清 ENABLE 清掉电平源 */
        g_timer_ok = 1;
    } else if (id == SGI_ID) {
        /* SGI 是边沿触发，无需清源，直接 EOI */
        wr_icc_eoir1(iar);
        g_sgi_ok = 1;
    } else {
        wr_icc_eoir1(iar);
    }
}

void svc_handler_c(unsigned long esr, unsigned long elr, unsigned long spsr) { (void)esr; (void)elr; (void)spsr; }
void fiq_handler_c(void)    {}
void serror_handler_c(void) {}
void unexpected_handler_c(void) {}

/* ================= GIC 初始化 ================= */

static void gic_init(void) {
    /* --- §8.1 全局：GICD_CTLR 开亲和性路由 + 各分组使能 --- */
    GICD_CTLR = (1u << 0)   /* EnableGrp0   */
              | (1u << 1)   /* EnableGrp1S  */
              | (1u << 2)   /* EnableGrp1NS */
              | (1u << 4)   /* ARE_NS：非安全态用亲和性路由(GICv3 模式) */
              | (1u << 5);  /* ARE_S ：安全态用亲和性路由 */

    /* --- §8.2 唤醒再分发器：清 ProcessorSleep，轮询 ChildrenAsleep --- */
    GICR_WAKER = GICR_WAKER & ~0x2u;          /* ProcessorSleep=0 */
    while (GICR_WAKER & 0x4u) { }             /* 等 ChildrenAsleep==0 */

    /* --- §8.2 CPU 接口：系统寄存器 --- */
    wr_icc_sre(0x1);      /* SRE=1：启用系统寄存器访问          */
    wr_icc_pmr(0xFF);     /* 优先级掩码：允许所有优先级          */
    wr_icc_bpr1(0x0);     /* 二进制点：全部 8 位都是组优先级     */
    wr_icc_igrpen1(0x1);  /* 使能 Group 1 中断信号               */
    wr_icc_ctlr(0x0);     /* EOImode=0：EOI 一步完成             */

    /* --- §8.3 配置 INTID 30 / 1：分组 + 优先级 + 使能 ---
     * IGROUP=1 + IGRPMOD=1 => Non-secure Group 1 => 在非安全 EL1 走 IRQ */
    unsigned int grp = (1u << TIMER_IRQ_ID) | (1u << SGI_ID);
    GICR_IGROUPR0  |= grp;                  /* INTID 30/1 -> Group 1 */
    GICR_IGRPMODR0 |= grp;                  /* Group 1 -> Non-secure  */
    GICR_IPRIORITY_BYTE(TIMER_IRQ_ID) = 0xA0;  /* 定时器优先级 0xA0（较低）*/
    GICR_IPRIORITY_BYTE(SGI_ID)       = 0x20;  /* SGI 优先级 0x20     */
    GICR_ISENABLER0 = grp;                  /* 使能 INTID 30 与 1     */
}

/* ================= 主流程 ================= */

void main(void) {
    unsigned long v;

    log_line("[====]", "AArch64 GICv3 裸机验证");

    /* §7 分发器：识别 GIC */
    log_reg("GICD_TYPER", GICD_TYPER);   /* 模型块 ID / 安全扩展 / 是否支持 Aff3 等 */
    log_reg("GICD_IIDR",  GICD_IIDR);    /* 实现标识（厂商/版本） */

    /* §8 初始化 GICv3 */
    gic_init();
    log_info("GICv3 初始化完成（GICD_CTLR/GICR_WAKER/ICC_SRE·PMR·BPR1·IGRPEN1·CTLR）");

    /* §9.6 空闲时：无挂起中断，运行优先级 = idle 0xFF */
    log_reg("ICC_HPPIR1_EL1", rd_icc_hppir1());
    log_reg("ICC_RPR_EL1",    rd_icc_rpr());

    /* --- 演示 1：定时器 PPI（INTID 30）--- */
    log_info("使能 EL1 物理定时器（INTID 30, 优先级 0xA0）...");
    __asm__ volatile("msr cntp_tval_el0, %0" : : "r"(0x8000));  /* 短倒计时 */
    __asm__ volatile("msr cntp_ctl_el0, %0" : : "r"(1));        /* ENABLE=1 */
    __asm__ volatile("msr daifclr, #2");                        /* 开 IRQ 屏蔽 */
    while (!g_timer_ok) { /* 忙循环，被定时器 IRQ 打断 */ }
    log_ok("定时器 PPI 处理完成（读 IAR1 -> INTID 0x1e -> 写 EOIR1）");

    /* §9.4 处理完写 EOI 后，运行优先级应回落 idle 0xFF */
    v = rd_icc_rpr();
    log_reg("EOI 后 ICC_RPR_EL1", v);
    log_ok((v == 0xFF) ? "运行优先级已回 idle 0xFF" : "运行优先级异常");

    /* --- 演示 2：给自己发 SGI（INTID 1）--- */
    log_info("写 ICC_SGI1R_EL1 给自己发 SGI（INTID 1）...");
    /* ICC_SGI1R_EL1 位域：Aff3[55:48] IRM[40] Aff2[39:32] INTID[27:24] Aff1[23:16] TargetList[15:0] */
    wr_icc_sgi1r((0ul << 48)                  /* Aff3 = 0        */
               | (0ul << 40)                  /* IRM  = 0（按亲和性路由）*/
               | (0ul << 32)                  /* Aff2 = 0        */
               | ((unsigned long)SGI_ID << 24)/* INTID = 1       */
               | (0ul << 16)                  /* Aff1 = 0        */
               | (1ul << 0));                 /* TargetList: aff0=0 → 自己 */
    while (!g_sgi_ok) { /* 忙循环，被 SGI 的 IRQ 打断 */ }
    log_ok("SGI 处理完成（读 IAR1 -> INTID 1 -> 写 EOIR1）");

    log_line("[EXIT]", "semihosting 正常退出");
    semihosting_exit();
    while (1) { __asm__ volatile("wfe"); }
}