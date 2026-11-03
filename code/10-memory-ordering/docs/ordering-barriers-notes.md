# AArch64 内存系统、排序与屏障 —— 概念、实现与结果印证

本文档按《AArch64 内存系统、排序与屏障（中文精读版）》章节组织，把**文段内容**、
**代码实现**、**执行结果**、**因果解释**串在一起。

该文档的核心是**跨观察者（多核）的内存排序**——Armv8-A 是弱排序架构，乱序只有在
多个观察者（多核 PE / 外设）互相观察对方的内存访问时才"可见"。QEMU TCG 单核无法
复现这种跨核重排序，故本示例聚焦**可确定性验证**的两部分：

1. **LDAR/STLR/LDAXP 的对齐要求**（§3 第一条硬规则）—— 非对齐必触发对齐故障；
2. **ISB 上下文同步**（§4）—— 写系统寄存器后必须 ISB 才保证生效。

跨核排序（§2.1 消息传递、§3 STLR/LDAR）以汇编文段说明，运行层不展开（见 §五）。

代码位置：`src/`（boot.S + vector_table.S + main.c + linker.ld）。
运行方式：

```bash
./scripts/build_all.sh --run
```

验证输出（摘要）：

```text
[ A ]LDAR/STLR 对齐访问（§3 对齐规则）
[ OK ] 对齐的 STLR/LDAR 正常完成（无故障）
[ B ]非对齐 LDAR → 对齐故障（LDAR 必须对齐到元素大小）
[SYNC] EC=0x25  FSC=0x21        ← 对齐故障
[ C ]非对齐 LDAXP → 对齐故障（独占对必须对齐到 2×元素大小）
[SYNC] EC=0x25  FSC=0x21
[ D ]非对齐 STLR → 对齐故障
[SYNC] EC=0x25  FSC=0x21
[ E ]上下文同步：CPACR_EL1.FPEN + ISB（§4）
[SYNC] EC=0x07  FSC=0x00        ← FP/SIMD 捕获
[ OK ] 写 CPACR + ISB 后，FADD 恢复正常执行
```

---

## 一、弱排序与依赖（§1）—— 概念

### 文段

Armv8-A 是弱排序架构：没有依赖的内存访问可乱序。只有两种依赖中的**地址依赖**
（后一次访问用前一次加载的结果当地址）必须保序；**控制依赖**（用加载值做分支）
不构成排序保证：

```asm
LDR X0, [X1]      ; 地址依赖：X0 是下一次存储的地址 → 必须保序
STR X2, [X0]

LDR X0, [X1]      ; 控制依赖：X0 只用来做分支 → 不保证排序
CBZ X0, elsewhere
LDR X2, [X5]
```

> 纯概念，无独立代码。地址依赖/控制依赖的区别是"内存重排序边界"的根基，其排序
> 强制手段由 §2/§3 的屏障与获取/释放指令提供。

## 二、屏障三兄弟 DMB/DSB/ISB（§2）—— 概念

### 文段

| 指令 | 作用 | 关键区别 |
|------|------|----------|
| **DMB** | 阻止显式数据访问跨屏障重排序 | 只管**顺序**，不保证**完成**，不阻塞 PE |
| **DSB** | 之前访问在 DSB 完成前必须**完成** | 比 DMB 更强：完成才退役 |
| **ISB** | 冲刷流水线，同步**上下文**（缓存/TLB/系统寄存器）| 只同步指令/上下文，不管数据排序 |

参数收窄作用范围：`DMB SY`（任意-任意）、`DMB ST`（只排存储）、`DMB LD`
（只排加载）、`OSH`（外部可共享域）。AArch64 **必须显式写参数**。

> 排序语义跨观察者才能观察到（见 §五）；单核无法复现，本示例不展开运行时验证。

## 三、获取/释放指令的对齐要求（§3）—— 已验证 ★

### 文段

`LDAR/STLR` 比 DMB 更轻（单向屏障），三条硬规则第一条：**对齐**——`LDAR/LDAPR/STLR`
地址必须对齐到元素大小；独占对 `LDAXP/STLXP` 必须对齐到**元素大小的两倍**，否则
**对齐故障**：

```asm
LDAXP x0, x1, [0x08]  ; 对齐故障（未 16 字节对齐）
LDAXP x0, x1, [0x10]  ; OK
```

### 代码实现（`main.c` 的 [A]–[D]）

```c
static volatile unsigned long g_shared = 0;                 // 8 字节对齐
static volatile unsigned long g_pair[2] __attribute__((aligned(16)));  // 16 字节对齐

// [A] 对齐访问：无故障
__asm__ volatile("stlr %0, [%1]" : : "r"(1UL), "r"((unsigned long)&g_shared));
__asm__ volatile("ldar %0, [%1]" : "=r"(v) : "r"((unsigned long)&g_shared));

// [B] 非对齐 LDAR（+1）→ 对齐故障
g_test = T_ALIGN;
__asm__ volatile("ldar %0, [%1]" : "=r"(v) : "r"((unsigned long)&g_shared + 1));

// [C] 非对齐 LDAXP（g_pair[1] 是 8 字节对齐、非 16 字节）→ 对齐故障
g_test = T_ALIGN;
__asm__ volatile("ldaxp %0, %1, [%2]" : "=r"(a), "=r"(b) : "r"((unsigned long)&g_pair[1]));

// [D] 非对齐 STLR（+1）→ 对齐故障
g_test = T_ALIGN;
__asm__ volatile("stlr %0, [%1]" : : "r"(1UL), "r"((unsigned long)&g_shared + 1));
```

同步异常经 `vector_table.S` 进入 `sync_handler_c`，按当前测试（`T_ALIGN`）识别
对齐故障后 `ELR += 4` 跳过故障指令并 ERET。

### 结果

```text
[SYNC] EC=0x25  FSC=0x21   ← [B]/[C]/[D] 都是数据中止 + 对齐故障
[ OK ] 非对齐 LDAR 触发对齐故障（FSC=0x21）
[ OK ] 非对齐 LDAXP（8B 对齐非 16B）触发对齐故障
[ OK ] 非对齐 STLR 触发对齐故障（FSC=0x21）
```

### 为什么

`FSC=0x21` 是**对齐故障**，`FAR_EL1` 指向未对齐地址（如 `...1a99`/`...1aa8`）。
这印证：LDAR/STLR 的地址必须对齐到元素大小（8 字节），LDAXP 必须对齐到 2×元素
大小（16 字节），违反即对齐故障——**与 SCTLR.A 无关**（这是获取/释放指令自身的
硬性要求，普通 LDR 在 A=0 时仍可非对齐，见 09 示例的 [ E ]）。

## 四、指令屏障与上下文同步（§4）—— 已验证 ★

### 文段

PE 的**上下文**是缓存、TLB、系统寄存器的状态。写系统寄存器后要等一次**上下文
同步事件**（执行 ISB / 进入异常 / 异常返回 / 退出调试）才保证对后续指令生效。
典型场景：改 `CPACR_EL1.FPEN` 使能 FP 后必须 `ISB`，否则后面的 `FADD` 可能仍按
旧上下文（禁用）触发同步异常。

### 代码实现（`main.c` 的 [E]）

```c
// 1. QEMU 复位后 FPEN=0（FP 禁用），FADD 触发 FP/SIMD 捕获
g_test = T_FP;
__asm__ volatile("fadd s0, s1, s2");              // 预期 EC=0x07

// 2. 写 CPACR_EL1.FPEN=3 使能 FP，必须 ISB 同步
__asm__ volatile("msr cpacr_el1, %0" : : "r"(3UL << 20));
__asm__ volatile("isb");
__asm__ volatile("fadd s0, s1, s2");              // 恢复正常
```

### 结果

```text
[REG ] CPACR_EL1(初始) = 0x0000000000000000      ← FPEN=0，FP 默认禁用
[SYNC] EC=0x07  FSC=0x00                        ← FADD 触发 FP/SIMD 捕获
[ OK ] FPEN=0：FADD 触发 FP/SIMD 捕获（EC=0x07）
[ OK ] 写 CPACR + ISB 后，FADD 恢复正常执行
```

### 为什么

`EC=0x07` 是"Trapped FP/SIMD"——`CPACR_EL1.FPEN=0` 时任何 FP 指令都会捕获。写
`FPEN=3` 后紧跟 `ISB`，ISB 冲刷流水线、使新的系统寄存器值对后续 `FADD` 生效，
于是第二次 `FADD` 正常执行。若省略 ISB，架构允许后续指令仍按旧上下文（禁用）执行
而再次捕获——这是"上下文同步"的完整含义。

---

## 五、跨观察者排序（§2.1、§3 消息传递）—— 概念说明

### 文段

**消息传递**是排序验证的经典模式。生产者先写数据、再写标志；消费者先读标志、
再读数据。无屏障时，消费者可能看到"标志已置位、数据却还是旧值"（因为两次存储
被乱序）：

```asm
; 生产者（PE0）
STR  #1, [X1]      ; 写数据
DMB  ST            ; 存储-存储屏障
STR  #1, [X3]      ; 写标志
; 消费者（PE1）
LDR  X0, [X3]      ; 读标志
LDR  X1, [X1]      ; 读数据（DMB ST 保证先观察到数据写）
```

用 STLR/LDAR 同理，且更轻：

```asm
STR  #1, [X1]      ; 写数据
STLR #1, [X3]      ; release 写标志：先观察到前面的 STR 才能观察到这条 STLR
; 消费者
LDAR X0, [X3]      ; acquire 读标志
LDR  X1, [X1]      ; 读到数据必为新值
```

### 为什么本示例不运行时验证

这类重排序只有在**多个 PE 互相观察**时才可见，需要多核（`-smp N`）环境；而 QEMU
TCG 的内存模型近乎顺序一致，通常**复现不出**无屏障时的错误结果（0x0/0x1 那行）。
因此本示例把运行时验证聚焦在单核可确定的两点上（对齐故障、ISB 上下文同步），
消息传递模式作为概念说明保留在此，实际排序验证需真机（或 Arm 官方的 Barrier
Litmus Tests，见文档"下一步"）。

> 关联：`02-a64-instruction-set` 的 `13_load_acquire.cpp` 用 `std::atomic` 的
> acquire/release 展示了 STLR/LDAR 的**指令形态**（单线程下的消息传递模式）。
