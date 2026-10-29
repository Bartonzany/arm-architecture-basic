# AArch64 异常模型 —— 概念说明与汇编对照

本文档分两部分：**可验证部分**（同步/异步异常在用户态的体现，附汇编对照）
与**特权级概念**（需 EL1+ 特权，无法用户态验证，仅文字说明）。

## 一、可验证部分：同步异常 → Linux 信号

AArch64 的**同步异常**由当前指令引起、与指令流严格对齐。它在 Linux 用户态
体现为**信号**（signal）。下表列出三类可验证的同步异常及其信号映射：

| 文档 § | 异常成因 | 触发指令 | Linux 信号 | demo 函数 |
|--------|----------|----------|------------|-----------|
| §5.2 | 内存访问异常（MMU 拒绝）| `ldr w0, [xzr]` | SIGSEGV | `null_deref` |
| §5.1 | 无效指令 | `udf #0` | SIGILL | `undefined_instruction` |
| §5.4 | 调试异常（断点）| `brk #0` | SIGTRAP | `breakpoint_trap` |

**本机实际汇编**（`01_sync_exceptions.cpp`）：

```asm
null_deref():
        ldr     w0, [x0]      ; x0=0（空指针），MMU 拒绝地址 0 -> SIGSEGV
        brk     #0x3e8        ; GCC 对空指针解引用（UB）的兜底 trap
undefined_instruction():
        udf     #0            ; 永久未定义指令 -> SIGILL
        ret
breakpoint_trap():
        brk     #0x0          ; 断点 -> SIGTRAP
        ret
divide(int, int):
        sdiv    w0, w0, w1    ; 整数除法；除零返回 0，不触发异常
        ret
```

**对应关系**：

1. `null_deref` 解引用空指针 → `ldr w0,[x0]`（x0 为 0）。MMU 检查发现地址 0
   越权，触发同步异常，"数据中止"经内核转为 SIGSEGV。这正是 §5.2 的
   "非特权代码访问特权/无效地址，MMU 阻止访问并触发 fault"。
2. `udf #0` 是 AArch64 的"永久未定义"指令，任何 EL 执行都会触发 UNDEFINED
   异常，对应 §5.1 无效指令，内核转为 SIGILL。
3. `brk #0` 是断点指令，产生断点异常（§5.4 调试异常），内核转为 SIGTRAP。

> ⚙ **整数除零不触发异常**：AArch64 的 `sdiv/udiv` 除零**返回 0**，
> 不像 x86 的 `div` 触发 SIGFPE。这是重要的架构差异——demo 中以
> `divide(1, 0) == 0` 作为负例验证。

## 二、可验证部分：异步中断的软件模拟

AArch64 的**异步异常**（中断）由外部产生、与指令流不同步、随时打断。
硬件异步异常是 IRQ/FIQ/SError（§6），用户态无法直接触发。`02_async_interrupt.cpp`
用 POSIX 信号模拟这一"外部、随时打断"特性：

```cpp
signal(SIGALRM, irq_like_handler);   // 注册"中断处理程序"
alarm(1);                            // 1 秒后定时器"中断"
while (!g_interrupted) { ++acc; }    // 忙循环被 SIGALRM 异步打断
```

- 信号由内核经"硬件 IRQ → 异常处理 → 信号投递"送达，间接体现异步异常路径；
- `irq_like_handler` 对应中断服务例程（ISR），置标志后被主循环观察到。

## 三、特权级概念（需 EL1+，无法用户态验证）

### 3.1 异常级别 EL0–EL3

- 数字越大特权越高：EL0=应用、EL1=内核(Rich OS)、EL2=hypervisor、EL3=固件。
- EL0/EL1 强制实现，EL2/EL3 可选。
- 高 EL 可访问低 EL 的系统寄存器（如 EL2 可访问 `SCTLR_EL1`）；低 EL 访问高
  EL 寄存器会触发异常。

### 3.2 EL 改变时机

EL 只在以下情况改变：进入异常、异常返回、复位、进入/退出调试状态。
两条硬规则：进入异常 EL 只**升**不降，异常返回只**降**不升。

### 3.3 进入异常时硬件自动做的事

以"从 EL0 向 EL1 送入异常"为例：

1. PSTATE 快照 → `SPSR_EL1`（保存程序状态寄存器）；
2. 返回地址 → `ELR_EL1`（异常链接寄存器）；
3. 异常原因 → `ESR_EL1`（异常综合寄存器）；
4. 地址相关同步异常 → `FAR_EL1`（故障地址寄存器）；
5. 更新 PSTATE（切到 EL1、目标执行状态）；
6. 跳到 `VBAR_EL1` 指定向量表的对应入口。

| 寄存器 | 作用 |
|--------|------|
| `SPSR_ELx` | 保存进入异常前的 PSTATE 快照 |
| `ELR_ELx` | 保存首选的异常返回地址 |
| `ESR_ELx` | 保存异常原因（syndrome）|
| `FAR_ELx` | 保存触发 MMU 故障的虚拟地址 |
| `VBAR_ELx` | 保存该 EL 的异常向量表基址 |

### 3.4 向量表

- 基址在 `VBAR_ELx`；表内存的是**指令**（不是地址），每项最多 32 条指令。
- 入口选择由三因素决定：异常类型（SError/FIQ/IRQ/同步）、来源 EL（更低 EL 的
  AArch32/AArch64、当前 EL）、栈指针（SP_ELx/SP_EL0）。
- 关键偏移示例：`VBAR_ELx+0x200`（当前 EL 同步, SP_ELx）、`+0x400`（更低 EL
  同步, AArch64）、`+0x600`（更低 EL 同步, AArch32）、`+0x280`（当前 EL IRQ）。

### 3.5 异常返回 ERET

- `ERET` 从 `SPSR_ELx` 恢复 PSTATE、跳 `ELR_ELx` 的地址，两步**原子**完成。
- 返回后的 EL 由 `SPSR_ELx` 指定，且必须与 `HCR_EL2.RW`/`SCR_EL3.RW` 一致，
  否则产生非法异常返回。

### 3.6 屏蔽与 NMI

- `PSTATE.DAIF`：D=调试、A=SError、I=IRQ、F=FIQ，置 1 屏蔽该类**异步**异常。
- 同步异常不可屏蔽（由指令直接引起）。
- 进入异常时 DAIF 自动置位；嵌套异常需软件显式重新使能。
- 路由到更高 EL 的异常无法被较低 EL 屏蔽；路由到更低 EL 的异常隐式屏蔽。

### 3.7 同步 vs 异步

| | 同步异常 | 异步异常（中断）|
|---|----------|------------------|
| 来源 | 当前指令 | 外部信号 |
| 时机 | 与指令流对齐 | 无法预测 |
| 例子 | SVC/内存故障/无效指令/调试 | IRQ/FIQ/SError/vIRQ/NMI |
| 屏蔽 | 不可屏蔽 | 可被 DAIF 屏蔽 |
| 返回地址 | 与触发指令有确定关系 | 第一条未执行完的指令 |

## 四、未覆盖原因小结

本指南主体（EL0-EL3、执行/安全状态、向量表、ERET、SPSR/ELR/ESR/FAR、DAIF
屏蔽、IRQ/FIQ 路由、虚拟中断、NMI）均为特权级或概念性内容，用户态 C++ 交叉
编译无法验证；需裸机/内核/EL1+ 环境（如 bare-metal 或内核模块）才能演示。