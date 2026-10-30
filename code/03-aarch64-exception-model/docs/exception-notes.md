# AArch64 异常模型 —— 概念、实现与结果印证

本文档按《AArch64 异常模型（中文精读版）》章节组织，把**文段内容**、
**代码实现**、**执行结果**、**因果解释**串在一起：每一节先讲文档这段话说了
什么机制，再给出对应的代码（`src/` 下用户态 demo 或裸机示例），最后用真实
执行结果解释"为什么长这样"。

代码位置：
- 用户态（EL0）：`src/01_sync_exceptions.cpp`、`src/02_async_interrupt.cpp`
- 裸机（EL1，QEMU）：`src/bare-metal/{boot.S,vector_table.S,main.c}`

---

## 一、特权与异常级别（§1–§2）

### 1.1 四个异常级别 EL0–EL3

**文段**：数字越大特权越高；EL0=应用、EL1=内核、EL2=hypervisor、EL3=固件。
EL0/EL1 强制实现，EL2/EL3 可选。

> 纯概念，无代码。裸机示例运行在 EL1（QEMU `-machine virt` 默认复位到 EL1）。

### 1.2 EL 如何跳转

**文段**：EL 只在进入异常/异常返回/复位/调试状态切换时改变；升 EL 靠进入
异常（SVC→EL1、HVC→EL2、SMC→EL3，或 IRQ/FIQ/SError 按 `SCR_EL3`/`HCR_EL2`
路由），降 EL 靠 `ERET`。硬规则：进入异常只升不降、返回只降不升、异常永不
送入 EL0。

**代码**：`main.c` 执行 `svc #0x42` 升入 EL1 异常，处理器末尾 `eret` 降回。
见 §5.3 与 §7.6 的完整印证。

### 1.3 两种特权视角：寄存器访问

**文段**：系统寄存器带 `_ELx` 后缀表示访问需的最低 EL（`SCTLR_EL1` 需 EL1+）。

**代码**（`main.c`）：

```c
__asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
__asm__ volatile("mrs %0, vbar_el1"  : "=r"(vbar));
```

**结果**：

```text
[REG ] SCTLR_EL1 = 0x0000000000c50838
[REG ] VBAR_EL1  = 0x0000000040000800
```

**为什么**：`mrs` 只能在 EL1 执行成功——用户态（EL0）执行会触发 SIGILL。
裸机跑在 EL1，所以能读出这两个寄存器；`VBAR_EL1=0x40000800` 正是 §7.4 说的
"向量表需 0x800 对齐"。

---

## 二、执行状态与安全状态（§3）

**文段**：AArch32/AArch64 两种执行状态（64 位层可承载 32 位层）；安全状态
Secure/Non-secure（+RME 的 Realm/Root），跨安全状态必经 EL3。

> 纯概念/需更高 EL，无用户态或裸机 EL1 代码可验证。

---

## 三、同步异常（§5）

### 3.1 无效指令

**文段**（§5.1）：执行无效/未定义指令产生 UNDEFINED 异常。

**代码**（`01_sync_exceptions.cpp`）：

```cpp
__attribute__((noinline))
void undefined_instruction() { __asm__ volatile("udf #0"); }
```

**结果**：`child_exit_signal(undefined_instruction) == SIGILL`（信号值 4）。

**为什么**：`udf` 是 AArch64 的"永久未定义"指令，任何 EL 执行都触发同步异常，
内核识别后向进程投递 SIGILL——印证"无效指令 → 同步异常"。

### 3.2 内存访问引起的异常

**文段**（§5.2）：MMU 检查不通过（越权/写只读）→ MMU 故障。

**代码**（`01_sync_exceptions.cpp`）：

```cpp
__attribute__((noinline))
void null_deref() {
    volatile int *p = nullptr;
    volatile int x = *p;          // 访问地址 0
    (void)x;
}
```

**结果**：`child_exit_signal(null_deref) == SIGSEGV`（信号值 11）。

**为什么**：解引用空指针产生地址 0 的加载，MMU 拒绝该访问触发数据中止，
内核转为 SIGSEGV——印证"内存访问异常 → 同步异常"。

### 3.3 SVC —— 主动产生同步异常

**文段**（§5.3）：`SVC` 的唯一用途是产生异常、在 EL 间移动；EL0 用它请求 EL1。

**代码**（`main.c` 触发 + `vector_table.S` 处理器）：

```c
__asm__ volatile("svc #0x42");   // main.c：触发
```

```asm
current_el_sync_handler:         ; vector_table.S：+0x200 同步向量
        save_all
        mrs     x0, esr_el1      ; 读异常原因
        mrs     x1, elr_el1      ; 读返回地址
        mrs     x2, spsr_el1     ; 读保存的 PSTATE
        bl      svc_handler_c
        restore_all
        eret
```

**结果**：

```text
[SYNC]触发同步异常 svc #0x42 ...
[SVC ] EC=0x15 (SVC)  imm=0x42
[REG ] ELR_EL1  = 0x0000000040001600
[REG ] SPSR_EL1 = 0x0000000060000345
[ OK ] SVC 处理器完成，ERET 返回
```

**为什么**：`svc #0x42` 触发同步异常，硬件自动把异常原因写入 `ESR_EL1`——
`EC=0x15` 正是"SVC from AArch64"；`imm=0x42` 是 `svc` 的立即数；`ELR_EL1`
指向 `svc` 下一条指令（返回地址）；`SPSR_EL1` 是进入前的 PSTATE 快照。
处理器读完这些后 `eret` 返回，印证 §7.1 的"硬件自动保存"。

### 3.4 调试异常

**文段**（§5.4）：断点指令等调试异常。

**代码**（`01_sync_exceptions.cpp`）：

```cpp
__attribute__((noinline))
void breakpoint_trap() { __asm__ volatile("brk #0"); }
```

**结果**：`child_exit_signal(breakpoint_trap) == SIGTRAP`（信号值 5）。

**为什么**：`brk #0` 产生断点异常，内核转为 SIGTRAP——印证"调试异常 → 同步异常"。

---

## 四、异步异常（§6）

### 4.1 IRQ —— 外设中断

**文段**（§6.3）：IRQ 传递外设事件（如定时器），由外部产生、随时打断。

**代码**（`main.c` 配置 + `vector_table.S` 处理器）：

```c
*gicd_is_enabler = (1U << 30);      // 使能定时器 PPI 中断 30
__asm__ volatile("msr cntp_tval_el0, %0" : : "r"(0x10000));  // 设倒计时
__asm__ volatile("msr cntp_ctl_el0, %0"  : : "r"(1));        // 使能定时器
__asm__ volatile("msr daifclr, #2");   // 清 PSTATE.I，解除 IRQ 屏蔽
```

```asm
current_el_irq_handler:            ; vector_table.S：+0x280 IRQ 向量
        save_all
        bl      irq_handler_c      ; 读 GICC_IAR、写 GICC_EOIR
        restore_all
        eret
```

**结果**：

```text
[ASYNC]等待异步中断（定时器 IRQ）...
[IRQ ] GICC_IAR=0x01e  中断ID=1e (EL1 物理定时器)
[ OK ] IRQ 处理器完成（EOI 已写），ERET 返回
```

**为什么**：定时器到期 → GIC 断言 IRQ → 硬件跳到向量表 `+0x280`（IRQ 入口）。
IRQ 处理器读 `GICC_IAR` 得到中断 ID=30（0x1e，EL1 物理定时器），处理完写
`GICC_EOIR` 结束中断，`eret` 返回——印证 §6.3 的"异步、外部产生、随时打断"。

### 4.2 FIQ / SError / 虚拟中断 / NMI

**文段**（§6）：FIQ 与 IRQ 优先级相同（AArch64）；SError 报告系统错误；
vIRQ/vFIQ/vSError 是虚拟化中断（只能 EL0/1）；NMI 是超级优先级中断。

**代码**：`vector_table.S` 已实现 `current_el_fiq_handler` / `current_el_serror_handler`
（打印提示后 ERET），但 QEMU 上难以轻易触发，故无实际输出。

> 虚拟中断需 EL2 + GIC 虚拟接口；NMI 需 Armv8.8-A+。均超出本示例范围。

---

## 五、异常处理流程（§7）

### 5.1 进入异常时硬件自动做的事

**文段**（§7.1）：PSTATE→`SPSR_ELx`、返回地址→`ELR_ELx`、原因→`ESR_ELx`、
（地址相关→`FAR_ELx`），然后跳向量表。

**代码**：§3.3 的 `current_el_sync_handler` 里的 `mrs esr_el1/elr_el1/spsr_el1`。

**结果**：见 §3.3 的 `EC=0x15`、`ELR_EL1`、`SPSR_EL1` 三行。

**为什么**：这三行就是处理器把硬件自动写好的三个寄存器读出来打印，直接印证
"硬件自动保存了原因、返回地址、PSTATE 快照"。

### 5.2 向量表（16 入口）

**文段**（§7.4）：基址在 `VBAR_ELx`，表内存**指令**（每项最多 32 条），入口由
异常类型 + 来源 EL + 栈指针决定。

**代码**（`boot.S` 设基址 + `vector_table.S` 表）：

```asm
; boot.S
ldr     x0, =vector_table
msr     vbar_el1, x0          ; VBAR_EL1 = 向量表基址

; vector_table.S：16 入口，每入口 0x80 字节
vector_table:
        .balign 0x800
        b   unexpected_handler      ; +0x000 当前 EL+SP_EL0 同步
        b   unexpected_handler      ; +0x080 IRQ
        ...                          ; 每入口 0x80 字节
        b   current_el_sync_handler  ; +0x200 当前 EL+SP_ELx 同步（SVC 走这里）
        b   current_el_irq_handler   ; +0x280 IRQ（定时器走这里）
        b   current_el_fiq_handler   ; +0x300 FIQ
        b   current_el_serror_handler; +0x380 SError
        b   unexpected_handler      ; +0x400 低 EL+AArch64 ...
        b   unexpected_handler      ; +0x600 低 EL+AArch32 ...
```

**结果**：`VBAR_EL1 = 0x0000000040000800`（0x800 对齐）；SVC 落在 +0x200、IRQ
落在 +0x280。

**为什么**：SVC 和 IRQ 都从 EL1 触发到 EL1（同 EL），进入异常时硬件自动选
`SP_ELx`，所以走"当前 EL + SP_ELx"这组向量（+0x200 同步 / +0x280 IRQ）——
印证 §7.4 的入口选择规则与 §7.5 的栈指针自动选择。

### 5.3 栈指针选择

**文段**（§7.5）：进入异常硬件自动选 `SP_ELx`（独立异常栈）。

**代码**：`current_el_sync_handler` 里 `save_all` 用当前 `sp`（即 `SP_ELx`）压栈。

**为什么**：处理器直接用 `sp` 保存现场，而没手动切换栈——因为进入异常时
硬件已自动选中 `SP_ELx`，印证 §7.5。

### 5.4 异常返回 ERET

**文段**（§7.6）：`ERET` 从 `SPSR_ELx` 恢复 PSTATE、跳 `ELR_ELx`，原子完成。

**代码**：每个异常处理器末尾的 `eret`。

**结果**：

```text
[ OK ] SVC 处理器完成，ERET 返回
[ OK ] IRQ 处理器完成（EOI 已写），ERET 返回
[ OK ] 全部验证通过（2/2）
```

**为什么**：处理器 `eret` 后，`main` 打印"已返回"，说明 PSTATE 从 `SPSR_EL1`
恢复、PC 跳回 `ELR_EL1`——两次（SVC、IRQ）都无缝接续，印证 §7.6 的原子返回。

---

## 附：完整执行输出

裸机一次运行（`make -C src/bare-metal run`）的完整输出：

```text
[====]AArch64 异常模型裸机验证
[REG ] SCTLR_EL1 = 0x0000000000c50838
[REG ] VBAR_EL1  = 0x0000000040000800
[INFO] GIC/定时器已初始化，IRQ 已使能
[SYNC]触发同步异常 svc #0x42 ...
[SVC ] EC=0x15 (SVC)  imm=0x42
[REG ] ELR_EL1  = 0x0000000040001600
[REG ] SPSR_EL1 = 0x0000000060000345
[ OK ] SVC 处理器完成，ERET 返回
[ASYNC]等待异步中断（定时器 IRQ）...
[IRQ ] GICC_IAR=0x01e  中断ID=1e (EL1 物理定时器)
[ OK ] IRQ 处理器完成（EOI 已写），ERET 返回
[ OK ] 全部验证通过（2/2）
[EXIT]semihosting 正常退出
```

用户态两个 demo 的结果：

```text
01_sync_exceptions: SIGSEGV=11 / SIGILL=4 / SIGTRAP=5 / 除零=0（负例）
02_async_interrupt: g_interrupted==1、iters>0（异步打断忙循环）
```
