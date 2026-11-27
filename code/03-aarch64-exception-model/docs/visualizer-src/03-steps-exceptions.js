S({ph:5, pc:0x40002158, code:'bl log_test("SVC/HVC/SMC 链条")', loc:"main.c:429", desc:"第二项测试：三层链条。", to:{pc:0x4000215c}, w:{x30:0x4000215c}, logs:[L.blank(), L.test("SVC/HVC/SMC 链条（EL0 -> EL1 -> EL2 -> EL3）")]});
S({ph:5, pc:0x4000215c, code:"svc #0x42", loc:"main.c:430", desc:"imm=0x42 是“链条”标记：EL1 收到后将转发 HVC 到 EL2，EL2 再转发 SMC 到 EL3——嵌套异常。", to:{pc:0x4000215c}, ev:{kind:"trigger", cause:"SVC #0x42"}});
S({ph:5, pc:0x4000215c, hw:true, code:"【硬件】异常入口：EL0 → EL1", loc:"CPU 自动", desc:"与上次 SVC 相同：ESR_EL1←0x54000042（EC=0x15、imm=0x42），ELR_EL1←0x40002160，SPSR_EL1 保存 EL0t。查 VBAR_EL1+0x400。", w:{spsr_el1:0x80000000, elr_el1:0x40002160, esr_el1:0x54000042}, to:{pc:0x40000C00, el:1, mode:"EL1h", daif:15, sp:0x40006080}, ev:{kind:"exc", from:0, to:1, level:1, vec:0x400, cause:"SVC #0x42"}});
S({ph:5, pc:0x40000C00, code:"b lower_el_sync_handler", loc:"vector_table.S:39 (+0x400)", desc:"进入共享同步异常处理器。", to:{pc:A.sync}});
S({ph:5, pc:A.sync, code:"save_all", loc:"vector_table.S:86", desc:"保存 EL0 的现场到 EL1 栈：SP_EL1 → 0x40005FC0。", to:{sp:0x40005FC0}});
S({ph:5, pc:0x4000102c, code:"mrs x0, currentel（=1）", loc:"vector_table.S:90-93", desc:"当前级别 EL1。", w:{x0:1}});
S({ph:5, pc:0x4000104c, code:"mrs x1/x2/x3, esr/elr/spsr_el1", loc:"vector_table.S:101-103", desc:"读 EL1 的异常上下文。", w:{x1:0x54000042, x2:0x40002160, x3:0x80000000}});
S({ph:5, pc:0x40001078, code:"bl dispatch_sync(level=1, ...)", loc:"vector_table.S:115 → main.c:327-330", desc:"C 分发器看到 imm=0x42：这是“链条”标记，EL1 转发 HVC 到 EL2。", to:{pc:0x4000107c}, w:{x30:0x4000107c}, logs:[
  L.ln("[SYNC]","到达 EL1：EC=0x15 (SVC)  imm=0x42"),
  L.ln("[REG]","ELR_ELx = 0x0000000040002160"),
  L.ln("[REG]","SPSR_ELx = 0x0000000080000000"),
  L.ln("[EL1]","转发 HVC 到 EL2 ...")
]});
S({ph:5, pc:0x40001e64, code:"hvc #0", loc:"main.c:330 dispatch_sync（EL1 运行）", desc:"EL1 在处理 SVC 的过程中主动发 HVC 升到 EL2——异常可以嵌套：此刻 EL0 的现场在 EL1 栈上，EL1 的上下文又将被保存进 EL2 栈。", to:{pc:0x40001e64}, ev:{kind:"trigger", cause:"HVC #0"}});
S({ph:5, pc:0x40001e64, hw:true, code:"【硬件】异常入口：EL1 → EL2", loc:"CPU 自动", desc:"保存 EL1h 状态（SPSR_EL2=0x800003C5）；ELR_EL2←0x40001E68（hvc 的下一条）；ESR_EL2←EC=0x16(HVC)。查 VBAR_EL2+0x400——三级共用同一张表、同一偏移。", w:{spsr_el2:0x800003C5, elr_el2:0x40001E68, esr_el2:0x58000000}, to:{pc:0x40000C00, el:2, mode:"EL2h", daif:15, sp:0x40005080}, ev:{kind:"exc", from:1, to:2, level:2, vec:0x400, cause:"HVC #0"}});
S({ph:5, pc:0x40000C00, code:"b lower_el_sync_handler", loc:"vector_table.S:39 (+0x400)", desc:"EL2 也从 +0x400 槽进入同一个共享处理器。", to:{pc:A.sync}});
S({ph:5, pc:A.sync, code:"save_all", loc:"vector_table.S:86", desc:"保存 EL1 的现场到 EL2 栈：SP_EL2 0x40005080 → 0x40004FC0。", to:{sp:0x40004FC0}});
S({ph:5, pc:0x4000102c, code:"mrs x0, currentel（=2）", loc:"vector_table.S:90-93", desc:"当前级别 EL2。", w:{x0:2}});
S({ph:5, pc:0x4000104c, code:"mrs x1/x2/x3, esr/elr/spsr_el2", loc:"vector_table.S:105-109", desc:"读 EL2 的异常上下文（走 b.eq 2f 的 EL2 分支）。", w:{x1:0x58000000, x2:0x40001E68, x3:0x800003C5}});
S({ph:5, pc:0x40001078, code:"bl dispatch_sync(level=2, ...)", loc:"vector_table.S:115 → main.c:345-349", desc:"EL2 的分发器：收到 HVC，继续转发 SMC 到 EL3。", to:{pc:0x4000107c}, w:{x30:0x4000107c}, logs:[
  L.ln("[SYNC]","到达 EL2：EC=0x16 (HVC)  imm=0x00"),
  L.ln("[REG]","ELR_ELx = 0x0000000040001e68"),
  L.ln("[REG]","SPSR_ELx = 0x00000000800003c5"),
  L.ln("[EL2]","转发 SMC 到 EL3 ...")
]});
S({ph:5, pc:0x40001cd4, code:"smc #0", loc:"main.c:348 dispatch_sync（EL2 运行）", desc:"EL2 转发 SMC：从 Non-secure EL2 请求进入 Secure EL3。HCR_EL2.TSC=0 保证 SMC 不被 trap 到 EL2，直通 EL3。", to:{pc:0x40001cd4}, ev:{kind:"trigger", cause:"SMC #0"}});
S({ph:5, pc:0x40001cd4, hw:true, code:"【硬件】异常入口：EL2 → EL3", loc:"CPU 自动", desc:"保存 EL2h（SPSR_EL3=0x800003C9）；ELR_EL3←0x40001CD8；ESR_EL3←EC=0x17(SMC)。查 VBAR_EL3+0x400。同时切回 Secure 世界。", w:{spsr_el3:0x800003C9, elr_el3:0x40001CD8, esr_el3:0x5C000000}, to:{pc:0x40000C00, el:3, mode:"EL3h", daif:15, sp:0x40004080, secure:true}, ev:{kind:"exc", from:2, to:3, level:3, vec:0x400, cause:"SMC #0"}});
S({ph:5, pc:0x40000C00, code:"b lower_el_sync_handler", loc:"vector_table.S:39 (+0x400)", desc:"EL3 从同一张表的 +0x400 槽进入。", to:{pc:A.sync}});
S({ph:5, pc:A.sync, code:"save_all", loc:"vector_table.S:86", desc:"保存 EL2 的现场到 EL3 栈：SP_EL3 0x40004080 → 0x40003FC0。", to:{sp:0x40003FC0}});
S({ph:5, pc:0x4000102c, code:"mrs x0, currentel（=3）", loc:"vector_table.S:90-93", desc:"当前级别 EL3。", w:{x0:3}});
S({ph:5, pc:0x4000104c, code:"mrs x1/x2/x3, esr/elr/spsr_el3", loc:"vector_table.S:110-114", desc:"读 EL3 的异常上下文（走 b.eq 1f 的 EL3 分支）。", w:{x1:0x5C000000, x2:0x40001CD8, x3:0x800003C9}});
S({ph:5, pc:0x40001078, code:"bl dispatch_sync(level=3, ...)", loc:"vector_table.S:115 → main.c:351-354", desc:"链条终点：EL3 报告“到达终点”，之后逐级 ERET 返回。返回 0（SMC 的 ELR 已是下一条）。", to:{pc:0x4000107c}, w:{x30:0x4000107c, x0:0}, logs:[
  L.ln("[SYNC]","到达 EL3：EC=0x17 (SMC)  imm=0x00"),
  L.ln("[REG]","ELR_ELx = 0x0000000040001cd8"),
  L.ln("[REG]","SPSR_ELx = 0x00000000800003c9"),
  L.ln("[EL3]","到达终点，开始逐级 ERET 返回")
]});
S({ph:5, pc:0x4000107c, code:"cbz x0, 4f", loc:"vector_table.S:121", desc:"返回 0 → 无需 ELR+4。", to:{pc:0x400010c0}});
S({ph:5, pc:0x400010c0, code:"restore_all", loc:"vector_table.S:140", desc:"恢复 EL2 的现场，SP_EL3 复位回 0x40004080。", to:{sp:0x40004080}});
S({ph:5, pc:0x400010ec, code:"eret", loc:"vector_table.S:141", desc:"EL3 → EL2：回到 0x40001CD8（EL2 的 dispatch_sync 中 smc 的下一条）。因 SCR_EL3.NS=1，切回 Non-secure。", to:{pc:0x40001CD8, el:2, mode:"EL2h", daif:15, sp:0x40005080, secure:false}, ev:{kind:"eret", from:3, to:2, cause:"ERET（链条回程 1/3）"}, logs:[L.ln("[RET]","从 EL3 返回到 EL2")]});
S({ph:5, pc:0x40001CD8, code:"dispatch_sync(EL2) 收尾：cbz; restore_all", loc:"main.c:349 → vector_table.S:121,140", desc:"EL2 的分发器打印返回信息后返回 0，恢复现场，准备 ERET 回 EL1。", to:{sp:0x40005080}});
S({ph:5, pc:0x400010ec, code:"eret", loc:"vector_table.S:141", desc:"EL2 → EL1：回到 0x40001E68（EL1 的 dispatch_sync 中 hvc 的下一条）。", to:{pc:0x40001E68, el:1, mode:"EL1h", daif:15, sp:0x40006080}, ev:{kind:"eret", from:2, to:1, cause:"ERET（链条回程 2/3）"}, logs:[L.ln("[RET]","从 EL3 逐级返回到 EL1")]});
S({ph:5, pc:0x40001E68, code:"dispatch_sync(EL1) 收尾：cbz; restore_all", loc:"main.c:331 → vector_table.S:121,140", desc:"EL1 的分发器收尾，恢复 EL0 现场，准备最后一次 ERET。", to:{sp:0x40006080}});
S({ph:5, pc:0x400010ec, code:"eret", loc:"vector_table.S:141", desc:"EL1 → EL0：链条走完，回到 0x40002160（EL0 的 svc #0x42 下一条）。", to:{pc:0x40002160, el:0, mode:"EL0t", daif:0, sp:0x40007080}, ev:{kind:"eret", from:1, to:0, cause:"ERET（链条回程 3/3）"}});
S({ph:5, pc:0x4000216c, code:'bl log_line("[OK]", "链条完成")', loc:"main.c:431", desc:"三层链条完成：进入只升（SVC→HVC→SMC），返回只降（ERET×3）。", to:{pc:0x40002170}, w:{x30:0x40002170}, logs:[L.ln("[OK]","链条完成，回到 EL0")]});

S({ph:6, pc:0x40002178, code:'bl log_test("BRK 调试异常")', loc:"main.c:433", desc:"第三项测试：BRK 软件断点。", to:{pc:0x4000217c}, w:{x30:0x4000217c}, logs:[L.blank(), L.test("BRK 调试异常")]});
S({ph:6, pc:0x4000217c, code:"brk #0", loc:"main.c:434", desc:"软件断点指令，EC=0x3C。关键差异：BRK 的 ELR 指向触发指令本身，而不是下一条！", to:{pc:0x4000217c}, ev:{kind:"trigger", cause:"BRK #0"}});
S({ph:6, pc:0x4000217c, hw:true, code:"【硬件】异常入口：EL0 → EL1", loc:"CPU 自动", desc:"ESR_EL1=0xF2000000（EC=0x3C、IL=1、imm=0）；ELR_EL1=0x4000217C——指向 brk 指令本身；SPSR_EL1 保存 EL0t。查 VBAR_EL1+0x400。", w:{spsr_el1:0x80000000, elr_el1:0x4000217C, esr_el1:0xF2000000}, to:{pc:0x40000C00, el:1, mode:"EL1h", daif:15, sp:0x40006080}, ev:{kind:"exc", from:0, to:1, level:1, vec:0x400, cause:"BRK #0"}});
S({ph:6, pc:0x40000C00, code:"b lower_el_sync_handler", loc:"vector_table.S:39 (+0x400)", desc:"进入共享同步异常处理器。", to:{pc:A.sync}});
S({ph:6, pc:A.sync, code:"save_all", loc:"vector_table.S:86", desc:"保存现场：SP_EL1 → 0x40005FC0。", to:{sp:0x40005FC0}});
S({ph:6, pc:0x4000102c, code:"mrs x0, currentel（=1）", loc:"vector_table.S:90-93", desc:"当前级别 EL1。", w:{x0:1}});
S({ph:6, pc:0x4000104c, code:"mrs x1/x2/x3, esr/elr/spsr_el1", loc:"vector_table.S:101-103", desc:"读 EL1 异常上下文。", w:{x1:0xF2000000, x2:0x4000217C, x3:0x80000000}});
S({ph:6, pc:0x40001078, code:"bl dispatch_sync(level=1, ...)", loc:"vector_table.S:115 → main.c:335-337", desc:"识别 EC=0x3C 为 BRK 调试异常，返回 1：要求把 ELR+4 跳过触发指令。", to:{pc:0x4000107c}, w:{x30:0x4000107c, x0:1}, logs:[
  L.ln("[SYNC]","到达 EL1：EC=0x3c (BRK)  imm=0x00"),
  L.ln("[REG]","ELR_ELx = 0x000000004000217c"),
  L.ln("[REG]","SPSR_ELx = 0x0000000080000000"),
  L.ln("[EL1]","BRK 调试异常，ELR+4 跳过该指令")
]});
S({ph:6, pc:0x4000107c, code:"cbz x0, 4f（x0=1，走 ELR+4 段）", loc:"vector_table.S:121", desc:"返回值 1 ≠ 0：不跳过修正段，进入 ELR+4 逻辑。", to:{pc:0x40001094}});
S({ph:6, pc:0x40001094, code:"mrs x5, elr_el1; add x5, x5, #4; msr elr_el1, x5", loc:"vector_table.S:123-131", desc:"ELR_EL1：0x4000217C → 0x40002180，跳过 brk 指令本身。否则 ERET 后会重新执行同一条 brk，再次触发异常，陷入死循环。", w:{x4:4, x5:0x40002180, elr_el1:0x40002180}});
S({ph:6, pc:0x400010c0, code:"restore_all", loc:"vector_table.S:140", desc:"恢复现场：SP_EL1 → 0x40006080。", to:{sp:0x40006080}});
S({ph:6, pc:0x400010ec, code:"eret", loc:"vector_table.S:141", desc:"EL1 → EL0：PC←修正后的 ELR_EL1=0x40002180（跳过了 brk）。", to:{pc:0x40002180, el:0, mode:"EL0t", daif:0, sp:0x40007080}, ev:{kind:"eret", from:1, to:0, cause:"ERET（BRK 返回，ELR 已 +4）"}});
S({ph:6, pc:0x4000218c, code:'bl log_line("[OK]", "BRK 处理完成")', loc:"main.c:435", desc:"BRK 演示完成。", to:{pc:0x40002190}, w:{x30:0x40002190}, logs:[L.ln("[OK]","BRK 处理完成，回到 EL0")]});

S({ph:7, pc:0x40002198, code:'bl log_test("UDF 无效指令")', loc:"main.c:437", desc:"第四项测试：UDF 无效指令（编码 0x00000000）。", to:{pc:0x4000219c}, w:{x30:0x4000219c}, logs:[L.blank(), L.test("UDF 无效指令")]});
S({ph:7, pc:0x4000219c, code:"udf #0（编码 0x00000000）", loc:"main.c:438", desc:"非法指令触发 UNDEFINED 同步异常，EC=0x00。与 BRK 一样，ELR 指向触发指令本身。", to:{pc:0x4000219c}, ev:{kind:"trigger", cause:"UDF #0"}});
S({ph:7, pc:0x4000219c, hw:true, code:"【硬件】异常入口：EL0 → EL1", loc:"CPU 自动", desc:"ESR_EL1=0x02000000（EC=0x00、IL=1、ISS=指令编码 0）；ELR_EL1=0x4000219C（指向 udf 本身）。查 VBAR_EL1+0x400。", w:{spsr_el1:0x80000000, elr_el1:0x4000219C, esr_el1:0x02000000}, to:{pc:0x40000C00, el:1, mode:"EL1h", daif:15, sp:0x40006080}, ev:{kind:"exc", from:0, to:1, level:1, vec:0x400, cause:"UDF #0"}});
S({ph:7, pc:0x40000C00, code:"b lower_el_sync_handler", loc:"vector_table.S:39 (+0x400)", desc:"进入共享同步异常处理器。", to:{pc:A.sync}});
S({ph:7, pc:A.sync, code:"save_all", loc:"vector_table.S:86", desc:"保存现场：SP_EL1 → 0x40005FC0。", to:{sp:0x40005FC0}});
S({ph:7, pc:0x4000102c, code:"mrs x0, currentel（=1）", loc:"vector_table.S:90-93", desc:"当前级别 EL1。", w:{x0:1}});
S({ph:7, pc:0x4000104c, code:"mrs x1/x2/x3, esr/elr/spsr_el1", loc:"vector_table.S:101-103", desc:"读 EL1 异常上下文。", w:{x1:0x02000000, x2:0x4000219C, x3:0x80000000}});
S({ph:7, pc:0x40001078, code:"bl dispatch_sync(level=1, ...)", loc:"vector_table.S:115 → main.c:338-340", desc:"识别 EC=0x00 无效指令，返回 1：同样要求 ELR+4。", to:{pc:0x4000107c}, w:{x30:0x4000107c, x0:1}, logs:[
  L.ln("[SYNC]","到达 EL1：EC=0x00 (UNDEFINED)  imm=0x00"),
  L.ln("[REG]","ELR_ELx = 0x000000004000219c"),
  L.ln("[REG]","SPSR_ELx = 0x0000000080000000"),
  L.ln("[EL1]","无效指令，ELR+4 跳过该指令")
]});
S({ph:7, pc:0x4000107c, code:"cbz x0, 4f（x0=1，走 ELR+4 段）", loc:"vector_table.S:121", desc:"返回 1：进入 ELR+4 修正。", to:{pc:0x40001094}});
S({ph:7, pc:0x40001094, code:"mrs x5, elr_el1; add x5, x5, #4; msr elr_el1, x5", loc:"vector_table.S:123-131", desc:"ELR_EL1：0x4000219C → 0x400021A0，跳过 udf 指令。", w:{x4:4, x5:0x400021A0, elr_el1:0x400021A0}});
S({ph:7, pc:0x400010c0, code:"restore_all", loc:"vector_table.S:140", desc:"恢复现场。", to:{sp:0x40006080}});
S({ph:7, pc:0x400010ec, code:"eret", loc:"vector_table.S:141", desc:"EL1 → EL0：PC←0x400021A0（跳过了 udf）。", to:{pc:0x400021A0, el:0, mode:"EL0t", daif:0, sp:0x40007080}, ev:{kind:"eret", from:1, to:0, cause:"ERET（UDF 返回，ELR 已 +4）"}});
S({ph:7, pc:0x400021ac, code:'bl log_line("[OK]", "无效指令处理完成")', loc:"main.c:439", desc:"UDF 演示完成。", to:{pc:0x400021b0}, w:{x30:0x400021b0}, logs:[L.ln("[OK]","无效指令处理完成，回到 EL0")]});

S({ph:8, pc:0x400021b8, code:'bl log_test("IRQ 定时器中断")', loc:"main.c:441", desc:"第五项测试：异步异常（中断）。", to:{pc:0x400021bc}, w:{x30:0x400021bc}, logs:[L.blank(), L.test("IRQ 定时器中断（异步）")]});
S({ph:8, pc:0x400021bc, code:"mov w0, #0x20000", loc:"main.c:410 timer_irq_demo", desc:"准备定时器装载值 131072。", w:{x0:0x20000}});
S({ph:8, pc:0x400021c0, code:"msr cntp_tval_el0, x0", loc:"main.c:410", desc:"装载定时器：CNTP_TVAL=0x20000，此后每个 tick 自动递减，减到 0 断言中断。", w:{cntp_tval:0x20000}});
S({ph:8, pc:0x400021c4, code:"mov w0, #0x1", loc:"main.c:411", desc:"准备控制字。", w:{x0:1}});
S({ph:8, pc:0x400021c8, code:"msr cntp_ctl_el0, x0", loc:"main.c:411", desc:"使能定时器（ENABLE=1、IMASK=0）。中断将异步到达——何时到、打断哪条指令都不确定，这是与同步异常的本质区别。", w:{cntp_ctl:1}});
S({ph:8, pc:0x400021dc, code:'bl log_line("[ASYNC]", "等待异步中断...")', loc:"main.c:412", desc:"EL0 开始忙等 g_irq_ok 置位。", to:{pc:0x400021e0}, w:{x30:0x400021e0}, logs:[L.ln("[ASYNC]","等待异步中断（定时器 IRQ -> EL1 +0x480）...")]});
S({ph:8, pc:0x400021e8, code:"忙等循环：ldr w0, [g_irq_ok]; cbz w0, loop", loc:"main.c:413-415", desc:"读 g_irq_ok=0，继续循环。CNTP_TVAL 递减中（0x18000）。", w:{x0:0, cntp_tval:0x18000}, to:{pc:0x400021e8}});
S({ph:8, pc:0x400021e8, code:"忙等循环：ldr w0, [g_irq_ok]; cbz w0, loop", loc:"main.c:413-415", desc:"继续等待。CNTP_TVAL=0x6000，快到 0 了。", w:{x0:0, cntp_tval:0x6000}, to:{pc:0x400021e8}});
S({ph:8, pc:0x400021e8, code:"忙等循环：ldr w0, [g_irq_ok]; cbz w0, loop", loc:"main.c:413-415", desc:"继续等待。CNTP_TVAL 减到 0：比较器条件满足，CNTP_CTL.ISTATUS 置 1，定时器输出断言！", w:{x0:0, cntp_tval:0, cntp_ctl:5}, to:{pc:0x400021e8}});
S({ph:8, pc:0x400021ec, hw:true, code:"【硬件】IRQ #30 到达（异步抢占）", loc:"定时器 + GIC", desc:"CNTP_TVAL 归零 → CNTP_CTL.ISTATUS=1 → EL1 物理定时器断言 PPI30 → GIC 分发器 → CPU interface 拉高 IRQ 线。EL0 的 PSTATE.I=0（中断未屏蔽）→ 立即抢占当前指令。同步异常是“指令主动触发”，异步异常是“不请自来”。", to:{pc:0x400021ec}, ev:{kind:"irq", cause:"IRQ #30（PPI 定时器）"}});
S({ph:8, pc:0x400021ec, hw:true, code:"【硬件】异常入口（IRQ）：EL0 → EL1", loc:"CPU 自动", desc:"异步入口动作：① PSTATE → SPSR_EL1（EL0t）；② ELR_EL1 ← 当前 PC=0x400021EC（停在循环处，而非“下一条”）；③ IRQ 不写 ESR；④ 查 VBAR_EL1+0x480（低 EL、AArch64、IRQ 槽）；⑤ 切 EL1h、SP_EL1、DAIF 屏蔽（防嵌套）。", w:{spsr_el1:0x80000000, elr_el1:0x400021EC}, to:{pc:0x40000C80, el:1, mode:"EL1h", daif:15, sp:0x40006080}, ev:{kind:"exc", from:0, to:1, level:1, vec:0x480, cause:"IRQ #30"}});
S({ph:8, pc:0x40000C80, code:"b lower_el_irq_handler", loc:"vector_table.S:40 (+0x480)", desc:"IRQ 槽（+0x480）跳转到 IRQ 专用处理器（与同步 +0x400 不同入口）。", to:{pc:A.irqh}});
S({ph:8, pc:A.irqh, code:"save_all", loc:"vector_table.S:144", desc:"保存 EL0 现场：SP_EL1 0x40006080 → 0x40005FC0。", to:{sp:0x40005FC0}});
S({ph:8, pc:0x4000111c, code:"bl irq_handler_c", loc:"vector_table.S:146 → main.c:362", desc:"中断处理：读 GICC_IAR 确认中断（ID=30 即 PPI30）；写 GICC_EOIR 结束；关闭定时器（电平触发，不关会立刻重新断言）；置 g_irq_ok=1 通知 EL0。", to:{pc:0x40001120}, w:{x30:0x40001120, gicc_iar:0x1E, gicc_eoir:0x1E, cntp_ctl:0, g_irq_ok:1}, logs:[
  L.ln("[IRQ]","GICC_IAR=0x01e  中断ID=0x1e (EL1 物理定时器)")
]});
S({ph:8, pc:0x40001120, code:"restore_all", loc:"vector_table.S:147", desc:"恢复 EL0 现场：SP_EL1 → 0x40006080。", to:{sp:0x40006080}});
S({ph:8, pc:0x4000114c, code:"eret", loc:"vector_table.S:148", desc:"EL1 → EL0：PC←ELR_EL1=0x400021EC（被打断的循环处），PSTATE 恢复 DAIF 开放。", to:{pc:0x400021ec, el:0, mode:"EL0t", daif:0, sp:0x40007080}, ev:{kind:"eret", from:1, to:0, cause:"ERET（IRQ 返回）"}});
S({ph:8, pc:0x400021ec, code:"ldr w0,[g_irq_ok]=1；退出忙等；bl log_line", loc:"main.c:443", desc:"g_irq_ok=1 → cbz 不成立，跳出忙等，打印 OK。", to:{pc:0x40002200}, w:{x0:1, x30:0x40002200}, logs:[L.ln("[OK]","IRQ 处理完成，回到 EL0")]});

S({ph:9, pc:0x40002208, code:'bl log_banner("全部验证完成")', loc:"main.c:445", desc:"收尾横幅。", to:{pc:0x4000220c}, w:{x30:0x4000220c}, logs:[
  L.blank(), L.banner("========== 全部验证完成 (EL0<->EL1<->EL2<->EL3、同步/异步异常) =========="), L.blank()
]});
S({ph:9, pc:0x4000220c, code:"wfe", loc:"main.c:447", desc:"等待事件指令：低功耗挂起。", to:{pc:0x4000220c}});
S({ph:9, pc:0x40002214, code:"b 0x4000220c（无限循环）", loc:"main.c:448", desc:"演示结束：真实程序在此无限循环（QEMU 中按 Ctrl-C 退出）。可视化到此为止。", to:{pc:0x4000220c}, ev:{kind:"end"}});

