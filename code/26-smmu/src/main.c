/* main.c — SMMUv3 裸机验证：识别 / 最小配置 / 命令队列
 *
 * 验证文档《26 - SMMU 软件指南》的核心知识点：
 *   §2      SMMU 作用（转换/保护/隔离），StreamID 定位 STE
 *   §4.1    SMMU 寄存器（IDR* 识别 + 控制寄存器）
 *   §4.2    流表（线性流表，STRTAB_BASE/CFG）
 *   §4.8    命令队列（CMDQ_BASE/PROD/CONS + CMD_SYNC）
 *   §4.9    事件队列（EVENTQ_BASE/PROD/CONS）
 *   §4.11   最小配置七步序列（这就是本程序的主体）
 *   §5.1    I/O 一致性（IDR0.COHACC）
 *
 * 运行环境：QEMU -machine virt,iommu=smmuv3，SMMUv3 在 0x09050000。
 */

#define UART0_DR ((volatile unsigned int *)0x09000000)

/* ---- SMMUv3 寄存器（非安全，寄存器页 0）---- */
#define SMMU_BASE 0x09050000u
#define SMMU_RD32(o)       (*(volatile unsigned int *)(SMMU_BASE + (o)))
#define SMMU_WR32(o, v)    (*(volatile unsigned int *)(SMMU_BASE + (o)) = (v))

/* 识别寄存器 */
#define SMMU_IDR0   SMMU_RD32(0x0000)
#define SMMU_IDR1   SMMU_RD32(0x0004)
#define SMMU_IDR2   SMMU_RD32(0x0008)
#define SMMU_IDR3   SMMU_RD32(0x000C)
#define SMMU_IDR5   SMMU_RD32(0x0014)

/* 控制寄存器 */
#define SMMU_CR0      SMMU_RD32(0x0020)
#define SMMU_CR0ACK   SMMU_RD32(0x0024)
#define SMMU_CR1      SMMU_RD32(0x0028)
#define SMMU_IRQ_CTRL SMMU_RD32(0x0050)
#define SMMU_GERROR   SMMU_RD32(0x0060)

/* 数据结构寄存器 */
#define SMMU_STRTAB_BASE_CFG SMMU_RD32(0x0088)
#define SMMU_CMDQ_PROD  SMMU_RD32(0x0098)
#define SMMU_CMDQ_CONS  SMMU_RD32(0x009C)
#define SMMU_EVENTQ_PROD SMMU_RD32(0x00A8)
#define SMMU_EVENTQ_CONS SMMU_RD32(0x00AC)

/* CR0 位（SMMUv3 规范：SMMUEN=0，EVENTQEN=2，CMDQEN=3，bit1 保留）*/
#define CR0_SMMUEN   (1u << 0)
#define CR0_EVENTQEN (1u << 2)
#define CR0_CMDQEN   (1u << 3)

/* ---- 内存数据结构（.bss 中，按架构要求对齐）---- */
static unsigned int strtab[32] __attribute__((aligned(64)));   /* 2 个 STE = 128 字节 */
static unsigned int cmdq[512 / 4] __attribute__((aligned(64)));/* 32 项 × 16 字节 = 512 字节 */
static unsigned int eventq[1024 / 4] __attribute__((aligned(64))); /* 32 项 × 32 字节 */

/* 命令操作码（SMMUv3 规范，命令项首字节）*/
#define CMD_TLBI_NSNH_ALL 0x30
#define CMD_SYNC          0x46

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
#define log_fail(m) log_line("[FAIL] ", (m))

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

/* ================= 异常处理器（由 vector_table.S 调用，本例不使用）================= */

void irq_handler_c(void) {}
void svc_handler_c(unsigned long esr, unsigned long elr, unsigned long spsr) { (void)esr; (void)elr; (void)spsr; }
void fiq_handler_c(void)    {}
void serror_handler_c(void) {}
void unexpected_handler_c(void) {}

/* ================= SMMU 最小配置（§4.11 七步）================= */

static void dsb_sy(void) { __asm__ volatile("dsb sy"); }

static void log_cr0(const char *when) {
    log_reg(when ? when : "SMMU_CR0", SMMU_CR0);
    log_reg("SMMU_CR0ACK", SMMU_CR0ACK);
}

int main(void) {
    unsigned long idr0, idr1, idr3, idr5;
    unsigned long cr0;
    unsigned long cmdq_cons, eventq_cons;

    log_line("[====]", "AArch64 SMMUv3 裸机验证");

    /* --- §4.1 识别 SMMU --- */
    idr0 = SMMU_IDR0; idr1 = SMMU_IDR1; idr3 = SMMU_IDR3; idr5 = SMMU_IDR5;
    log_reg("SMMU_IDR0", idr0);
    log_reg("SMMU_IDR1", idr1);
    log_reg("SMMU_IDR3", idr3);
    log_reg("SMMU_IDR5", idr5);

    /* --- 步骤 1：分配流表（线性），STE.V=0 标记无效，DSB --- */
    SMMU_WR32(0x0088, 0x0);            /* STRTAB_BASE_CFG：FMT=0 线性, LOG2SIZE=0(2 个 STE) */
    SMMU_WR32(0x0080, (unsigned long)strtab & 0xFFFFFFFFu);   /* STRTAB_BASE 低 32 位 */
    SMMU_WR32(0x0084, 0);              /* STRTAB_BASE 高 32 位 */
    /* strtab 在 .bss 已清 0，即所有 STE.V=0（无效）*/
    dsb_sy();                          /* 确保 SMMU 能观察到写数据（§4.11 步骤 1）*/
    log_info("流表已配置（线性, 2 个 STE, 均无效 V=0）");

    /* --- 步骤 2：分配命令队列 + 事件队列 ---
     * 64 位 Q_BASE 寄存器：[4:0]=LOG2SIZE，[51:5]=ADDR。
     * 分两次 32 位写：低 32 位含 LOG2SIZE，高 32 位为地址高位（本例为 0）。*/
    SMMU_WR32(0x0090, (unsigned int)(unsigned long)cmdq | 4u);   /* CMDQ_BASE 低32: ADDR|LOG2SIZE=4(32项) */
    SMMU_WR32(0x0094, 0);                                        /* CMDQ_BASE 高32 = 0 */
    SMMU_WR32(0x0098, 0);              /* CMDQ_PROD = 0 */
    SMMU_WR32(0x009C, 0);              /* CMDQ_CONS = 0 */
    SMMU_WR32(0x00A0, (unsigned int)(unsigned long)eventq | 4u); /* EVENTQ_BASE 低32 */
    SMMU_WR32(0x00A4, 0);                                        /* EVENTQ_BASE 高32 = 0 */
    SMMU_WR32(0x00A8, 0);              /* EVENTQ_PROD = 0（SMMU 写）*/
    SMMU_WR32(0x00AC, 0);              /* EVENTQ_CONS = 0 */
    log_info("命令队列(32项)/事件队列(32项)已配置");

    /* --- 步骤 3/4：设置内存属性 + 使能 IRQ（本例 IRQ 关闭）--- */
    SMMU_WR32(0x0028, 0);              /* CR1 = 0：默认内存属性 */
    SMMU_WR32(0x0050, 0);              /* IRQ_CTRL = 0：不需中断 */

    /* --- 步骤 5：使能命令/事件队列（轮询 CR0ACK）--- */
    SMMU_WR32(0x0020, CR0_CMDQEN);     /* 先使能命令队列 */
    while (!(SMMU_CR0ACK & CR0_CMDQEN)) { }
    log_ok("命令队列已使能（CR0ACK.CMDQEN=1）");

    SMMU_WR32(0x0020, CR0_CMDQEN | CR0_EVENTQEN);
    while (!(SMMU_CR0ACK & CR0_EVENTQEN)) { }
    log_ok("事件队列已使能（CR0ACK.EVENTQEN=1）");

    /* --- 步骤 6：发 CMD_TLBI_NSNH_ALL + CMD_SYNC，观察 CONS 前进 --- */
    cmdq[0] = CMD_TLBI_NSNH_ALL;       /* 项 0：非安全 EL1 上下文 TLB 全部失效 */
    cmdq[4] = CMD_SYNC;                /* 项 1：同步（前命令效果可见才消费）*/
    dsb_sy();
    /* 告诉 SMMU 有 2 条新命令（WR=2），等 CONS.RD 同样到 2 */
    SMMU_WR32(0x0098, 2);              /* PROD.WR = 2：告知 SMMU 有 2 条新命令 */
    log_info("已提交 CMD_TLBI_NSNH_ALL + CMD_SYNC，等待消费 ...");
    while ((SMMU_CMDQ_CONS & 0xFFFFFu) < 2) { }   /* RD[19:0] 前进到 2 */
    cmdq_cons = SMMU_CMDQ_CONS;
    uart_puts("[REG ] 消费后 CMDQ_CONS = 0x");
    uart_puthex(cmdq_cons, 8);
    uart_puts("（RD=");
    uart_puthex(cmdq_cons & 0xFFFFFu, 2);
    uart_puts("）\n");
    log_ok("CMD_SYNC 已消费，此前命令效果可见");

    /* --- 步骤 7：使能转换（SMMUEN），轮询 CR0ACK --- */
    cr0 = CR0_CMDQEN | CR0_EVENTQEN | CR0_SMMUEN;
    SMMU_WR32(0x0020, cr0);
    while (!(SMMU_CR0ACK & CR0_SMMUEN)) { }
    log_ok("SMMU 转换已使能（CR0ACK.SMMUEN=1）");
    log_cr0("最终 SMMU_CR0");

    /* 检查是否有全局错误（§4.10）*/
    log_reg("SMMU_GERROR", SMMU_GERROR);
    {
        unsigned int eval = SMMU_GERROR & 0xFFFFu;
        log_ok(eval == 0 ? "无全局错误（GERROR=0）" : "存在全局错误");
    }

    /* 事件队列为空（无设备流量，不产生事件）*/
    log_reg("SMMU_EVENTQ_CONS", SMMU_EVENTQ_CONS);
    log_reg("SMMU_EVENTQ_PROD", SMMU_EVENTQ_PROD);

    log_line("[EXIT]", "semihosting 正常退出");
    semihosting_exit();
    while (1) { __asm__ volatile("wfe"); }
    return 0;
}