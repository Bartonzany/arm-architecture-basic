# AArch64 异常模型 —— 知识点验证代码

本目录针对《03 - AArch64 异常模型（中文精读版）》整理的可验证知识点。
与"A64 指令集"不同，本指南**绝大多数是特权级/系统级内容**（异常级别、
向量表、ERET、系统寄存器等需要 EL1+ 特权），用户态 C++ 无法直接验证。
因此本目录的策略是：

- **可验证部分**：同步异常在 Linux 用户态的体现（异常类型 → 信号映射）、
  异步中断的软件模拟；
- **特权级部分**：写成概念说明（见 [概念速览](#七概念速览特权级内容) 与
  [docs/exception-notes.md](docs/exception-notes.md)）。

> 文档原文：《Learn the architecture - AArch64 Exception Model》　版本：v1.3

## 一、文档框架分析

异常模型围绕四条主线展开：

| 主线 | 章节 | 核心内容 | 可验证性 |
|------|------|----------|----------|
| 特权与异常级别 | §1–§2 | EL0–EL3 划分、EL 只在异常进出/复位时改变 | ❌ 概念/特权级 |
| 执行状态与安全状态 | §3 | AArch32/AArch64、Secure/Non-Secure/RME | ❌ 概念/特权级 |
| 异常分类 | §4–§6 | 同步（SVC/内存/无效指令/调试）与异步（IRQ/FIQ/SError）| ⚠️ 用户态部分可验证 |
| 异常处理流程 | §7 | PSTATE/SPSR/ELR/ESR、向量表、栈选择、ERET、DAIF 屏蔽 | ❌ 特权级 |

**可验证的仅有两处**（用户态 EL0 能观察到）：

1. **同步异常 → 信号**（§5）：`udf`→SIGILL、空指针→SIGSEGV、`brk`→SIGTRAP；
2. **异步中断的软件模拟**（§6）：用 SIGALRM 定时器信号模拟"外部事件异步打断"。

其余（EL0-EL3、向量表 `VBAR_ELx`、`ERET`、`SPSR/ELR/ESR/FAR`、`DAIF` 屏蔽、
IRQ/FIQ 路由）都需要 EL1+ 特权，写成概念说明。

## 二、知识点 → Demo 映射

| # | 源文件 | 文档章节 | 验证机制 |
|---|--------|----------|----------|
| 01 | `01_sync_exceptions.cpp` | §5 | 同步异常 → 信号：`udf`→SIGILL、空指针→SIGSEGV、`brk`→SIGTRAP；整数除零不触发异常（负例）|
| 02 | `02_async_interrupt.cpp` | §6 | 异步中断：SIGALRM 定时器信号异步打断忙循环 |

> §5.3 的 SVC/HVC/SMC 指令形式已在《02-a64-instruction-set》的
> `18_syscall.cpp` 验证，本目录不重复。

## 三、目录结构

```
03-aarch64-exception-model/
├── README.md                # 本文件
├── docs/
│   └── exception-notes.md   # 特权级概念说明 + 可验证部分汇编对照
├── CMakeLists.txt / Makefile
├── scripts/build_all.sh
├── src/
│   ├── 01_sync_exceptions.cpp
│   ├── 02_async_interrupt.cpp
│   └── test_util.hpp
└── build/                   # 构建产物（已 gitignore）
```

## 四、构建

```bash
./scripts/build_all.sh          # 交叉编译 + 汇编 + 反汇编
./scripts/build_all.sh --run    # qemu 运行（验证信号行为）
make / make disasm              # 或 make 方式
```

> 运行需要 AArch64 真机或 `qemu-aarch64`（`apt install qemu-user`）。
> 这两个 demo 依赖**运行时行为**（信号、fork），交叉编译只能验证
> "触发异常的指令"（`udf`/`brk`/`ldr`/`sdiv`），信号映射需运行观察。

## 五、关键汇编对照（触发异常的指令）

```asm
null_deref():            ldr  w0, [x0]     ; x0=0，访问地址 0 -> SIGSEGV
undefined_instruction(): udf  #0           ; 永久未定义 -> SIGILL
breakpoint_trap():       brk  #0x0         ; 断点 -> SIGTRAP
divide(int,int):         sdiv w0, w0, w1   ; 除零返回 0，不触发异常
```

## 六、可验证知识点详解

见 [docs/exception-notes.md](docs/exception-notes.md)，含：
- 同步异常三类（内存访问 / 无效指令 / 调试）与 Linux 信号的对应；
- AArch64 整数除零返回 0（与 x86 的 SIGFPE 差异）；
- 异步中断的"外部、随时打断"特性。

## 七、概念速览（特权级内容）

以下内容需 EL1+ 特权，无法用户态验证，仅作概念梳理（详见
docs/exception-notes.md）：

- **EL0–EL3**：数字越大特权越高；EL0=应用、EL1=内核、EL2=hypervisor、EL3=固件。
- **EL 改变时机**：只在进入异常 / 异常返回 / 复位 / 调试状态切换时改变；
  进入异常 EL 只升不降，异常返回只降不升。
- **进入异常时硬件自动做**：PSTATE 快照→`SPSR_ELx`、返回地址→`ELR_ELx`、
  异常原因→`ESR_ELx`（地址相关→`FAR_ELx`），然后跳向量表。
- **向量表**：基址在 `VBAR_ELx`，表内存**指令**（每项 32 条），按异常类型 +
  来源 EL + 栈指针选择入口。
- **异常返回**：`ERET` 从 `SPSR_ELx` 恢复 PSTATE、跳 `ELR_ELx`，原子完成。
- **DAIF 屏蔽位**：D=调试、A=SError、I=IRQ、F=FIQ；置 1 屏蔽该类异步异常。
- **栈指针**：进入异常自动选 `SP_ELx`（独立异常栈），正常可切 `SP_EL0`。