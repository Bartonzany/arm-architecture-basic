S({ph:0, pc:A.start, hw:true, code:"— RESET —", loc:"QEMU 复位", desc:"QEMU virt（secure=on, virtualization=on）上电：CPU 从 0x40000000 取第一条指令，复位进入最高级别 EL3（Secure），PSTATE=EL3h、DAIF 全屏蔽。", to:{pc:A.start, el:3, mode:"EL3h", daif:15, secure:true, sp:0}, ev:{kind:"reset"}});
S({ph:0, pc:A.start, code:"ldr x19, =vector_table", loc:"boot.S:27 _start", desc:"把向量表地址存进 X19。PCS 规定 X19–X28 是被调用者保存寄存器，跨 bl 调用不丢失，后面每级都用它设 VBAR_ELx。", w:{x19:0x40000800}});
S({ph:0, pc:A.start+4, code:"mrs x0, currentel", loc:"boot.S:30", desc:"读当前异常级别：CurrentEL 的 bits[3:2]，EL3 → 0b1100。", w:{x0:0xC}});
S({ph:0, pc:A.start+8, code:"lsr x0, x0, #2", loc:"boot.S:31", desc:"右移 2 位取出级别数值。", w:{x0:3}});
S({ph:0, pc:A.start+12, code:"and x0, x0, #3", loc:"boot.S:32", desc:"清掉高位，X0=3：当前在 EL3。", w:{x0:3}});
S({ph:0, pc:A.start+16, code:"cmp x0, #3; b.eq start_el3", loc:"boot.S:34", desc:"复位 EL=3，跳到 start_el3 分支（代码对 EL2/EL1 复位也做了分支，本场景走 EL3）。", to:{pc:A.el3}});

S({ph:1, pc:A.el3, code:"msr vbar_el3, x19", loc:"boot.S:46 start_el3", desc:"设置 EL3 向量表基址。本示例三级共用同一张表：VBAR_EL1 = VBAR_EL2 = VBAR_EL3 = vector_table。", w:{vbar_el3:0x40000800}});
S({ph:1, pc:A.el3+4, code:"ldr x2, =_stack_top_el3; mov sp, x2", loc:"boot.S:47-48", desc:"设置 EL3 栈。EL3 恒用 SP_EL3（即当前 sp）。每个 EL 有独立 4KB 栈，互不覆盖。", w:{x2:0x40004080, spEl3:0x40004080, sp:0x40004080}});
S({ph:1, pc:A.el3+12, code:"mov x2, #((1<<0)|(1<<8)|(1<<10))", loc:"boot.S:56", desc:"组装 SCR_EL3 的值：NS=1（下一级进 Non-secure）、HCE=1（允许 HVC）、RW=1（下一级 AArch64）。", w:{x2:0x501}});
S({ph:1, pc:A.el3+16, code:"msr scr_el3, x2", loc:"boot.S:57", desc:"写入 0x501；读回 0x531（bit4/bit5 为 QEMU 复位默认位）。SCR_EL3 决定 ERET 后的安全状态与执行态。", w:{scr_el3:0x531}});
S({ph:1, pc:A.el3+20, code:"mov x2, #0x3c9; msr spsr_el3, x2", loc:"boot.S:59-60", desc:"SPSR_EL3 = 0x3C9：目标状态 EL2h + DAIF 全屏蔽。ERET 时它会被整体装入 PSTATE。", w:{x2:0x3C9, spsr_el3:0x3C9}});
S({ph:1, pc:A.el3+24, code:"ldr x2, =start_el2; msr elr_el3, x2", loc:"boot.S:61-62", desc:"ELR_EL3 = 0x40000060：ERET 的返回地址，即 EL2 代码入口。", w:{x2:0x40000060, elr_el3:0x40000060}});
S({ph:1, pc:A.el3+36, code:"bl startup_banner", loc:"boot.S:64 → main.c:248", desc:"打印启动横幅与基本信息（C 函数，经 PL011 串口输出）。bl 把返回地址写入 X30（LR）。", to:{pc:0x40000054}, w:{x30:0x40000054}, logs:[
  L.blank(), L.banner("========== AArch64 异常模型验证 启动 =========="), L.blank(),
  L.kv("CPU:", "cortex-a53"),
  L.kv("Machine:", "virt (secure=on, virtualization=on)"),
  L.kv("Levels:", "EL0 - EL3"),
  L.kv("Tests:", "5 项（SVC / 链条 / BRK / UDF / IRQ）"),
  L.blank(), L.step(1,2,"引导降级 EL3 -> EL2 -> EL1 -> EL0")
]});
S({ph:1, pc:A.el3+40, code:"bl el3_report", loc:"boot.S:65 → main.c:259", desc:"报告 EL3 级别：打印 SCR_EL3 与 VBAR_EL3。", to:{pc:0x40000058}, w:{x30:0x40000058}, logs:[
  L.ln("[EL3]","当前级别 EL3（Secure 固件）"),
  L.ln("[REG]","SCR_EL3   = 0x0000000000000531"),
  L.ln("[REG]","VBAR_EL3  = 0x0000000040000800"),
  L.ln("[EL3]","设 SCR_EL3(NS=1,RW=1,HCE=1)，ERET 到 EL2（切换安全状态）")
]});
S({ph:1, pc:A.el3+44, code:"bl el3_setup", loc:"boot.S:66 → main.c:291", desc:"Secure EL3 专属职责：GICD_IGROUPR 等是 Secure-only 寄存器，必须趁还在 Secure 时配好，降级到 Non-secure 后就写不了了。把定时器中断 PPI30 划入 Group1 并使能 GIC。", to:{pc:0x4000005c}, w:{x30:0x4000005c, gicd_igroupr:0x40000000, gicd_ctlr:3, gicd_isenabler:0x40000000, gicc_ctlr:3, gicc_pmr:0xFF}, logs:[
  L.ln("[EL3]","GIC 已配置（PPI30 -> Group1，distributor/CPU interface 使能）")
]});
S({ph:1, pc:A.el3+48, code:"eret", loc:"boot.S:67", desc:"EL3 → EL2：PC←ELR_EL3，PSTATE←SPSR_EL3（EL2h）。因 SCR_EL3.NS=1，同时切换到 Non-secure 世界。异常返回只降级或同级，不升级。", to:{pc:A.el2, el:2, mode:"EL2h", daif:15, sp:0, secure:false}, ev:{kind:"eret", from:3, to:2, cause:"ERET（引导降级）"}});

S({ph:2, pc:A.el2, code:"msr vbar_el2, x19", loc:"boot.S:73 start_el2", desc:"设置 EL2 向量表（同一张表）。", w:{vbar_el2:0x40000800}});
S({ph:2, pc:A.el2+4, code:"ldr x2, =_stack_top_el2; mov sp, x2", loc:"boot.S:74-75", desc:"设置 EL2 栈 SP_EL2 = 0x40005080。", w:{x2:0x40005080, spEl2:0x40005080, sp:0x40005080}});
S({ph:2, pc:A.el2+12, code:"mov x2, #(1 << 31)", loc:"boot.S:81", desc:"组装 HCR_EL2 的值：只置 RW 位。", w:{x2:0x80000000}});
S({ph:2, pc:A.el2+16, code:"msr hcr_el2, x2", loc:"boot.S:82", desc:"HCR_EL2.RW=1：下一级（EL1）为 AArch64。TSC=0/HCD=0：SMC 不被 trap 到 EL2（直通 EL3）、HVC 可用。", w:{hcr_el2:0x80000000}});
S({ph:2, pc:A.el2+20, code:"mov x2, #0x3c5; msr spsr_el2, x2", loc:"boot.S:84-85", desc:"目标状态 EL1h + DAIF 全屏蔽。", w:{x2:0x3C5, spsr_el2:0x3C5}});
S({ph:2, pc:A.el2+24, code:"ldr x2, =start_el1; msr elr_el2, x2", loc:"boot.S:86-87", desc:"ELR_EL2 = 0x4000008C：ERET 目标为 start_el1。", w:{x2:0x4000008C, elr_el2:0x4000008C}});
S({ph:2, pc:A.el2+32, code:"bl el2_report", loc:"boot.S:89 → main.c:269", desc:"报告 EL2 级别：打印 HCR_EL2 与 VBAR_EL2。", to:{pc:0x40000088}, w:{x30:0x40000088}, logs:[
  L.ln("[EL2]","当前级别 EL2（Hypervisor）"),
  L.ln("[REG]","HCR_EL2   = 0x0000000080000000"),
  L.ln("[REG]","VBAR_EL2  = 0x0000000040000800"),
  L.ln("[EL2]","设 HCR_EL2(RW=1)，ERET 到 EL1")
]});
S({ph:2, pc:A.el2+40, code:"eret", loc:"boot.S:90", desc:"EL2 → EL1：进入“内核”级别。SP 切到 SP_EL1（尚未设置，为复位值 0）。", to:{pc:A.el1, el:1, mode:"EL1h", daif:15, sp:0}, ev:{kind:"eret", from:2, to:1, cause:"ERET（引导降级）"}});

S({ph:3, pc:A.el1, code:"msr vbar_el1, x19", loc:"boot.S:96 start_el1", desc:"设置 EL1 向量表。之后 EL0 的 SVC/BRK/UDF/IRQ 都会查这张表。", w:{vbar_el1:0x40000800}});
S({ph:3, pc:A.el1+4, code:"ldr x2, =_stack_top_el1; mov sp, x2", loc:"boot.S:97-98", desc:"设置 EL1 栈 SP_EL1 = 0x40006080（EL1h 下当前 sp 即 SP_EL1）。", w:{x2:0x40006080, spEl1:0x40006080, sp:0x40006080}});
S({ph:3, pc:A.el1+8, code:"ldr x2, =_stack_top_el0; msr sp_el0, x2", loc:"boot.S:99-100", desc:"提前设好 EL0 栈：SP_EL0 可从 EL1 访问，但 EL0 自己没有 msr 权限。", w:{x2:0x40007080, spEl0:0x40007080}});
S({ph:3, pc:A.el1+16, code:"mov x2, #(1 << 9); msr cntkctl_el1, x2", loc:"boot.S:104-105", desc:"CNTKCTL_EL1.EL0PTEN=1：允许 EL0 访问 EL1 物理定时器（cntp_*_el0），否则 EL0 一碰就 trap 到 EL1。", w:{x2:0x200, cntkctl_el1:0x200}});
S({ph:3, pc:A.el1+20, code:"mov x2, #0; msr spsr_el1, x2", loc:"boot.S:107-108", desc:"目标状态 EL0t + DAIF 不屏蔽（中断开放——后面 IRQ 演示必需）。", w:{x2:0, spsr_el1:0}});
S({ph:3, pc:A.el1+24, code:"ldr x2, =main; msr elr_el1, x2", loc:"boot.S:109-110", desc:"ELR_EL1 = 0x40002100：ERET 目标为 C 的 main 函数。", w:{x2:0x40002100, elr_el1:0x40002100}});
S({ph:3, pc:A.el1+40, code:"bl el1_report", loc:"boot.S:112 → main.c:279", desc:"报告 EL1 级别：打印 VBAR_EL1 与 SCTLR_EL1（复位值，本示例不改 MMU）。", to:{pc:0x400000bc}, w:{x30:0x400000bc}, logs:[
  L.ln("[EL1]","当前级别 EL1（内核）"),
  L.ln("[REG]","VBAR_EL1  = 0x0000000040000800"),
  L.ln("[REG]","SCTLR_EL1 = 0x0000000000c50838"),
  L.ln("[EL1]","设 SPSR_EL1=EL0t，ERET 到 EL0")
]});
S({ph:3, pc:A.el1+48, code:"eret", loc:"boot.S:113", desc:"EL1 → EL0：PC←ELR_EL1=main；SP 切到 SP_EL0=0x40007080；DAIF=0（中断开放）。到达应用层。", to:{pc:A.main, el:0, mode:"EL0t", daif:0, sp:0x40007080}, ev:{kind:"eret", from:1, to:0, cause:"ERET（引导降级）"}});

S({ph:4, pc:0x40002114, code:'bl log_current_level(0, "（应用）")', loc:"main.c:421 main", desc:"main 从 EL0 开始执行：报告当前级别。", to:{pc:0x40002118}, w:{x30:0x40002118}, logs:[L.ln("[EL0]","当前级别 EL0（应用）")]});
S({ph:4, pc:0x40002124, code:'bl log_step(2, 2, "异常演示")', loc:"main.c:423", desc:"进入第 2 阶段：异常演示。", to:{pc:0x40002128}, w:{x30:0x40002128}, logs:[L.blank(), L.step(2,2,"异常演示")]});
S({ph:4, pc:0x40002130, code:'bl log_test("SVC 系统调用")', loc:"main.c:425", desc:"测试项标题：SVC 系统调用。", to:{pc:0x40002134}, w:{x30:0x40002134}, logs:[L.blank(), L.test("SVC 系统调用（EL0 -> EL1 简单往返）")]});
S({ph:4, pc:0x40002134, code:"svc #0", loc:"main.c:426", desc:"EL0 主动发出同步异常（系统调用），请求进入 EL1。异常进入只能升级或同级：EL0→EL1。", to:{pc:0x40002134}, ev:{kind:"trigger", cause:"SVC #0"}});
S({ph:4, pc:0x40002134, hw:true, code:"【硬件】异常入口：EL0 → EL1", loc:"CPU 自动", desc:"硬件自动完成五件事：① PSTATE → SPSR_EL1（EL0t，NZCV=0x8）；② ELR_EL1 ← PC+4 = 0x40002138；③ ESR_EL1 ← EC=0x15(SVC)、imm=0；④ 按 VBAR_EL1+0x400（来自低 EL、AArch64、同步）取向量；⑤ 切到 EL1h，SP←SP_EL1，DAIF 屏蔽。", w:{spsr_el1:0x80000000, elr_el1:0x40002138, esr_el1:0x54000000}, to:{pc:0x40000C00, el:1, mode:"EL1h", daif:15, sp:0x40006080}, ev:{kind:"exc", from:0, to:1, level:1, vec:0x400, cause:"SVC #0"}});
S({ph:4, pc:0x40000C00, code:"b lower_el_sync_handler", loc:"vector_table.S:39 (+0x400)", desc:"向量槽 +0x400 的第一条指令：跳转到共享的同步异常处理器（SVC/HVC/SMC/BRK/UDF 都从这进）。", to:{pc:A.sync}});
S({ph:4, pc:A.sync, code:"save_all（12 × stp/str）", loc:"vector_table.S:86 save_all", desc:"保存 x0–x30 到 EL1 栈：SP_EL1 0x40006080 → 0x40005FC0（-192 字节）。中断是异步的，现场必须全保存。", to:{sp:0x40005FC0}});
S({ph:4, pc:0x4000102c, code:"mrs x0, currentel; lsr; and; mov x19, x0", loc:"vector_table.S:90-93", desc:"在处理器内部确认当前级别 = 1（EL1）。", w:{x0:1}});
S({ph:4, pc:0x4000104c, code:"mrs x1, esr_el1; x2, elr_el1; x3, spsr_el1", loc:"vector_table.S:101-103", desc:"按当前级别读异常上下文三件套（ESR/ELR/SPSR）到参数寄存器。", w:{x1:0x54000000, x2:0x40002138, x3:0x80000000}});
S({ph:4, pc:0x40001078, code:"bl dispatch_sync(level=1, esr, elr, spsr)", loc:"vector_table.S:115 → main.c:305", desc:"C 分发器：识别 EC=0x15 普通 SVC，返回 0（ELR 已指向下一条指令，无需跳过）。", to:{pc:0x4000107c}, w:{x30:0x4000107c, x0:0}, logs:[
  L.ln("[SYNC]","到达 EL1：EC=0x15 (SVC)  imm=0x00"),
  L.ln("[REG]","ELR_ELx = 0x0000000040002138"),
  L.ln("[REG]","SPSR_ELx = 0x0000000080000000"),
  L.ln("[EL1]","普通 SVC，直接 ERET 返回 EL0")
]});
S({ph:4, pc:0x4000107c, code:"cbz x0, 4f", loc:"vector_table.S:121", desc:"返回值 0 → 跳过 ELR+4 修正段，直接恢复现场。", to:{pc:0x400010c0}});
S({ph:4, pc:0x400010c0, code:"restore_all（11 × ldp/ldr）", loc:"vector_table.S:140 restore_all", desc:"从栈恢复 x0–x30，SP_EL1 复位回 0x40006080。", to:{sp:0x40006080}});
S({ph:4, pc:0x400010ec, code:"eret", loc:"vector_table.S:141", desc:"EL1 → EL0：PSTATE←SPSR_EL1（EL0t、中断开放），PC←ELR_EL1=0x40002138。正好回到 svc 的下一条指令。", to:{pc:0x40002138, el:0, mode:"EL0t", daif:0, sp:0x40007080}, ev:{kind:"eret", from:1, to:0, cause:"ERET（SVC 返回）"}});
S({ph:4, pc:0x4000214c, code:'bl log_line("[OK]", "SVC 往返完成")', loc:"main.c:427", desc:"打印 OK：一次完整的“进入异常→处理→ERET 返回”。", to:{pc:0x40002150}, w:{x30:0x40002150}, logs:[L.ln("[OK]","SVC 往返完成，回到 EL0")]});

