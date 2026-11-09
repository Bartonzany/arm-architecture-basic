# 读懂 SVE 与 Neon 编码对比（中文精读版）

> 原文：*SVE and Neon coding compared*（文档 ID：102131，Issue 01，2020-09-14）
>
> 版权 © 2020 Arm Limited（或其关联公司）。保留所有权利。非保密。
>
> 说明：本文在原中文翻译的基础上重新组织了叙述逻辑与表达方式，技术内容、表格与图表保持原意；个别明显笔误已按 Arm 架构规范订正。

---

## 导读：这篇文章讲什么

这篇是给**已经会用 Neon、想转 SVE** 的人看的对照表。核心就一句话：**Neon 的向量固定 128 位，SVE 的向量长度可扩展（128～2048 位）**。这个差异往下展开成三件事——**寄存器不一样**（Neon 只有 V 向量，SVE 多了 P 谓词和 FFR）、**编程方式不一样**（SVE 是"向量长度无关"编程）、**内建函数/类型/宏不一样**（Neon 的 `float32x4_t` vs SVE 的 `svfloat32_t`）。

读完你要能：说清两者的寄存器差异、为什么 SVE 能向量化 Neon 向量化不了的循环、以及内建函数命名的对应关系。

## 1. 一句话区别：固定 vs 可扩展

- **Neon**：Armv8-A 的 Advanced SIMD，指令总是操作 **64 位或 128 位**固定向量。
- **SVE**：下一代 SIMD（Armv8.2-A 引入），**不是 Neon 的扩展**，而是一套专为 HPC 设计的新向量指令。关键是**向量长度无关（VLA）**——向量寄存器大小不固定，硬件厂商自由选。

VLA 带来一个 Neon 做不到的能力：**同一份二进制跑在任何 SVE 硬件上**（不管向量多宽），因为 SVE 有"运行时自适应向量长度"的指令。

一个具体差异：**自动向量化的前提条件不同**。Neon 向量化要求"循环迭代次数在循环开始时已知"；SVE **不要求**——所以带 break 的循环、动态长度的循环，Neon 向量化不了，SVE 可以。

## 2. 寄存器差异

| | Neon | SVE |
| --- | --- | --- |
| 数据寄存器 | 128 位 V 寄存器（可拆 8/16/32/64/128 位访问） | 32 个 Z 寄存器（128 的倍数，最大 2048 位） |
| 谓词寄存器 | 无 | 16 个 P 寄存器（每字节一位，Z 宽度的 1/8） |
| 首故障寄存器 | 无 | 1 个 FFR（累积首故障加载的故障状态） |

关键点：

- **Z 寄存器的低 128 位与 Neon V 寄存器重叠**（也和标量浮点寄存器重叠）——同一块物理寄存器，Neon 和 SVE 可以互换读写。
- **P 谓词和 FFR 是 SVE 独有的**。P 寄存器每 bit 对应 Z 的一个字节，决定该通道"活动/不活动"——这就是 SVE 能"忽略矩阵一个维度"的根源。
- FFR 配合首故障加载：加载时如果某个活动元素越界，不报错，而是把 FFR 对应位置标记出来，其余元素正常加载。

## 3. 向量长度无关（VLA）编程

传统 SIMD 固定寄存器大小；SVE 只规定**最大** 2048 位，实现者自选。目标是"**一次编译、处处运行**"。做法是用 `svcntw()`（当前向量能装多少个 32 位元素）在运行时算循环步进，用 `svwhilelt`（WHILELT）按"还剩多少元素"生成谓词，避免硬编码 128 位。

## 4. 内建函数对比：矩阵乘法示例

文档用矩阵乘法演示 Neon 和 SVE 内建函数的对应关系。同一个算法（4×4 分块 + 融合乘加），两种写法：

| | Neon | SVE |
| --- | --- | --- |
| 向量类型 | `float32x4_t`（固定 4 个 32 位） | `svfloat32_t`（无大小，运行时定） |
| 加载 | `vld1q_f32` | `svld1_f32(pred, ...)`（带谓词） |
| 广播加载 | （无对应，靠 lane 索引） | `svld1rq_f32`（复制 128 位段） |
| 融合乘加 | `vfmaq_lane_f32` | `svmla_lane_f32` |
| 存储 | `vst1q_f32` | `svst1_f32(pred, ...)`（带谓词） |
| 全真谓词 | （无） | `svptrue_b32()` |
| 循环边界谓词 | （无） | `svwhilelt_b32_u32(i, n)` |

**SVE 版的核心差异**：外层循环用 `i_idx += svcntw()` 按运行时向量长度步进，配合 `svwhilelt` 生成谓词——所以**对 A 的行数没有约束**（n 不必是 4 的倍数）。而 Neon 版必须保证各维度是 4 的倍数（否则要补零）。

一句话记住：**Neon 把"向量有多宽"写死在类型里（float32x4_t），SVE 把它留给运行时（svfloat32_t + 谓词）**。

## 5. 编程约定差异

**宏**：Neon 用 `_ARM_NEON`、`_ARM_NEON_FP`、`_ARM_FEATURE_CRYPTO`、`_ARM_FEATURE_FMA`；SVE 用 `_ARM_FEATURE_SVE`（一个宏，恒 1 表示 SVE 可用）。用 `#ifdef __ARM_FEATURE_SVE` 包住 `#include <arm_sve.h>`。

**类型**：
- Neon：`<base><W>x<L>_t`，如 `int32x2_t`（固定 64/128 位）。
- SVE：无大小（sizeless）类型 `sv<base><W>_t`，如 `svint64_t`、`svbool_t`、`svint64x2_t`。无大小是必须的——因为编译时不知道向量多长。

**函数命名**：
- Neon：`v[p][q][r]name[u][n][q][x][_high][_lane|_laneq][_n][_result]_type`，如 `vmulq_s8`。
- SVE：`svbase[_disambiguator][_type0][_type1]...[_predication]`，如 `svclz_u16_m`。末尾 `_predication` 是 `z`（清零）/`m`（合并）/`x`（不关心），描述不活动元素怎么处理。

**编译命令**：
- Neon：`armclang -O<level> -mcpu=native -o out file.c`
- SVE：`armclang -O<level> -march=armv8-a+sve -o out file.c`（无 SVE 硬件时用 Arm 指令仿真器跑）

## 6. 知识自测

- **Neon/SVE 内建函数在哪些头文件？** `arm_neon.h` / `arm_sve.h`。
- **Neon 和 SVE 寄存器多大？** Neon 固定 128 位；SVE 是实现定义的 128 的倍数，最大 2048 位。
- **"编译器自动找 SIMD 机会"叫什么？** 自动向量化。

## 7. 相关资源与下一步

Neon：Neon Programmer's Guide、ACLE、架构探索工具、Neon Intrinsics Reference。SVE：SVE 的 ARM 补充、Arm HPC tools for SVE、SVE 的 ACLE、VLA 编程。想移植 HPC 应用看《Porting and Optimizing HPC Applications for Arm SVE》。
