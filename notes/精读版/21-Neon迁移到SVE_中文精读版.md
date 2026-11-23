# 读懂 Neon 迁移到 SVE（中文精读版）

> 原文：*Learn the architecture - Migrate Neon to SVE*（文档 ID：102131_0100_04_en，Issue 04，2022-04-20）
>
> 版权 © 2022 Arm Limited（或其关联公司）。保留所有权利。非保密。
>
> 说明：本文在原中文翻译的基础上重新组织了叙述逻辑与表达方式，技术内容、表格与图表保持原意；个别明显笔误已按 Arm 架构规范订正。

---

## 导读：这篇文章讲什么

上一篇《SVE 与 Neon 编码对比》讲"两者哪里不一样"，这一篇讲"**怎么把已经写好的 Neon 代码搬过去**"。核心是三句话：

1. **迁移就三步**：改编译器选项 → 内建函数替换（Neon → SVE）→ 链接 SVE 版数学库。
2. **不是所有 Neon 代码都该迁**——有两个反例（稀疏谓词开销、VLA 开销）下，保留 Neon 反而更好。
3. **Neon 向量化不了"动态长度/带 break 的循环"，SVE 可以**——这正是迁移的主要收益。

前置：熟悉 Neon 和 SVE 基础（见 18 号文）。

## 1. 回顾一下两者基础（一句话版）

- Neon：固定 128 位向量，指令总操作 64/128 位。
- SVE：可扩展向量（128～2048 位）+ 谓词（P）+ 首故障（FFR），支持 **向量长度无关（VLA）** 编程。

关键推论：**在不知道向量长度时编译应用**，意味着向量/谓词不能从编译期常量初始化、循环增量和行程计数未知、寄存器溢出/填充要适配向量长度——这些都由 SVE 的运行时指令（`svcntw`、`svwhilelt` 等）在运行时解决。

## 2. 迁移前的准备：四种写码方式 SVE 都有

你当初为 Neon 优化代码用过的四种方式，SVE 全都有对应：

| 方式 | Neon → SVE 的对应 |
| --- | --- |
| 自动向量化 | 改编译器选项即可（`-march=armv8-a+sve`） |
| 内建函数 | `arm_neon.h` → `arm_sve.h` |
| 数学库 | Arm Performance Libraries 的 SVE 变体 |
| 汇编 | SVE 汇编 / 内联汇编 |

**编译器选项关键点**：自动向量化在 `-O2/-O3/-Ofast` 默认启用（`-fno-vectorize` 关闭；`-O1` 需 `-fvectorize` 显式开）。头文件/编译命令的对应见下表（核心）：

- Neon：`#include <arm_neon.h>` + `armclang -O2 -mcpu=native -o out file.c`
- SVE：`#ifdef __ARM_FEATURE_SVE #include <arm_sve.h> #endif` + `armclang -O2 -march=armv8-a+sve -o out file.c`（无 SVE 硬件时用 ArmIE 仿真；有硬件用 `-mcpu=native`）

**给编译器的两个提示**：
1. `restrict` 关键字——告诉编译器指针不别名，从而激进向量化。
2. `#pragma clang loop vectorize(enable)` / `interleave(enable)`。

**一个重要的自动向量化条件差异**：Neon 要求"循环迭代次数在编译时已知"，SVE **不要求**。所以带 break 的循环、动态长度的循环，Neon 向量化不了，SVE 可以。

## 3. 什么时候别迁：保留 Neon 的两个反例

迁移不是无脑的。两个边角情况下，保留优化的 Neon 代码反而更划算：

1. **稀疏谓词开销**：当内层循环 `inner <= 0.5*VL` 时，用谓词化的全长 SVE 寄存器算部分向量，开销可能高于直接用标量 Neon。谓词大多为假时，每次向量操作做的工作少，不划算。

```c
// inner 很小时，SVE 的谓词开销 > Neon 标量
void foo(float *restrict x, float *restrict y, int outer, int inner) {
  for (int j = 0; j < outer; j++)
    for (int i = 0; i < inner; i++)
      x[i + j*inner] = y[i + j*inner] * y[i + j*inner];
}
```

2. **一般 VLA 开销**：循环行程计数用 Neon 可以显式算出（编译期已知），编译器能完全展开 Neon 循环、产生更省寄存器的代码；VLA 写法做不到，还得额外谓词。

```c
// 固定 8 次循环，Neon 完全展开更省，SVE 反而要谓词
void foo(float *restrict x, float *restrict y) {
  for (unsigned i = 0; i < 8; i++) x[i] = y[i] * y[i];
}
```

一句话：**Neon 的强项是"小而固定"的循环（能完全展开、省寄存器），SVE 的强项是"大而动态"的循环（谓词 + 可变长度）**。

## 4. 迁移三步

**高级语言（C/C++/Fortran）**：
1. 更新编译器选项（含自动向量化选项）。
2. 参考 SVE 规范，把 Neon 内建函数替换为 SVE 内建函数。
3. 链接数学库的 SVE 变体。

**汇编**：同上，把 Neon 指令替换为 SVE 指令（参考 SVE 的 ACLE + 带 SVE 支持的 PCS）。SVE 汇编几条提示：尽早加载寄存器、值尽量留寄存器、避免目标寄存器当下一条源、避免溢出到栈、写 VLA 兼容汇编（别硬编码循环增量）、多流水线就展开、用预取（注意缓存大小）。

### 示例：矩阵乘法（Neon → SVE）

同一个 4×4 矩阵乘法，内建函数从 Neon 换到 SVE，结构几乎不变，只换类型和函数：

| | Neon | SVE |
| --- | --- | --- |
| 向量类型 | `float32x4_t` | `svfloat32_t` |
| 加载 | `vld1q_f32` | `svld1_f32(pred, ...)` |
| 清零 | `vmovq_n_f32(0)` | `svdup_n_f32(0)` |
| 广播加载 | （靠 lane 索引） | `svld1rq_f32` |
| 融合乘加 | `vfmaq_laneq_f32` | `svmla_lane_f32` |
| 存储 | `vst1q_f32` | `svst1_f32(pred, ...)` |
| 循环谓词 | （无） | `svwhilelt_b32_u32(0, n)` |

**迁移后的关键收益**：SVE 版对 A 的**行数 n 不再有约束**（不必是 4 的倍数），因为 `svwhilelt` 谓词会防越界；Neon 版要求 n/m/k 都是 4 的倍数。

## 5. 知识自测

- **Neon/SVE 内建函数在哪些头文件？** `arm_neon.h` / `arm_sve.h`。
- **Neon 和 SVE 寄存器多大？** Neon 固定 128 位；SVE 是实现定义的 128 的倍数，最大 2048 位。
- **"编译器自动找 SIMD 机会"叫什么？** 自动向量化。

## 6. 相关资源

Neon：ACLE、Neon Programmer's Guide、Neon Intrinsics Reference。SVE：SVE 的 ARM 补充、ArmIE、SVE 的 ACLE、VLA 编程。迁移实战：SVE(2) Programmers Guide、Porting and Optimizing HPC Applications for Arm SVE。
