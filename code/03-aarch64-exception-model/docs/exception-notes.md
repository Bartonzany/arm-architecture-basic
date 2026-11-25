# AArch64 异常模型 —— 概念、实现与结果印证

本文档按《AArch64 异常模型（中文精读版）》章节组织，把**文段内容**、
**代码实现**、**执行结果**、**因果解释**串在一起：每一节先讲文档这段话说了
什么机制，再给出对应的代码（全部在 `src/bare-metal/` 一个文件夹里），最后用
真实执行结果解释“为什么长这样”。

代码位置（唯一文件夹）：
- `src/bare-metal/boot.S`         —— EL3 -> EL2 -> EL1 -> EL0 降级引导
- `src/bare-metal/vector_table.S` —— 16 入口共享向量表 + 异常处理器
- `src/bare-metal/main.c`         —— 各级别报告 + 同步/异步异常演示
- `src/bare-metal/linker.ld`      —— 链接脚本（EL0/EL1/EL2/EL3 各 4KB 栈）

运行方式：

```bash
make -C src/bare-metal run
# QEMU: -machine virt,secure=on,virtualization=on -cpu cortex-a53 -kernel kernel.elf
```

关键点：`secure=on,virtualization=on` 让 CPU 实现 EL3/EL2 并**复位进入 EL3**，
裸机从 EL3 开始逐级降级，最终在 EL0 完成各异常演示。

---

## 一、特权与异常级别（§1–§2）

### 1.1 四个异常级别 EL0–EL3

**文段**：数字越大特权越高；EL0=应用、EL1=内核、EL2=hypervisor、EL3=固件。
EL0/EL1 强制实现，EL2/EL3 可选。

**代码**（`boot.S`）：复位进入 EL3 后逐级 `ERET` 降级，每级调用 `*_report`
打印自身级别与关键寄存器：

```text
[EL3 ]当前级别 EL3（Secure 固件）
[REG ] SCR_EL3   = 0x0000000000000531
[EL2 ]当前级别 EL2（Hypervisor）
[REG ] HCR_EL2   = 0x0000000080000000
[EL1 ]当前级别 EL1（内核）
[REG ] SCTLR_EL1 = 0x0000000000c50838
[EL0 ]当前级别 EL0（应用）
```

**为什么**：印证“四个级别各司其职”，并且演示了 EL 只在复位/异常进出时改变。

### 1.2 EL 如何跳转

**文段**：EL 只在进入异常/异常返回/复位时改变；升 EL 靠进入异常（SVC→EL1、
HVC→EL2、SMC→EL3），降 EL 靠 `ERET`。硬规则：进入异常只升不降、返回只降不升。

**代码**（`main.c` 的 dispatch_sync + `vector_table.S`）：

```c
__asm__ volatile("svc #0x42");   // EL0 -> EL1
// EL1 处理器内：hvc #0          // EL1 -> EL2
// EL2 处理器内：smc #0          // EL2 -> EL3
// 各级处理器末尾：eret          // EL3 -> EL2 -> EL1 -> EL0
```

**结果**：

```text
[SYNC] 到达 EL1：EC=0x15 (SVC)  imm=0x42
[EL1 ] 转发 HVC 到 EL2 ...
[SYNC] 到达 EL2：EC=0x16 (HVC)  imm=0x00
[EL2 ] 转发 SMC 到 EL3 ...
[SYNC] 到达 EL3：EC=0x17 (SMC)  imm=0x00
[EL3 ] 到达终点，开始逐级 ERET 返回
[RET ] 从 EL3 返回到 EL2
[RET ] 从 EL3 逐级返回到 EL1
```

**为什么**：一条 `svc` 就串起了完整的 EL0→EL1→EL2→EL3 上升链，再靠三次
`eret` 逐级下降，完美印证“升靠异常、降靠 ERET”。

### 1.3 寄存器访问特权

**文段**：系统寄存器带 `_ELx` 后缀表示访问需的最低 EL（`SCTLR_EL1` 需 EL1+）。

**代码**（`main.c`）：`el3_report/el2_report/el1_report` 分别在对应级别 `mrs` 读
`SCR_EL3`、`HCR_EL2`、`SCTLR_EL1`。这些 `mrs` 在低于对应级别执行会触发异常。

**为什么**：各级别报告能成功读到自己的寄存器，说明“当前 EL 满足访问门槛”。

---

## 二、执行状态与安全状态（§3）

### 2.1 执行状态 AArch32/AArch64

**文段**：每个 EL 的执行状态由更高一级控制：EL1 看 `HCR_EL2.RW`，EL2 看
`SCR_EL3.RW`。

**代码**（`boot.S`）：降级时配置

```asm
mov x2, #((1<<0)|(1<<8)|(1<<10))   ; SCR_EL3：NS=1、HCE=1、RW=1
msr scr_el3, x2                    ; EL2 = AArch64
mov x2, #(1<<31)                   ; HCR_EL2：RW=1
msr hcr_el2, x2                    ; EL1 = AArch64
```

**结果**：`HCR_EL2=0x80000000`、`SCR_EL3=0x531`（含 RW=1），本示例全程 AArch64。

### 2.2 安全状态 Secure/Non-secure

**文段**：跨安全状态必经 EL3；`SCR_EL3.NS` 决定从 EL3 返回时是否进入 Non-secure。

**代码**（`boot.S`）：`SCR_EL3.NS=1` 后 `eret`，EL2/EL1/EL0 全部进入 Non-secure。

**为什么**：印证“EL3 是安全状态的看门人”，且“切换安全状态必须经过 EL3”。

---

## 三、同步异常（§5）

### 3.1 无效指令（UDF）

**文段**（§5.1）：执行无效指令产生 UNDEFINED 异常（EC=0x00）。

**代码**（`main.c`）：

```c
__asm__ volatile("udf #0");
```

**结果**：

```text
[SYNC] 到达 EL1：EC=0x00 (UNDEFINED)  imm=0x00
[EL1 ] 无效指令，ELR+4 跳过该指令
```

**为什么**：`udf` 是“永久未定义”指令，EC=0x00；处理器把 ELR+4 跳过该指令，
程序无缝继续。

### 3.2 调试异常（BRK）

**文段**（§5.4）：断点指令属于同步调试异常。

**代码**（`main.c`）：

```c
__asm__ volatile("brk #0");
```

**结果**：

```text
[SYNC] 到达 EL1：EC=0x3c (BRK)  imm=0x00
[EL1 ] BRK 调试异常，ELR+4 跳过该指令
```

**为什么**：`brk` 触发同步异常，EC=0x3c；BRK 的 ELR 指向断点指令本身，处理器
+4 跳过，避免死循环。

### 3.3 SVC/HVC/SMC —— 主动产生异常的指令

**文段**（§5.3）：这三条指令的唯一用途就是产生异常、在 EL 间移动；EL0 不能
直接调 EL2/EL3，必须先 `svc` 请 EL1 代劳。

**代码**：见 §1.2 的完整链条。ESR 的 `EC` 字段精确区分三种调用：

```text
[SYNC] 到达 EL1：EC=0x15 (SVC)
[SYNC] 到达 EL2：EC=0x16 (HVC)
[SYNC] 到达 EL3：EC=0x17 (SMC)
```

**为什么**：EC=0x15/0x16/0x17 正是 AArch64 下 SVC/HVC/SMC 的异常类编号，
印证“异常类型被编码进 ESR”。

> 内存访问异常（§5.2 的 MMU fault）需要开启 MMU，本裸机示例未开启 MMU，故
> 不演示；其“同步、精确、可重试”的性质见文档 §5.2 正文。

---

## 四、异步异常（§6）

### 4.1 IRQ —— 外设中断（定时器）

**文段**（§6.3）：IRQ 传递外设事件，由外部产生、随时打断。

**代码**：

- `el3_setup`（Secure EL3）配置 GIC：把定时器 PPI30 划为 Group1(Non-secure)，
  使能 distributor/CPU interface。
- `boot.S` 的 EL1 阶段设 `CNTKCTL_EL1.EL0PTEN=1`，允许 EL0 访问物理定时器。
- `main.c` 的 EL0 启动定时器，等待 IRQ：

```c
__asm__ volatile("msr cntp_tval_el0, %0" : : "r"(0x20000));
__asm__ volatile("msr cntp_ctl_el0, %0"  : : "r"(1));
while (!g_irq_ok) { /* 忙等 */ }
```

- 向量表 `+0x480`（更低 EL AArch64 的 IRQ）进 `irq_handler_c`，读 GIC 的 IAR、
  写 EOIR。

**结果**：

```text
[ASYNC]等待异步中断（定时器 IRQ -> EL1 +0x480）...
[IRQ ] GICC_IAR=0x01e  中断ID=0x1e (EL1 物理定时器)
```

**为什么**：定时器到期 → GIC 断言 IRQ → 硬件跳到 `VBAR_EL1+0x480`（更低 EL 的
IRQ 入口）。处理器读 `GICC_IAR` 得到中断 ID=30(0x1e)，写完 `GICC_EOIR` 结束。
这印证 §6.3“异步、外部产生、随时打断”，以及 §7.7 的“IRQ 用 IAR/EOIR 识别与结束”。

### 4.2 FIQ / SError / 虚拟中断

**文段**（§6）：FIQ 与 IRQ 优先级相同；SError 报告系统错误；vIRQ/vFIQ/vSError
只能发信号给 EL1。

**代码**：`fiq_handler_c`/`serror_handler_c` 已实现（打印提示），但 QEMU 上难以
轻易触发，故无实际输出。

> 虚拟中断需 EL2 + GIC 虚拟接口；NMI 需 Armv8.8-A+，均超出本示例范围。

---

## 五、异常处理流程（§7）

### 5.1 进入异常时硬件自动做的事

**文段**（§7.1）：PSTATE→`SPSR_ELx`、返回地址→`ELR_ELx`、原因→`ESR_ELx`。

**代码**（`vector_table.S` 的 `lower_el_sync_handler`）：按当前级别读
`esr_el1/elr_el1/spsr_el1`（或 `_el2`/`_el3`），交给 C 的 dispatch_sync 打印。

**结果**（SVC 链的 EL1 一跳）：

```text
[SYNC] 到达 EL1：EC=0x15 (SVC)  imm=0x00
[REG ] ELR_ELx = 0x0000000040001a30
[REG ] SPSR_ELx = 0x0000000000000000
```

**为什么**：这三行就是把硬件自动写好的寄存器读出来打印，直接印证“硬件自动
保存原因、返回地址、PSTATE 快照”。

### 5.2 向量表（16 入口）

**文段**（§7.4）：基址在 `VBAR_ELx`，表内存**指令**，入口由异常类型+来源 EL+
栈指针决定。

**代码**：EL1/EL2/EL3 的 VBAR 都指向同一张表（`vector_table.S`），16 个入口
每组 0x80 字节。本示例命中的入口：

```text
+0x400  低 EL AArch64 同步  -> lower_el_sync_handler（SVC/HVC/SMC/BRK/UDF）
+0x480  低 EL AArch64 IRQ   -> lower_el_irq_handler（定时器）
```

**结果**：`VBAR_EL1/2/3 = 0x40000800`（0x800 对齐）；SVC 落 `+0x400`、定时器
IRQ 落 `+0x480`。

**为什么**：异常都从更低 EL 以 AArch64 触发，所以走“低 EL + AArch64”这一组，
印证 §7.4 的入口选择规则。

### 5.3 栈指针选择

**文段**（§7.5）：进入异常硬件自动选 `SP_ELx`；EL0 永远用 `SP_EL0`。

**代码**（`boot.S`）：EL3/EL2/EL1 用 `mov sp` 设各自的 SP_ELx，EL1 提前用
`msr sp_el0` 备好 EL0 栈。

**为什么**：异常处理器直接用 `sp` 保存现场（`save_all`），没手动切栈——因为
进入异常时硬件已自动选中 `SP_ELx`，印证 §7.5。

### 5.4 异常返回 ERET

**文段**（§7.6）：`ERET` 从 `SPSR_ELx` 恢复 PSTATE、跳 `ELR_ELx`，原子完成。

**代码**：每个异常处理器末尾的 `eret`（SVC/HVC/SMC 链里共 3 次）。

**结果**：`[RET ] 从 EL3 返回到 EL2` → `[RET ] 从 EL3 逐级返回到 EL1` →
`[ OK ]链条完成，回到 EL0`。

**为什么**：三次 `eret` 依次把执行从 EL3 降到 EL0，程序无缝接续，印证 §7.6 的
原子返回。

---

## 附：完整执行输出

一次运行（`make -C src/bare-metal run`）的完整输出（终端里带颜色，此处为去色的
纯文本；`[STEP n/2]` 区分“引导降级 / 异常演示”两大阶段，测试项用 `----------`
分隔，标签左对齐 7 字符）：

```text
========== AArch64 异常模型验证 启动 ==========

CPU:              cortex-a53
Machine:          virt (secure=on, virtualization=on)
Levels:           EL0 - EL3
Tests:            5 项（SVC / 链条 / BRK / UDF / IRQ）

[STEP 1/2] 引导降级 EL3 -> EL2 -> EL1 -> EL0
[EL3]   当前级别 EL3（Secure 固件）
[REG]   SCR_EL3   = 0x0000000000000531
[REG]   VBAR_EL3  = 0x0000000040000800
[EL3]   设 SCR_EL3(NS=1,RW=1,HCE=1)，ERET 到 EL2（切换安全状态）
[EL3]   GIC 已配置（PPI30 -> Group1，distributor/CPU interface 使能）
[EL2]   当前级别 EL2（Hypervisor）
[REG]   HCR_EL2   = 0x0000000080000000
[REG]   VBAR_EL2  = 0x0000000040000800
[EL2]   设 HCR_EL2(RW=1)，ERET 到 EL1
[EL1]   当前级别 EL1（内核）
[REG]   VBAR_EL1  = 0x0000000040000800
[REG]   SCTLR_EL1 = 0x0000000000c50838
[EL1]   设 SPSR_EL1=EL0t，ERET 到 EL0
[EL0]   当前级别 EL0（应用）

[STEP 2/2] 异常演示

---------- SVC 系统调用（EL0 -> EL1 简单往返） ----------
[SYNC]  到达 EL1：EC=0x15 (SVC)  imm=0x00
[REG]   ELR_ELx = 0x000000004000209c
[REG]   SPSR_ELx = 0x0000000080000000
[EL1]   普通 SVC，直接 ERET 返回 EL0
[OK]    SVC 往返完成，回到 EL0

---------- SVC/HVC/SMC 链条（EL0 -> EL1 -> EL2 -> EL3） ----------
[SYNC]  到达 EL1：EC=0x15 (SVC)  imm=0x42
[REG]   ELR_ELx = 0x00000000400020c4
[REG]   SPSR_ELx = 0x0000000060000000
[EL1]   转发 HVC 到 EL2 ...
[SYNC]  到达 EL2：EC=0x16 (HVC)  imm=0x00
[REG]   ELR_ELx = 0x0000000040001dc8
[REG]   SPSR_ELx = 0x00000000600003c5
[EL2]   转发 SMC 到 EL3 ...
[SYNC]  到达 EL3：EC=0x17 (SMC)  imm=0x00
[REG]   ELR_ELx = 0x0000000040001c38
[REG]   SPSR_ELx = 0x00000000600003c9
[EL3]   到达终点，开始逐级 ERET 返回
[RET]   从 EL3 返回到 EL2
[RET]   从 EL3 逐级返回到 EL1
[OK]    链条完成，回到 EL0

---------- BRK 调试异常 ----------
[SYNC]  到达 EL1：EC=0x3c (BRK)  imm=0x00
[REG]   ELR_ELx = 0x00000000400020e0
[REG]   SPSR_ELx = 0x0000000060000000
[EL1]   BRK 调试异常，ELR+4 跳过该指令
[OK]    BRK 处理完成，回到 EL0

---------- UDF 无效指令 ----------
[SYNC]  到达 EL1：EC=0x00 (UNDEFINED)  imm=0x00
[REG]   ELR_ELx = 0x0000000040002100
[REG]   SPSR_ELx = 0x0000000060000000
[EL1]   无效指令，ELR+4 跳过该指令
[OK]    无效指令处理完成，回到 EL0

---------- IRQ 定时器中断（异步） ----------
[ASYNC] 等待异步中断（定时器 IRQ -> EL1 +0x480）...
[IRQ]   GICC_IAR=0x01e  中断ID=0x1e (EL1 物理定时器)
[OK]    IRQ 处理完成，回到 EL0

========== 全部验证完成 (EL0<->EL1<->EL2<->EL3、同步/异步异常) ==========
```

（寄存器地址因编译优化会略有浮动，属正常。日志支持编译期 `NO_COLOR=1` 关闭颜色，
见 `src/bare-metal/Makefile`。）
