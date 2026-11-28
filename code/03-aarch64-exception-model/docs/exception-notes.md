# AArch64 异常模型 —— 概念、实现与结果印证

本文档按《AArch64 异常模型（中文精读版）》章节组织，把**文段内容**、**代码实现**、**执行结果**、**因果解释**串在一起：每一节先讲文档这段话说了什么机制，再给出对应的代码（全部在 `src/bare-metal/` 一个文件夹里），最后用真实执行结果解释“为什么长这样”。

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

关键点：`secure=on,virtualization=on` 让 CPU 实现 EL3/EL2 并**复位进入 EL3**，裸机从 EL3 开始逐级降级，最终在 EL0 完成各异常演示。

---

## 一、特权与异常级别（§1–§2）

### 1.1 四个异常级别 EL0–EL3

**文段**：数字越大特权越高；EL0=应用、EL1=内核、EL2=hypervisor、EL3=固件。EL0/EL1 强制实现，EL2/EL3 可选。

**代码**（`boot.S`）：复位进入 EL3 后逐级 `ERET` 降级，每级调用 `*_report` 打印自身级别与关键寄存器：

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

**文段**：EL 只在进入异常/异常返回/复位时改变；升 EL 靠进入异常（SVC→EL1、HVC→EL2、SMC→EL3），降 EL 靠 `ERET`。硬规则：进入异常只升不降、返回只降不升。

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

**为什么**：一条 `svc` 就串起了完整的 EL0→EL1→EL2→EL3 上升链，再靠三次 `eret` 逐级下降，完美印证“升靠异常、降靠 ERET”。

**嵌套是怎么形成的**：EL1 的异常处理器还没处理完，就在它内部执行 `hvc #0`，于是被更高层的 EL2 异常打断；EL2 处理器同样在内部执行 `smc #0`，又被 EL3 打断。各层能互不干扰，靠的是**每个 EL 有各自独立的一组 `SPSR_ELx`/`ELR_ELx`**：

```
EL0:  svc #0x42  ──────────────────────────── 回到 EL0 继续 ──→
        │ 触发 SVC                                    ↑ eret
        ↓ 硬件把 EL0 状态存 SPSR_EL1/ELR_EL1           │
EL1:   dispatch_sync(level=1)                         │
            │ 执行 hvc #0（处理到一半又触发）           │
            ↓ 硬件把 EL1 状态存 SPSR_EL2/ELR_EL2       │
EL2:       dispatch_sync(level=2)                     │
                │ 执行 smc #0（又触发）                │
                ↓ 硬件把 EL2 状态存 SPSR_EL3/ELR_EL3   │
EL3:           dispatch_sync(level=3) ── eret 逐级弹回 ┘
```

`SPSR_EL1`/`SPSR_EL2`/`SPSR_EL3` 是三个不同寄存器，各存各的现场、互不覆盖；返回时 `eret` 按 LIFO 顺序一层层弹回（EL3→EL2→EL1→EL0），本质和函数调用栈（一层调一层、一层返回一层）是同一个结构。

### 1.3 寄存器访问特权

**文段**：系统寄存器带 `_ELx` 后缀表示访问需的最低 EL（`SCTLR_EL1` 需 EL1+）。

**代码**（`main.c`）：`el3_report/el2_report/el1_report` 分别在对应级别 `mrs` 读 `SCR_EL3`、`HCR_EL2`、`SCTLR_EL1`。这些 `mrs` 在低于对应级别执行会触发异常。

**为什么**：各级别报告能成功读到自己的寄存器，说明“当前 EL 满足访问门槛”。

---

## 二、执行状态与安全状态（§3）

### 2.1 执行状态 AArch32/AArch64

**文段**：每个 EL 的执行状态由更高一级控制：EL1 看 `HCR_EL2.RW`，EL2 看 `SCR_EL3.RW`。

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

**为什么**：`udf` 是“永久未定义”指令，EC=0x00；处理器把 ELR+4 跳过该指令，程序无缝继续。

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

**为什么**：`brk` 触发同步异常，EC=0x3c；BRK 的 ELR 指向断点指令本身，处理器+4 跳过，避免死循环。

### 3.3 SVC/HVC/SMC —— 主动产生异常的指令

**文段**（§5.3）：这三条指令的唯一用途就是产生异常、在 EL 间移动；EL0 不能直接调 EL2/EL3，必须先 `svc` 请 EL1 代劳。

**代码**：见 §1.2 的完整链条。ESR 的 `EC` 字段精确区分三种调用：

```text
[SYNC] 到达 EL1：EC=0x15 (SVC)
[SYNC] 到达 EL2：EC=0x16 (HVC)
[SYNC] 到达 EL3：EC=0x17 (SMC)
```

**为什么**：EC=0x15/0x16/0x17 正是 AArch64 下 SVC/HVC/SMC 的异常类编号，印证“异常类型被编码进 ESR”。

> 内存访问异常（§5.2 的 MMU fault）需要开启 MMU，本裸机示例未开启 MMU，故不演示；其“同步、精确、可重试”的性质见文档 §5.2 正文。

---

## 四、异步异常（§6）

### 4.1 IRQ —— 外设中断（定时器）

**文段**（§6.3）：IRQ 传递外设事件，由外部产生、随时打断。

**代码**：

- `el3_setup`（Secure EL3）配置 GIC：把定时器 PPI30 划为 Group1(Non-secure)，使能 distributor/CPU interface。
- `boot.S` 的 EL1 阶段设 `CNTKCTL_EL1.EL0PTEN=1`，允许 EL0 访问物理定时器。
- `main.c` 的 EL0 启动定时器，等待 IRQ：

```c
__asm__ volatile("msr cntp_tval_el0, %0" : : "r"(0x20000));
__asm__ volatile("msr cntp_ctl_el0, %0"  : : "r"(1));
while (!g_irq_ok) { /* 忙等 */ }
```

- 向量表 `+0x480`（更低 EL AArch64 的 IRQ）进 `irq_handler_c`，读 GIC 的 IAR、写 EOIR。

**结果**：

```text
[ASYNC]等待异步中断（定时器 IRQ -> EL1 +0x480）...
[IRQ ] GICC_IAR=0x01e  中断ID=0x1e (EL1 物理定时器)
```

**为什么**：定时器到期 → GIC 断言 IRQ → 硬件跳到 `VBAR_EL1+0x480`（更低 EL 的 IRQ 入口）。处理器读 `GICC_IAR` 得到中断 ID=30(0x1e)，写完 `GICC_EOIR` 结束。这印证 §6.3“异步、外部产生、随时打断”，以及 §7.7 的“IRQ 用 IAR/EOIR 识别与结束”。

### 4.2 FIQ / SError / 虚拟中断

**文段**（§6）：FIQ 与 IRQ 优先级相同；SError 报告系统错误；vIRQ/vFIQ/vSError 只能发信号给 EL1。

**代码**：`fiq_handler_c`/`serror_handler_c` 已实现（打印提示），但 QEMU 上难以轻易触发，故无实际输出。

> 虚拟中断需 EL2 + GIC 虚拟接口；NMI 需 Armv8.8-A+，均超出本示例范围。

---

## 五、异常处理流程（§7）

### 5.1 进入异常时硬件自动做的事

**文段**（§7.1）：PSTATE→`SPSR_ELx`、返回地址→`ELR_ELx`、原因→`ESR_ELx`。

**代码**（`vector_table.S` 的 `lower_el_sync_handler`）：按当前级别读 `esr_el1/elr_el1/spsr_el1`（或 `_el2`/`_el3`），交给 C 的 dispatch_sync 打印。

**结果**（SVC 链的 EL1 一跳）：

```text
[SYNC] 到达 EL1：EC=0x15 (SVC)  imm=0x00
[REG ] ELR_ELx = 0x0000000040001a30
[REG ] SPSR_ELx = 0x0000000000000000
```

**为什么**：这三行就是把硬件自动写好的寄存器读出来打印，直接印证“硬件自动保存原因、返回地址、PSTATE 快照”。

### 5.2 向量表（16 入口）

**文段**（§7.4）：基址在 `VBAR_ELx`，表内存**指令**，入口由异常类型+来源 EL+栈指针决定。

**代码**：EL1/EL2/EL3 的 VBAR 都指向同一张表（`vector_table.S`），16 个入口每组 0x80 字节。本示例命中的入口：

```text
+0x400  低 EL AArch64 同步  -> lower_el_sync_handler（SVC/HVC/SMC/BRK/UDF）
+0x480  低 EL AArch64 IRQ   -> lower_el_irq_handler（定时器）
```

**结果**：`VBAR_EL1/2/3 = 0x40000800`（0x800 对齐）；SVC 落 `+0x400`、定时器 IRQ 落 `+0x480`。

**为什么**：异常都从更低 EL 以 AArch64 触发，所以走“低 EL + AArch64”这一组，印证 §7.4 的入口选择规则。

### 5.3 栈指针选择

**文段**（§7.5）：进入异常硬件自动选 `SP_ELx`；EL0 永远用 `SP_EL0`。

**代码**（`boot.S`）：EL3/EL2/EL1 用 `mov sp` 设各自的 SP_ELx，EL1 提前用 `msr sp_el0` 备好 EL0 栈。

**为什么**：异常处理器直接用 `sp` 保存现场（`save_all`），没手动切栈——因为进入异常时硬件已自动选中 `SP_ELx`，印证 §7.5。

### 5.4 异常返回 ERET

**文段**（§7.6）：`ERET` 从 `SPSR_ELx` 恢复 PSTATE、跳 `ELR_ELx`，原子完成。

**代码**：每个异常处理器末尾的 `eret`（SVC/HVC/SMC 链里共 3 次）。

**结果**：`[RET ] 从 EL3 返回到 EL2` → `[RET ] 从 EL3 逐级返回到 EL1` → `[ OK ]链条完成，回到 EL0`。

**为什么**：三次 `eret` 依次把执行从 EL3 降到 EL0，程序无缝接续，印证 §7.6 的原子返回。

---

## 六、代码详解（逐文件导读）

上面五章是"原文概念 → 代码"的对照；这一章反过来，**逐文件、逐段解释 `src/bare-metal/` 四个文件各自在做什么、每段代码是什么意思**。建议先读这里建立代码全景，再回看上面的概念对照。

### 6.1 boot.S —— 引导降级（EL3 → EL2 → EL1 → EL0）

复位进入 EL3 后，靠一系列 `ERET` 逐级往下降。每级要做的四件事固定不变：**设 VBAR_ELx、设栈、设控制寄存器、设 SPSR/ELR 再 eret**。

#### 6.1.1 两条 GAS 伪指令（本文件开头）

```asm
.section .text.boot, "ax"
.global _start
```

- `.section .text.boot, "ax"`：定义一个名为 `.text.boot` 的段，"ax" 表示该段**可分配（allocatable）且可执行（executable）**。`.text.boot` 这个名字约定俗成地告诉链接脚本："这是最早执行的引导代码"，要放在镜像最开头（ELF 入口 `_start` 所在位置，即复位后映射到的起始地址）。
- `.global _start`：把 `_start` 标记为**全局符号**，使其对链接器和其他目标文件可见；链接器据此把它当作程序入口点（链接脚本里的 `ENTRY(_start)`），CPU 上电/复位后跳转到的第一条指令就从这里开始执行。
- 简言之：第一行把引导代码放进专门的启动段，第二行让启动标签对外可见，二者配合保证"复位后第一条指令就是 `_start` 处的代码"。

#### 6.1.2 入口 `_start`

```asm
ldr     x19, =vector_table        // 把向量表地址存进 x19
mrs     x0, currentel             // 读当前异常级别
lsr     x0, x0, #2
and     x0, x0, #3                // 取出 bits[3:2]，得到 0~3
cmp     x0, #3                    // 判断从哪一级开始降，做健壮分支
```

- 为什么存进 **x19**？这是 PCS（过程调用标准，《A64 指令集架构指南》§26）定下的"谁能破坏谁"规则——后面每级都要调 C 函数（`*_report` 等），按 PCS 的通用寄存器分工：

  | 寄存器 | 角色 | 跨 `bl` 调用 |
  | --- | --- | --- |
  | X0–X7 | 参数/结果 | 被破坏（调用者保存） |
  | X9–X15 | 临时 | 被破坏（调用者保存） |
  | **X19–X28** | **被调用者保存** | **不丢（C 用了会先存后恢复）** |
  | X29 / X30 | 帧指针 / 链接寄存器 | — |

  所以把地址放 X19–X28 里任何一个都能跨调用保活，这里选 x19 是任意的第一个；若放 X0–X18 之类的 caller-saved，`bl *_report` 一进去值就没了。
- `CurrentEL` 的异常级别编码在 **bits[3:2]**（`0b00`=EL0 … `0b11`=EL3），所以右移 2 位再掩 3。
- 分支是为健壮：即便复位在 EL2 或 EL1，也能正确从那一级往下走（当前 QEMU 配置复位在 EL3）。

#### 6.1.3 `start_el3`（Secure 固件）

```asm
msr     vbar_el3, x19             // 1. 装 EL3 向量表
ldr     x2, =_stack_top_el3
mov     sp, x2                    // 2. EL3 异常栈
mov     x2, #0x501                // 3. SCR_EL3：NS+HCE+RW
msr     scr_el3, x2
mov     x2, #0x3c9                // 4. SPSR_EL3 = EL2h
msr     spsr_el3, x2
ldr     x2, =start_el2            //    ELR_EL3 = EL2 入口
msr     elr_el3, x2               // 写 ELR_EL3：eret 时 PC 跳到 start_el2
bl      startup_banner            // 打印横幅 + STEP 1 标题
bl      el3_report                // 打印 EL3 级别 + SCR_EL3/VBAR_EL3
bl      el3_setup                 // Secure 阶段配 GIC（IGROUPR 是 Secure-only）
eret                               // → EL2
```

关键含义：

- `SCR_EL3=0x501` = `NS(bit0)+HCE(bit8)+RW(bit10)`。**NS=1** 让下一级进 Non-secure；**RW=1** 让下一级（EL2）是 AArch64；**HCE=1** 使能 HVC 指令。
- `SPSR_EL3=0x3c9` 的 M 字段（bits[4:0]）= `0b01001`，即 **EL2h + AArch64**；其余 DAIF=1111（进入 EL2 时中断全屏蔽，由后续代码再放开）。其中 M[4]=0 表示 AArch64（=1 才是 AArch32），M[3:2]=`0b10` 选 EL2，M[0] 选栈：`h`=用本级别的 SP_EL2、`t`=用共享的 SP_EL0——这里选 h 是让 EL2 用独立栈。
- 三个 `bl` 在 eret 之前：打印信息、配 GIC。其中 GIC 配在 Secure EL3 是**必须**的——`GICD_IGROUPR` 等是 Secure-only，切到 Non-secure 后再写无效。

#### 6.1.4 `start_el2`（Hypervisor）与 `start_el1`（内核）

```asm
start_el2:
    msr     vbar_el2, x19          // VBAR_EL2
    ldr     x2, =_stack_top_el2
    mov     sp, x2                 // SP_EL2
    mov     x2, #0x80000000        // HCR_EL2.RW=1 → EL1 是 AArch64
    msr     hcr_el2, x2
    mov     x2, #0x3c5             // SPSR_EL2 = EL1h
    msr     spsr_el2, x2
    ldr     x2, =start_el1         // ELR_EL2 = EL1 入口
    msr     elr_el2, x2
    bl      el2_report
    eret                            // → EL1

start_el1:
    msr     vbar_el1, x19
    ldr     x2, =_stack_top_el1
    mov     sp, x2                 // SP_EL1
    ldr     x2, =_stack_top_el0
    msr     sp_el0, x2             // 提前备好 EL0 栈
    mov     x2, #0x200             // CNTKCTL_EL1.EL0PTEN=1
    msr     cntkctl_el1, x2        // 允许 EL0 访问物理定时器
    mov     x2, #0x0               // SPSR_EL1 = EL0t + DAIF 不屏蔽
    msr     spsr_el1, x2
    ldr     x2, =main              // ELR_EL1 = main
    msr     elr_el1, x2
    bl      el1_report
    eret                            // → EL0
```

- `HCR_EL2.RW=1`、`SPSR_EL2=EL1h`、`SPSR_EL1=EL0t` 依次决定"下一级是 AArch64、用什么栈"——正是原文 §3.1/§7.5 说的"执行状态和栈指针选择由更高 EL 拍板"。
- `SPSR_EL1=0`（EL0t + DAIF=0000）里 **DAIF 全 0**，意味着到 EL0 后中断不被屏蔽——后文定时器 IRQ 能直接打断 EL0。

#### 6.1.5 一个关键坑：栈指针怎么设

```asm
mov sp, x2        // EL3/EL2/EL1：本级改栈只能 mov sp
msr sp_el0, x2    // EL1：给 EL0 备栈用 msr sp_el0
```

`SP_EL1`/`SP_EL2`/`SP_EL3` **不能在本级用 `msr` 直接写**：它们的编码 op1=4 表示"访问更低 EL 的 banked 栈指针"，只允许在更高 EL 执行，本级执行会 UNDEFINED。本级当前栈 `sp` 本来就映射到该级的 `SP_ELx`，所以 `mov sp` 即可；唯独 EL0 只能用 `SP_EL0`，需要由 EL1 用 `msr sp_el0` 提前设好。

那"给 EL0 备的栈"为什么不放到 `start_el0` 里设？因为两个原因：

1. **正常降级路径不经过 `start_el0`**：EL1 到 EL0 是 `eret`（`ELR_EL1=main`），直接落到 `main`，`start_el0` 只是 `_start` 开头"复位即 EL0"的兜底分支，正常走不到——放那里就等于没设。
2. **EL0 没权限执行 `msr sp_el0`**：该访问要 EL1+，EL0 里会 UNDEFINED；EL0 里只能用 `mov sp`（其 `sp` 即 SP_EL0），但 EL0 还没进 `main`、没人给它 `sp` 初值。

所以必须由 EL1 在 eret 之前用 `msr sp_el0` 备好，eret 一落进 EL0 栈就绪——这是"高 EL 为低 EL 准备上下文"的典型例子（原文档 §7.5）。

### 6.2 vector_table.S —— 共享向量表 + 异常处理器

#### 6.2.1 16 入口布局

```asm
.section .text.vector, "ax"
.balign 0x800              // 向量表需 0x800 对齐（VBAR 要求）
.global vector_table

.macro ventry label
    b       \label
    .balign 0x80           // 每个入口 0x80 字节（32 条指令）
.endm
```

- `ventry` 宏：每个入口放一条 `b` 跳到处理代码，再 `.balign 0x80` 对齐到下一个入口，保证 16 个入口的偏移对应正确（+0x000/+0x080/…/+0x780）。
- 4 组 × 4 类：`+0x000/0x200/0x400/0x600` 是同步，`+0x080/…` IRQ，`+0x100` FIQ，`+0x180` SError；来源分"当前 EL+SP_EL0 / 当前 EL+SP_ELx / 低 EL AArch64 / 低 EL AArch32"。
- 本示例只有两个入口是"活的"：`+0x400` 同步（`lower_el_sync_handler`）和 `+0x480` IRQ（`lower_el_irq_handler`），其余全接 `unexpected_handler`。

**为什么放一条 `b` 就能跳到处理代码？** 关键认知：AArch64 的向量表**不是"函数指针表"，而是一段可执行代码**。硬件进入异常时并不"查表取地址再跳"，而是直接令 `PC = VBAR_ELx + 偏移`，随即从该地址开始取指执行。因此那个固定偏移处放的必须是**指令**，这里放的就是 `b \label` 这条分支指令本身——CPU 取到并执行它，PC 便转到 `\label`，而不是把 label 的地址"存"进某个表项。以本示例 SVC 落 EL1 为例：

```text
VBAR_EL1                    = 0x0000000040000800
低 EL AArch64 同步异常的偏移 = +0x400
硬件置 PC                   = 0x40000800 + 0x400 = 0x40000c00
0x40000c00 处               = `b lower_el_sync_handler` 这 4 个字节
CPU 执行这条 b              -> PC = lower_el_sync_handler，进入处理代码
```

对比 x86 的 IDT 更直观：x86 的门描述符里存的是处理函数地址，硬件查表拿到地址再跳；ARM64 的向量表是"代码槽"，硬件算好地址直接执行，槽里写什么指令就先执行什么。另外，`b` 是 ±128MB 的直接分支，处理函数在此范围内一跳即达；`b` 只占 4 字节，槽内其余到下一个 0x80 边界的空间由 `.balign 0x80` 填充（代码段通常填 NOP/0），真正"落地"的只有槽首那条 `b`。

#### 6.2.2 `save_all` / `restore_all`

```asm
.macro save_all
    stp     x0,  x1,  [sp, #-16]!
    ...                        // x0~x18
    stp     x18, x29, [sp, #-16]!
    str     x30, [sp, #-16]!   // x29 帧指针、x30 返回地址
.endm
```

- 中断是异步的，处理器不知道被打断的程序正在用哪些寄存器，所以要把**全部 caller-saved 寄存器（x0~x18）+ 帧指针 x29 + 返回地址 x30** 压栈；x19~x28 是 callee-saved，由 C 函数自行保存，这里不管。
- `stp ..., [sp, #-16]!` 是"先减栈、再存两寄存器"的推入写法，`restore_all` 用 `ldp ..., [sp], #16` 反向弹出，一一对应。

#### 6.2.3 `lower_el_sync_handler`（同步异常公共入口）

```asm
save_all                       // 压现场
mrs     x0, currentel         // 判断当前是 EL1/EL2/EL3
lsr     x0, x0, #2
and     x0, x0, #3
mov     x19, x0               // 级别存 x19（callee-saved）
// 按级别读 ESR/ELR/SPSR 到 x1/x2/x3
cmp     x0, #3
b.eq    1f
cmp     x0, #2
b.eq    2f
mrs     x1, esr_el1          // EL1
mrs     x2, elr_el1
mrs     x3, spsr_el1
b       3f
2:      mrs     x1, esr_el2   // EL2
        ...
1:      mrs     x1, esr_el3   // EL3
        ...
3:
bl      dispatch_sync          // C 分派：dispatch_sync(level, esr, elr, spsr)
                               // 返回 x0=1 表示要跳过触发指令
cbz     x0, 4f                // 返回 0 → 直接 eret
// 返回 1 → 把对应 ELR_ELx +4（跳过 BRK/UDF 这类把 ELR 指向自身的指令）
mrs     x5, elr_el1
add     x5, x5, #4
msr     elr_el1, x5
4:
restore_all
eret
```

> **关于标签写法**：`b.eq 1f` 里的 `1f` 是 GNU 汇编的**局部标签**——`1f`=向前（forward）找最近的 `1:` 标签，数字只是"标签名字"（局部标签可用 0~9），不是顺序编号。所以上面 `1:` 标 EL3、`2:` 标 EL2、`3:` 是汇合点，编号和物理顺序不一致完全正常。这样编排是刻意为之：**EL1 是最常走的热路径**（SVC/BRK/UDF 都从 EL0 降到 EL1），放在 `b.eq` 之后的顺序位置（fall-through，省一次跳转）；EL2（HVC）/EL3（SMC）只在链条里偶尔走一次，用 `1f`/`2f` 跳过去。三个标签的物理排列是"EL1(顺序) → EL2(`2:`) → EL3(`1:`) → 汇合(`3:`)"。

这段体现了两件架构事实：

1. **硬件自动填好了三个寄存器**：进入异常时，硬件已把 PSTATE→`SPSR_ELx`、返回地址→`ELR_ELx`、原因→`ESR_ELx`，这里只是把它们读出来交给 C 打印/分派。
2. **ELR 的语义因异常类型而异**：SVC/HVC/SMC 的 ELR 指向"下一条指令"，直接 eret 即继续；但 BRK/UDF 的 ELR 指向"触发指令本身"，若直接 eret 会死循环，所以 `dispatch_sync` 返回 1，汇编把 ELR_ELx +4 跳过该指令。
3. **"读"和"改"是两回事**：前面 `mrs x1/x2/x3` 只是把 ESR/ELR/SPSR **读出来打印**，不改任何东西；后面 `mrs…add…msr elr_elx` 才是真正**修改返回地址**（+4 跳过触发指令，A64 一条指令 4 字节）。写回哪个 `ELR_ELx` 取决于当前级别（eret 从当前级别返回，用的就是该级 ELR_ELx），所以又按 x19 判断了一次——这就是"打印读一遍、改地址又按级别写一遍"的原因。

### 6.3 main.c —— 日志框架 + 异常分发 + 演示主流程

#### 6.3.1 硬件基址（顶部的宏）

```c
#define UART0_DR ((volatile unsigned int *)0x09000000)   // PL011 串口数据寄存器
#define GICD_CTLR ((volatile unsigned int *)0x08000000)  // GICv2 分发器控制
#define GICC_IAR  ((volatile unsigned int *)0x0801000C)  // 中断确认寄存器
#define GICC_EOIR ((volatile unsigned int *)0x08010010)  // 中断结束寄存器
#define TIMER_IRQ_ID 30                                  // EL1 物理定时器 PPI
```

全是 QEMU `-machine virt` 的物理外设地址，用 `volatile` 指针直接读写 MMIO。`TIMER_IRQ_ID=30` 是 SBSA 定义的 EL1 物理定时器私有外设中断号。

#### 6.3.2 日志框架（工程化输出，非核心逻辑）

`uart_putc/puts/puthex/putdec` 只是串口字节输出；`tag_color()` 按标签前缀返回 ANSI 颜色；`log_begin/log_line/log_reg/log_kv/log_step/log_test/log_banner` 拼出带颜色、对齐的日志行。这层只是"让输出好看"，与异常模型本身无关，可跳过。

#### 6.3.3 引导时期的几个函数

- `startup_banner()`：打印启动横幅 + 键值对信息 + `STEP 1/2` 标题。
- `el3_report/el2_report/el1_report()`：`mrs` 读本级的 SCR_EL3 / HCR_EL2 / SCTLR_EL1 并打印——只想说明"到哪一级就够权限读哪一级的寄存器"。
- `el3_setup()`：在 Secure EL3 配 GIC（上一章 §4.1 已解释为什么必须在 Secure）。

#### 6.3.4 `dispatch_sync` —— 同步异常的分发核心

```c
int dispatch_sync(level, esr, elr, spsr) {
    unsigned int ec  = (esr >> 26) & 0x3F;   // ESR[31:26] = 异常类
    unsigned int imm = esr & 0xFFFF;         // ESR[15:0]  = SVC 立即数
    int skip = 0;
    // 打印"到达 ELx：EC=… imm=…" + ELR/SPSR

    if (level == 1) {
        if (ec == 0x15) {                    // SVC
            if (imm == 0x42) {               // 立即数标记"走链条"
                hvc #0;                      // 转发到 EL2
            } else {                         // 普通 SVC
                ;                            // 直接返回
            }
        } else if (ec == 0x3c)  skip = 1;    // BRK：ELR 要 +4
        else if (ec == 0x00)    skip = 1;    // UDF：ELR 要 +4
        else if (ec == 0x24||0x25) skip = 1; // 数据中止
    } else if (level == 2) {
        if (ec == 0x16) smc #0;              // HVC→EL3
    } else if (level == 3) {
        if (ec == 0x17) ;                    // SMC 终点
    }
    return skip;                              // 1=让汇编把 ELR+4
}
```

- `ec` 从 ESR[31:26] 取，**EC=0x15=SVC、0x16=HVC、0x17=SMC、0x3C=BRK、0x00=UNDEFINED**——"异常到底为啥发生"全编码在 ESR 里。
- `imm = esr & 0xFFFF` 是取 ESR 的**低 16 位**：A64 的 SVC/HVC/SMC 指令各带一个 16 位立即数（imm16，如 `svc #0x42` 里的 `0x42`），硬件执行该指令时把这个 imm16 自动搬进 `ESR[15:0]`，软件从 ESR 读即可、不必去解析指令机器码。
- **参数怎么传进来的（AAPCS）**：`dispatch_sync(level, esr, elr, spsr)` 的四个参数按 AAPCS 依次放在 **X0/X1/X2/X3**。汇编在 `bl dispatch_sync` 之前已把 level 留在 x0（读 currentel 的产物，中间 `cmp`/`mrs x1~x3` 都不碰 x0）、把 esr/elr/spsr 读进 x1/x2/x3，所以调用瞬间正好对位。之所以还要 `mov x19, x0` 备份 level：x0 是 caller-saved，dispatch_sync 返回后 x0 变成 skip，后面"ELR+4"那段还得靠 callee-saved 的 x19 拿回级别。
- **链条的实现方式**：EL1 收到 SVC 就再执行 `hvc`、EL2 收到 HVC 就再执行 `smc`，形成嵌套异常；每级处理完 `eret` 自然逐级退回（嵌套机制见 §1.2）。imm==0x42 只是区分"普通往返/走链条"的开关。
- **返回值 `skip` 与汇编协作**：BRK/UDF 这类 ELR 指向自身，返回 1，汇编把对应 `ELR_ELx+4`，避免 eret 后重跑同一条指令死循环。

#### 6.3.5 异步异常处理器

```c
void irq_handler_c(void) {
    unsigned int iar = *GICC_IAR;      // 读中断确认 = 拿到 INTID
    unsigned int id  = iar & 0x3FF;
    ...
    *GICC_EOIR = iar;                  // 写中断结束
    msr cntp_ctl_el0, 0;               // 禁用定时器清电平中断
    g_irq_ok = 1;
}
```

三件套对应 §4.1：**读 IAR 识别中断 → 清 ENABLE 清除电平中断 → 写 EOIR 结束**。`fiq_handler_c`/`serror_handler_c` 只是占位（打印），本示例不触发。

#### 6.3.6 `main` —— 5 个异常演示

```c
svc #0;       // ① SVC 简单往返（EL0→EL1→EL0）
svc #0x42;    // ② SVC/HVC/SMC 完整链条
brk #0;       // ③ BRK 调试异常
udf #0;       // ④ UDF 无效指令
timer_irq_demo();  // ⑤ 定时器 IRQ（异步）
```

每条异常指令都会打断当前 EL0 执行流、经向量表走一遍"进入异常 → 处理 → ERET"，再回到下一条指令继续，所以能顺序把五类异常各验一遍。

### 6.4 linker.ld —— 内存布局与 4 个异常栈

```ld
SECTIONS {
    . = 0x40000000;             // QEMU virt RAM 起点 = 复位入口
    .text : { *(.text.boot) *(.text.vector) *(.text*) }
    ...
    . += 0x1000; _stack_top_el3 = .;   // EL3 栈 4KB
    . += 0x1000; _stack_top_el2 = .;   // EL2 栈 4KB
    . += 0x1000; _stack_top_el1 = .;   // EL1 栈 4KB
    . += 0x1000; _stack_top_el0 = .;   // EL0 栈 4KB
}
```

- `.text.boot` 在最前、`.text.vector` 紧随其后，保证 `_start` 在 0x40000000、向量表在其后 0x800 对齐处（所以输出里 `VBAR=0x40000800`）。
- 给四个异常级别各留 4KB 独立栈（`_stack_top_el3/2/1/0` 依次递增、互不重叠），对应 boot.S 里每级 `mov sp` / `msr sp_el0` 取用的栈顶。

---

## 附：完整执行输出

一次运行（`make -C src/bare-metal run`）的完整输出（终端里带颜色，此处为去色的纯文本；`[STEP n/2]` 区分“引导降级 / 异常演示”两大阶段，测试项用 `----------` 分隔，标签左对齐 7 字符）：

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

（寄存器地址因编译优化会略有浮动，属正常。日志支持编译期 `NO_COLOR=1` 关闭颜色，见 `src/bare-metal/Makefile`。）
