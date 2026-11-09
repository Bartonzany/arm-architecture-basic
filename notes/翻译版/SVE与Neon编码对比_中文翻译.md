# SVE 与 Neon 编码对比（中文翻译）

> 原文：*SVE and Neon coding compared*（文档 ID：102131，Issue 01，2020-09-14）
>
> 版权 © 2020 Arm Limited（或其关联公司）。保留所有权利。非保密。

---

## 1. 概述

本指南总结了为可扩展向量扩展（SVE）编码和为 Neon 编码之间的重要区别。对于已经把应用移植到 Armv8-A Neon 硬件的用户，本指南还重点说明把应用移植到 SVE 时需要考虑的关键差异。

Arm Neon 技术是 Armv8-A 架构档案的 Advanced 单指令多数据（SIMD）特性。Neon 是指令集架构（ISA）的一个特性，提供能在多个数据流上并行执行数学运算的指令。

SVE 是 Armv8-A 指令集的下一代 SIMD 扩展。它不是 Neon 的扩展，而是一套新的向量指令，专为 HPC 工作负载开发。简言之，SVE 能对 Neon 无法向量化（或向量化无益）的循环进行向量化。重要的是，与其他 SIMD 架构不同，SVE 可以做到向量长度无关（Vector Length Agnostic，VLA）。VLA 意味着向量寄存器的大小不是固定的。相反，硬件实现者可以自由选择最适合目标工作负载的大小。

在本指南末尾，你可以进行"知识自测"。你将学到 SVE 与 Neon 的基本区别，包括寄存器类型、谓词指令和向量长度无关编程。

## 2. 开始之前

如果你对 Arm Neon 技术陌生，请阅读《Neon Programmer's Guide for Armv8-A》作为该主题的一般性介绍。

如果你对可扩展向量扩展（SVE）陌生，请阅读我们的 Arm HPC tools for SVE 教程。该教程提供 SVE 和 SVE2 的背景信息。

## 3. 数据处理方法

处理大型数据集时，限制性能的一个主要因素是执行数据处理指令所花费的 CPU 时间。这个 CPU 时间取决于处理整个数据集所需的指令数量。指令数量取决于每条指令能处理多少个数据项。

大多数 Arm 指令是单指令单数据（SISD）。每条指令执行一次操作，写入一个输出数据流。处理多个数据项需要多条指令。

例如，用传统的 SISD 指令执行四次独立的加法运算，需要四条指令来对四对寄存器的值做加法：

```asm
ADD x0, x0, x5
ADD x1, x1, x6
ADD x2, x2, x7
ADD x3, x3, x8
```

单指令多数据（SIMD）指令同时对多个数据项执行相同的操作。这些数据项作为独立的元素打包在更大的寄存器中。

下图展示了向量寄存器 V8 和 V9 各自包含四个数据元素。加法运算同时对所有四个通道执行计算，然后把结果放入寄存器 V10。

下面的示例指令把四对单精度（32 位）值相加。不过，在这种情况下，这些值作为独立的通道打包在一对 128 位寄存器中。第一个源寄存器的每个通道与第二个源寄存器中相应的通道相加，然后存储到目标寄存器：

```asm
ADD Q8.4S, Q8.4S, Q9.4S
```

用一条 SIMD 指令执行四次操作，比用四条独立的 SISD 指令更高效。

## 4. 基础

本节介绍 Neon 和 SVE 技术的一些基本概念。

### 4.1 指令集

AArch64 是描述 Armv8 架构 64 位执行状态的名称。在 AArch64 状态下，处理器执行 A64 指令集，其中包含 Neon 指令。这些指令也被称为 Advanced SIMD 指令。SVE 扩展在架构的 Armv8.2-A 版本中引入，为现有的 Armv8-A A64 指令集添加了一个新的指令子集。

下表重点列出每个扩展提供的关键特性和指令类别：

| 扩展 | 关键特性 | 新指令分类 |
| --- | --- | --- |
| Neon | 提供能在多个数据流上并行执行数学运算的指令；支持双精度浮点算术，从而支持使用 double 的 C 代码 | 提升与降级、成对操作、加载与存储操作、逻辑运算符、乘法操作 |
| SVE | 支持宽向量和谓词寄存器（谓词的引入意味着指令可分为两大类：谓词化和非谓词化）；提供一组操作宽向量的指令；为配置和标识寄存器引入少量新增 | 加载/存储/预取指令、整数操作、向量地址计算、位运算、浮点操作、谓词操作、移动操作、归约操作 |

有关 Neon 指令集的更多信息，请参见《Arm A64 Instruction Set Architecture for Armv8-A》。

有关 SVE 指令集扩展的更多信息，请参见《Arm Architecture Reference Manual Supplement - The Scalable Vector Extension (SVE), for Armv8-A》。

### 4.2 寄存器、向量、通道和元素

Neon 单元在独立的 128 位寄存器组上操作，并完全集成到 Armv8-A 处理器中。Neon 单元使用简单的编程模型，因为它们使用与应用程序相同的地址空间。

Neon 寄存器组是一组寄存器。这些寄存器可以作为 8 位、16 位、32 位、64 位或 128 位寄存器来访问。

Neon 寄存器包含向量。向量被划分为通道（lane），每个通道包含一个称为元素（element）的数据值。向量中的所有元素都有相同的数据类型。

Neon 向量中通道的数量取决于向量的大小和向量中的数据元素。例如，128 位 Neon 向量可以包含以下元素大小：

- 十六个 8 位元素
- 八个 16 位元素
- 四个 32 位元素
- 两个 64 位元素

然而，Neon 指令总是操作 64 位或 128 位向量。

在 SVE 中，指令集操作一组新的向量和谓词寄存器：32 个 Z 寄存器、16 个 P 寄存器和一个首故障寄存器（FFR）：

- Z 寄存器是数据寄存器。Z 寄存器的位是实现定义（IMPLEMENTATION DEFINED）的 128 的倍数，架构上限为 2048 位。这些寄存器中的数据可以解释为 8 位、16 位、32 位、64 位或 128 位。每个 Z 寄存器的低 128 位与对应的 Neon 寄存器重叠，因此也与标量浮点寄存器重叠。
- P 寄存器为 Z 寄存器中的每个字节保存一位。换句话说，P 寄存器始终是 Z 寄存器宽度的 1/8。谓词化指令使用 P 寄存器来确定处理哪些向量元素。P 寄存器中的每个位指定 Z 寄存器中对应的字节是活动还是不活动。
- FFR 寄存器是一个专用谓词寄存器，捕获一串 SVE 向量加载指令的累积故障状态。SVE 为某些 SVE 向量加载指令提供首故障选项。这个选项会抑制那些不是由向量第一个活动元素引起的内存访问故障。相反，FFR 被更新以指示哪些活动向量元素没有被成功加载。

P 寄存器和 FFR 寄存器都是 SVE 独有的。

### 4.3 向量长度无关编程

SVE 引入了向量长度无关（VLA）编程的概念。

与传统 SIMD 架构定义固定的向量寄存器大小不同，SVE 只指定最大大小。这种选择自由使不同的 Arm 架构授权方能够开发自己的实现，针对可能受益于特定向量长度的特定工作负载和技术。

SVE 的一个目标是允许同一程序镜像运行在架构的任何实现上。为此，SVE 包含允许向量代码在运行时自动适应当前向量长度的指令。

有关 VLA 编程的更多信息，请参见 SVE Vector Length Agnostic programming。

## 5. 编码最佳实践

作为程序员，你有多种方式可以使用 Neon 和 SVE 技术。

用任何高级语言编程，都是在编写代码的便利性与对编译器输出的底层指令的控制力之间做权衡。

编写代码的不同选项包括：

- Neon 和 SVE 支持的数学库，如 Arm Performance Libraries。SVE 支持的库在 Arm Compiler for Linux 版本 19.3+ 中引入。
- 编译器中的自动向量化特性可以自动优化代码以利用 Neon 和 SVE。
- 内建函数是编译器会用相应 Neon 或 SVE 指令替换的函数调用。这些内建函数让你直接访问你想要的精确 Neon 或 SVE 指令。Neon 内建函数的可搜索索引见 Neon Intrinsics Reference。SVE 内建函数定义在 Arm C Language Extensions for SVE 规范中。
- 对于追求极高性能的场景，手写 Neon 或 SVE 汇编代码对有经验的程序员来说可以是一种备选方法。

## 6. 编译器优化

Arm Compiler for Linux 可以自动生成包含 Neon 和 SVE 指令的代码。让编译器自动识别代码中使用 Neon 或 SVE 指令的机会，被称为自动向量化。

自动向量化包括以下具体的编译技术：

- 循环向量化：展开循环以减少迭代次数，同时每次迭代执行更多操作。
- 超字级并行（SLP）向量化：把标量操作捆绑在一起，以使用完整宽度的 Advanced SIMD 指令。

依赖编译器自动向量化的好处包括：

- 如果没有内联汇编或内建函数等架构特定代码元素，用高级语言实现的程序是可移植的。
- 现代编译器可以自动执行高级优化。
- 针对给定微架构进行优化，可以像设置单个编译器选项一样简单。然而，在汇编中手工优化程序需要深入了解目标硬件。

下表展示如何用 Arm Compiler for Linux 为 AArch64 编译：

| 扩展 | 头文件形式 | 推荐的 Arm Compiler for Linux 命令行 | 备注 |
| --- | --- | --- | --- |
| Neon | `#include <arm_neon.h>` | `armclang -O<level> -mcpu={native\|<target>} -o <binary_name> <filename>.c` | 要利用微架构优化，把 -mcpu 设置到应用运行的处理器。如果目标处理器就是你编译代码的处理器，设置 -mcpu=native，让编译器自动检测处理器。也支持 -march=armv8-a，但不包含微架构优化。 |
| SVE | `#ifdef __ARM_FEATURE_SVE #include <arm_sve.h> #endif` | `armclang -O<level> -march=armv8-a+sve -o <binary_name> <filename>.c` | -march=armv8-a+sve 指定编译器为 Armv8-A 硬件优化。然后可以用 Arm 指令仿真器仿真 SVE 指令。在 SVE 硬件上编译时，Arm 推荐用 -mcpu=native。 |

下表展示 Neon 和 SVE 代码都支持的 -O<level> 优化级别：

| 选项 | 描述 | 自动向量化 |
| --- | --- | --- |
| -O0 | 编译二进制的最小优化。关闭大多数优化。调试时，此选项生成直接对应源代码的代码。此选项可能导致显著更大的镜像。这是默认优化级别。 | 从不 |
| -O1 | 受限优化。调试时，此选项在镜像大小、性能和调试之间给出最佳调试视图。 | 默认禁用 |
| -O2 | 高优化。调试时，调试视图可能不太令人满意。 | 默认启用 |
| -O3 | 极高优化。调试时，此选项通常给出较差的调试视图。Arm 推荐在较低优化级别调试。 | 默认启用 |
| -Ofast | 启用级别 3 的所有优化，包括用 -ffp-mode=fast armclang 选项执行的优化。此级别还执行可能违反严格语言标准合规性的其他激进优化。 | 默认启用 |

自动向量化在优化级别 -O2 及更高时默认启用。-fno-vectorize 选项可以禁用自动向量化。

在优化级别 -O0，自动向量化始终禁用。如果指定 -fvectorize 选项，编译器会忽略它。

在优化级别 -O1，自动向量化默认禁用。-fvectorize 选项可以启用自动向量化。

随着实现变得越来越复杂，编译器能自动向量化代码的可能性会降低。例如，具有以下特征的循环特别难或不可能向量化：

- 不同循环迭代之间存在相互依赖的循环
- 带有 break 子句的循环
- 带有复杂条件的循环

Neon 和 SVE 有不同的自动向量化条件。例如，自动向量化 Neon 代码的一个必要条件是，循环大小的迭代次数必须在循环开始时、在执行时已知。然而，自动向量化 SVE 代码不要求知道循环大小的迭代次数。

break 条件意味着循环大小可能在循环开始时无法得知，这会阻止 Neon 代码的自动向量化。如果无法完全避免 break 条件，考虑把循环拆分成多个可向量化和不可向量化的部分。

用于控制循环向量化的编译器指令的完整讨论可以在 LLVM-Clang 文档中找到。两个最重要的指令是：

- `#pragma clang loop vectorize(enable)`
- `#pragma clang loop interleave(enable)`

这些 pragma 是给编译器的提示，分别执行 SLP 和循环向量化。

## 7. 内建函数

内建函数是编译器知道其精确实现的函数。内建函数让你无需编写汇编代码就能使用 Neon 或 SVE。这是因为内建函数本身包含简短的汇编内核，会被内联到调用代码中。此外，寄存器分配和流水线优化由编译器处理，避免了开发汇编代码时经常出现的许多困难。

使用内建函数有几个好处：

- 强大：使用内建函数，你在开发期间直接访问 Neon 和 SVE 指令集，无需手写汇编代码。
- 可移植：手写的 Neon 或 SVE 汇编指令可能需要针对不同目标处理器重写。包含 Neon 内建函数的 C 和 C++ 代码，可以以极少或无需改动为新 AArch64 目标或新执行状态编译。然而，包含 SVE 内建函数的 C 和 C++ 代码只能在 SVE 硬件上运行。
- 灵活：你可以只在需要时利用 Neon 或 SVE。

然而，内建函数并非在所有情况下都合适：

- 使用内建函数比导入库或依赖编译器需要更多学习。
- 手写优化的汇编代码虽然更难写，但可能提供最大的性能提升空间。

所有 Neon 内建函数的列表见 Neon intrinsics interactive reference。Neon 内建函数的工程规范包含在 Arm C Language Extensions（ACLE）规范中。SVE 内建函数的工程规范包含在 Arm C Language Extensions for SVE 规范中。

### 7.1 示例：用内建函数做简单矩阵乘法

这个示例用 Neon 内建函数和 SVE 内建函数实现一些 C 函数。该示例不展示应用的完整复杂性，但展示了内建函数的用法，是编写更复杂代码的起点。

矩阵乘法是许多数据密集型应用中执行的操作。矩阵乘法由一组以简单方式重复的算术运算组成。

矩阵乘法的过程如下：

1. 取矩阵 A 中的一行。
2. 用矩阵 B 中的一列对该行做点积。
3. 把结果存储到新矩阵 C 的相应行和列。

对于 32 位浮点矩阵，乘法可以写成如下代码：

```c
void matrix_multiply_c(float32_t *A, float32_t *B, float32_t *C, uint32_t n, uint32_t m, uint32_t k) {
    for (int i_idx=0; i_idx < n; i_idx++) {
        for (int j_idx=0; j_idx < m; j_idx++) {
            C[n*j_idx + i_idx] = 0;
            for (int k_idx=0; k_idx < k; k_idx++) {
                C[n*j_idx + i_idx] += A[n*k_idx + i_idx]*B[k*j_idx + k_idx];
            }
        }
    }
}
```

假设矩阵在内存中按列主序布局。也就是说，一个 n × m 矩阵 M 表示为一个数组 M_array，其中 Mij = M_array[n*j + i]。

这段代码是次优的，因为它没有充分利用 Neon。内建函数可以用来提升性能。

下面的代码使用内建函数乘两个 4×4 矩阵。由于要处理的值数量小且固定，循环可以完全展开。所有值都能同时装入处理器的 Neon 寄存器：

```c
void matrix_multiply_4x4_neon(const float32_t *A, const float32_t *B, float32_t *C) {
    // 这些是 A 的列
    float32x4_t A0, A1, A2, A3;
    // 这些是 B 的列
    float32x4_t B0, B1, B2, B3;
    // 这些是 C 的列
    float32x4_t C0, C1, C2, C3;

    A0 = vld1q_f32(A);
    A1 = vld1q_f32(A+4);
    A2 = vld1q_f32(A+8);
    A3 = vld1q_f32(A+12);

    // 清零 C 值的累加器
    C0 = vmovq_n_f32(0);
    C1 = vmovq_n_f32(0);
    C2 = vmovq_n_f32(0);
    C3 = vmovq_n_f32(0);

    // 按 4x1 块做乘加，即 C 的每一列
    B0 = vld1q_f32(B);
    C0 = vfmaq_laneq_f32(C0, A0, B0, 0);
    C0 = vfmaq_laneq_f32(C0, A1, B0, 1);
    C0 = vfmaq_laneq_f32(C0, A2, B0, 2);
    C0 = vfmaq_laneq_f32(C0, A3, B0, 3);
    vst1q_f32(C, C0);

    B1 = vld1q_f32(B+4);
    C1 = vfmaq_laneq_f32(C1, A0, B1, 0);
    C1 = vfmaq_laneq_f32(C1, A1, B1, 1);
    C1 = vfmaq_laneq_f32(C1, A2, B1, 2);
    C1 = vfmaq_laneq_f32(C1, A3, B1, 3);
    vst1q_f32(C+4, C1);

    B2 = vld1q_f32(B+8);
    C2 = vfmaq_laneq_f32(C2, A0, B2, 0);
    C2 = vfmaq_laneq_f32(C2, A1, B2, 1);
    C2 = vfmaq_laneq_f32(C2, A2, B2, 2);
    C2 = vfmaq_laneq_f32(C2, A3, B2, 3);
    vst1q_f32(C+8, C2);

    B3 = vld1q_f32(B+12);
    C3 = vfmaq_laneq_f32(C3, A0, B3, 0);
    C3 = vfmaq_laneq_f32(C3, A1, B3, 1);
    C3 = vfmaq_laneq_f32(C3, A2, B3, 2);
    C3 = vfmaq_laneq_f32(C3, A3, B3, 3);
    vst1q_f32(C+12, C3);
}
```

选择固定大小 4×4 矩阵是因为：

- 某些应用特别需要 4×4 矩阵，例如图形或相对论物理。
- Neon 向量寄存器保存四个 32 位值。让程序匹配架构更容易优化。
- 这个 4×4 内核可以用在更通用的内核中。

下表总结了这个例子中使用的 Neon 内建函数：

| 代码元素 | 它是什么？ | 为什么使用它？ |
| --- | --- | --- |
| float32x4_t | 四个 32 位浮点数组成的数组。 | 一个 float32x4_t 装入一个 128 位寄存器，确保没有浪费的寄存器位。 |
| vld1q_f32() | 把四个 32 位浮点数加载到 float32x4_t 的函数。 | 从 A 和 B 获取所需的矩阵值。 |
| vfmaq_lane_f32() | 使用融合乘加指令的函数。把 float32x4_t 值乘以另一个 float32x4_t 的单个元素，再加到第三个 float32x4_t 上。 | 行乘列点积是一组乘法和加法，这个操作天然契合。 |
| vst1q_f32() | 在给定地址存储 float32x4_t 的函数。 | 计算结果后存储结果。 |

为 SVE 优化类似情况得到以下代码（原文函数名 matrix_multiply_nx4_neon 系笔误，应为 matrix_multiply_nx4_sve）：

```c
void matrix_multiply_nx4_sve(const float32_t *A, const float32_t *B, float32_t *C, uint32_t n) {
    // 这些是 A 的列
    svfloat32_t A0, A1, A2, A3;
    // 这些是 B 的列
    svfloat32_t B0, B1, B2, B3;
    // 这些是 C 的列
    svfloat32_t C0, C1, C2, C3;

    svbool_t pred = svwhilelt_b32_u32(0, n);
    A0 = svld1_f32(pred, A);
    A1 = svld1_f32(pred, A+n);
    A2 = svld1_f32(pred, A+2*n);
    A3 = svld1_f32(pred, A+3*n);

    // 清零 C 值的累加器
    C0 = svdup_n_f32(0);
    C1 = svdup_n_f32(0);
    C2 = svdup_n_f32(0);
    C3 = svdup_n_f32(0);

    // 按 4x1 块做乘加，即 C 的每一列
    B0 = svld1rq_f32(svptrue_b32(), B);
    C0 = svmla_lane_f32(C0, A0, B0, 0);
    C0 = svmla_lane_f32(C0, A1, B0, 1);
    C0 = svmla_lane_f32(C0, A2, B0, 2);
    C0 = svmla_lane_f32(C0, A3, B0, 3);
    svst1_f32(pred, C, C0);

    B1 = svld1rq_f32(svptrue_b32(), B+4);
    C1 = svmla_lane_f32(C1, A0, B1, 0);
    C1 = svmla_lane_f32(C1, A1, B1, 1);
    C1 = svmla_lane_f32(C1, A2, B1, 2);
    C1 = svmla_lane_f32(C1, A3, B1, 3);
    svst1_f32(pred, C+4, C1);

    B2 = svld1rq_f32(svptrue_b32(), B+8);
    C2 = svmla_lane_f32(C2, A0, B2, 0);
    C2 = svmla_lane_f32(C2, A1, B2, 1);
    C2 = svmla_lane_f32(C2, A2, B2, 2);
    C2 = svmla_lane_f32(C2, A3, B2, 3);
    svst1_f32(pred, C+8, C2);

    B3 = svld1rq_f32(svptrue_b32(), B+12);
    C3 = svmla_lane_f32(C3, A0, B3, 0);
    C3 = svmla_lane_f32(C3, A1, B3, 1);
    C3 = svmla_lane_f32(C3, A2, B3, 2);
    C3 = svmla_lane_f32(C3, A3, B3, 3);
    svst1_f32(pred, C+12, C3);
}
```

下表总结了这个例子中使用的 SVE 内建函数：

| 代码元素 | 它是什么？ | 为什么使用它？ |
| --- | --- | --- |
| svfloat32_t | 32 位浮点数组，其确切数量在运行时根据 SVE 向量长度定义。 | 让你直接使用 SVE 向量和谓词，而不依赖编译器自动向量化。 |
| svwhilelt_b32_u32() | 从两个 uint32_t 整数计算谓词的函数。 | 从 A 加载、向 C 存储时，确保不会越过每列的末尾。 |
| svld1_f32() | 把 32 位 svfloat32_t 浮点加载进 SVE 向量的函数。 | 从 A 获取矩阵值；接受谓词确保不越过矩阵末尾，无谓词元素置零。 |
| svptrue_b32() | 把 32 位值的谓词设为全真的函数。 | 从 B 加载时确保向量完全填满（前提是矩阵维度是四的倍数）。 |
| svld1rq_f32() | 用相同 128 位（四个 32 位值）的副本加载 SVE 向量的函数。 | 从 B 获取矩阵值；只加载四个复制值，因为 svmla_lane_f32 只按 128 位段索引。 |
| svmla_lane_f32() | 使用融合乘加指令的函数。 | 行乘列点积天然契合。 |
| svst1_f32() | 在给定地址存储 svfloat32_t 的函数。 | 计算结果后存储结果；谓词确保不越过每列末尾。 |

SVE 代码的重要区别是，由于可变长度向量，可以忽略矩阵的一个维度。相反，你可以显式传递 n 维度的长度，并用谓词确保不超过最大长度。

### 7.2 示例：用内建函数做大型矩阵乘法

要乘更大的矩阵，可以把它们当作 4×4 矩阵的块来处理。然而，这种方法只适用于两个维度都是四的倍数的矩阵大小。要对其他矩阵大小使用此方法，可以给矩阵补零。

下面的代码块展示了更通用矩阵乘法的 Neon 代码。内核的结构变了，最重要的变化是增加了循环和地址计算。与 4×4 内核一样，为 B 的列使用唯一的变量名（替代方案是用一个变量并重新加载）。这是给编译器的提示，让它为这些变量分配不同的寄存器，使处理器能在等一列加载时完成另一列的算术指令：

```c
void matrix_multiply_neon(const float32_t *A, const float32_t *B, float32_t *C,
                          uint32_t n, uint32_t m, uint32_t k) {
    int a_idx, b_idx, c_idx;
    float32x4_t A0, A1, A2, A3;
    float32x4_t B0, B1, B2, B3;
    float32x4_t C0, C1, C2, C3;

    for (int i_idx=0; i_idx<n; i_idx+=4) {
        for (int j_idx=0; j_idx<m; j_idx+=4) {
            C0 = vmovq_n_f32(0);
            C1 = vmovq_n_f32(0);
            C2 = vmovq_n_f32(0);
            C3 = vmovq_n_f32(0);
            for (int k_idx=0; k_idx<k; k_idx+=4){
                a_idx = i_idx + n*k_idx;
                b_idx = k*j_idx + k_idx;
                A0 = vld1q_f32(A+a_idx);
                A1 = vld1q_f32(A+a_idx+n);
                A2 = vld1q_f32(A+a_idx+2*n);
                A3 = vld1q_f32(A+a_idx+3*n);
                B0 = vld1q_f32(B+b_idx);
                C0 = vfmaq_laneq_f32(C0,A0,B0,0);
                C0 = vfmaq_laneq_f32(C0,A1,B0,1);
                C0 = vfmaq_laneq_f32(C0,A2,B0,2);
                C0 = vfmaq_laneq_f32(C0,A3,B0,3);
                B1 = vld1q_f32(B+b_idx+k);
                C1 = vfmaq_laneq_f32(C1,A0,B1,0);
                C1 = vfmaq_laneq_f32(C1,A1,B1,1);
                C1 = vfmaq_laneq_f32(C1,A2,B1,2);
                C1 = vfmaq_laneq_f32(C1,A3,B1,3);
                B2 = vld1q_f32(B+b_idx+2*k);
                C2 = vfmaq_laneq_f32(C2,A0,B2,0);
                C2 = vfmaq_laneq_f32(C2,A1,B2,1);
                C2 = vfmaq_laneq_f32(C2,A2,B2,2);
                C2 = vfmaq_laneq_f32(C2,A3,B2,3);
                B3 = vld1q_f32(B+b_idx+3*k);
                C3 = vfmaq_laneq_f32(C3,A0,B3,0);
                C3 = vfmaq_laneq_f32(C3,A1,B3,1);
                C3 = vfmaq_laneq_f32(C3,A2,B3,2);
                C3 = vfmaq_laneq_f32(C3,A3,B3,3);
            }
            c_idx = n*j_idx + i_idx;
            vst1q_f32(C+c_idx, C0);
            vst1q_f32(C+c_idx+n, C1);
            vst1q_f32(C+c_idx+2*n, C2);
            vst1q_f32(C+c_idx+3*n, C3);
        }
    }
}
```

编译并反汇编这个函数，与 C 函数对比，可以看到以下差异：

- 给定矩阵乘法的算术指令更少，因为函数利用 Advanced SIMD 技术做了完整的寄存器打包。一般 C 代码通常不这么做。
- FMLA 指令而不是 FMUL 指令，正如内建函数所指定。
- 循环迭代更少。正确使用内建函数时，循环可以轻松展开。

然而，反汇编代码也揭示了由于数据类型（如 float32x4_t）的内存分配和初始化而产生的不必要加载和存储。这些数据类型在非内建函数的 C 代码中不使用。

为 SVE 优化此代码得到以下代码：

```c
void matrix_multiply_sve(const float32_t *A, const float32_t *B, float32_t *C, uint32_t n, uint32_t m, uint32_t k) {
    int a_idx, b_idx, c_idx;
    svfloat32_t A0, A1, A2, A3;
    svfloat32_t B0, B1, B2, B3;
    svfloat32_t C0, C1, C2, C3;

    for (int i_idx=0; i_idx<n; i_idx+=svcntw()) {
        svbool_t pred = svwhilelt_b32_u32(i_idx, n);
        for (int j_idx=0; j_idx<m; j_idx+=4) {
            C0 = svdup_n_f32(0);
            C1 = svdup_n_f32(0);
            C2 = svdup_n_f32(0);
            C3 = svdup_n_f32(0);
            for (int k_idx=0; k_idx<k; k_idx+=4){
                a_idx = i_idx + n*k_idx;
                b_idx = k*j_idx + k_idx;
                A0 = svld1_f32(pred, A+a_idx);
                A1 = svld1_f32(pred, A+a_idx+n);
                A2 = svld1_f32(pred, A+a_idx+2*n);
                A3 = svld1_f32(pred, A+a_idx+3*n);
                B0 = svld1rq_f32(svptrue_b32(), B+b_idx);
                C0 = svmla_lane_f32(C0,A0,B0,0);
                C0 = svmla_lane_f32(C0,A1,B0,1);
                C0 = svmla_lane_f32(C0,A2,B0,2);
                C0 = svmla_lane_f32(C0,A3,B0,3);
                B1 = svld1rq_f32(svptrue_b32(), B+b_idx+k);
                C1 = svmla_lane_f32(C1,A0,B1,0);
                C1 = svmla_lane_f32(C1,A1,B1,1);
                C1 = svmla_lane_f32(C1,A2,B1,2);
                C1 = svmla_lane_f32(C1,A3,B1,3);
                B2 = svld1rq_f32(svptrue_b32(), B+b_idx+2*k);
                C2 = svmla_lane_f32(C2,A0,B2,0);
                C2 = svmla_lane_f32(C2,A1,B2,1);
                C2 = svmla_lane_f32(C2,A2,B2,2);
                C2 = svmla_lane_f32(C2,A3,B2,3);
                B3 = svld1rq_f32(svptrue_b32(), B+b_idx+3*k);
                C3 = svmla_lane_f32(C3,A0,B3,0);
                C3 = svmla_lane_f32(C3,A1,B3,1);
                C3 = svmla_lane_f32(C3,A2,B3,2);
                C3 = svmla_lane_f32(C3,A3,B3,3);
            }
            c_idx = n*j_idx + i_idx;
            svst1_f32(pred, C+c_idx, C0);
            svst1_f32(pred, C+c_idx+n, C1);
            svst1_f32(pred, C+c_idx+2*n, C2);
            svst1_f32(pred, C+c_idx+3*n, C3);
        }
    }
}
```

这段代码与之前的 Neon 代码几乎相同，只是内建函数不同。此外，由于谓词化，对 A 的行数不再有约束。然而，你必须确保 A 和 C 的列数、以及 B 的两个维度都是四的倍数。这是因为这段代码使用的谓词没有考虑这一点。添加更多谓词是可能的，但会降低这个例子的清晰度。

把这个例子与 C 函数和 Neon 函数对比，可以发现 SVE 例子：

- 使用 WHILELT 为外层循环的每次迭代确定谓词，保证循环条件始终至少有一个元素要处理。
- 用 CNTW（向量中 32 位元素的数量）递增 i_idx，避免硬编码外层循环一次迭代处理的元素数量。

## 8. 编程约定：宏、类型和函数

Arm C Language Extensions（ACLE）让 C/C++ 程序员能在对源代码可移植性限制最小的情况下利用 Arm 架构。ACLE 包含一组宏、类型和函数，使 Arm 架构的特性可直接用于 C 和 C++ 程序。

本节概述这些特性。

### 8.1 宏

特性测试宏允许程序员确定目标架构特性的可用性。例如，要使用 Neon 或 SVE 内建函数，目标平台必须支持 Advanced SIMD 或 SVE 架构。当宏被定义且等于 1 时，对应特性可用。

#### 8.1.1 Neon 宏

| 宏 | 特性 |
| --- | --- |
| __aarch64__ | 编译时选择架构相关源码。AArch64 下始终为 1。 |
| _ARM_NEON | 编译器支持 Advanced SIMD。AArch64 下始终为 1。 |
| _ARM_NEON_FP | 支持 Neon 浮点运算。AArch64 下始终为 1。 |
| _ARM_FEATURE_CRYPTO | 加密指令可用。 |
| _ARM_FEATURE_FMA | 融合乘加指令可用。 |

#### 8.1.2 SVE 宏

| 宏 | 特性 |
| --- | --- |
| _ARM_FEATURE_SVE | 支持 SVE 时始终为 1。SVE 指令和 SVE 内建函数可用。 |

### 8.2 数据类型

ACLE 定义了几种支持 SIMD 处理的数据类型。Neon 和 SVE 的这些数据类型不同。

#### 8.2.1 Neon 数据类型

Neon 的 arm_neon.h 中有三大类数据类型，命名模式如下：

| 数据类型 | 描述 |
| --- | --- |
| <base><W>_t | 标量数据类型。例如 int64_t。 |
| <base><W>x<L>_t | 向量数据类型。例如 int32x2_t。 |
| <base><W>x<L>x<N>_t | 向量数组数据类型。例如 int16x4x2_t。 |

其中 <base> 是基础数据类型，<W> 是基础类型宽度，<L> 是向量数据类型中标量实例数，<N> 是向量数组类型中向量实例数。通常 <W> 和 <L> 使向量数据类型为 64 或 128 位长。

#### 8.2.2 SVE 数据类型

SVE 没有直接映射 SVE 向量或谓词概念的现有机制。ACLE 把 SVE 向量和谓词归为一种称为无大小（sizeless）数据类型的新类别。无大小数据类型由向量类型和谓词类型组成，前缀为 sv，例如 svint64_t。

| 数据类型 | 描述 |
| --- | --- |
| <base><W>_t | 标量数据类型。SVE 增加对 float16_t、float32_t 和 float64_t 的支持。 |
| sv<base><W>_t | 单个向量的无大小标量数据类型。例如 svint64_t。 |
| sv<base><W>x<N>_t | 两个、三个和四个向量的无大小向量数据类型。例如 svint64x2_t。 |
| svbool_t | 无大小单谓词数据类型，有足够位控制字节向量上的操作。 |

### 8.3 函数

Neon 和 SVE 内建函数分别在头文件 arm_neon.h 和 arm_sve.h 中作为函数原型提供。这些函数遵循通用的命名模式。

#### 8.3.1 Neon 函数

Neon 的 arm_neon.h 函数原型遵循通用命名模式（与 ACLE 类似）：

```
ret v[p][q][r]name[u][n][q][x][_high][_lane | laneq][_n][_result]_type(args)
```

例如 `int8x16_t vmulq_s8 (int8x16_t a, int8x16_t b)` 对应指令 `MUL Vd.16B, Vn.16B, Vm.16B`。各模式元素的含义（v/p/q/r/name/u/n/q/x/_high/_n/_lane/type/args）详见原文及《用 Neon 内建函数优化 C 代码》第 7 章。

#### 8.3.2 SVE 函数

SVE 的 arm_sve.h 函数原型遵循通用模式：

```
svbase[_disambiguator][_type0][_type1]...[_predication]
```

| 模式元素 | 描述 |
| --- | --- |
| base | SVE 指令的小写名称（略作调整）。 |
| _disambiguator | 区分函数的不同形式。 |
| _type0\|_type1\|... | 列出向量和谓词的类型，从返回类型开始，然后是参数类型。 |
| _predication | 描述谓词化操作结果中不活动元素的后缀。可以是 z（清零谓词）、m（合并谓词）或 x（"不关心"谓词）。 |

## 9. 知识自测

**Q. 哪些头文件定义了 Neon 内建函数和 SVE 内建函数？**

A. Neon 内建函数和 SVE 内建函数分别在头文件 arm_neon.h 和 arm_sve.h 中作为函数原型提供。

**Q. Neon 和 SVE 向量数据寄存器的大小是多少？**

A. Neon 寄存器是 128 位寄存器。SVE 不为其向量寄存器定义固定大小。SVE 向量寄存器是实现定义的 128 的倍数，架构上限为 2048 位。

**Q. 描述让编译器自动识别代码中使用 Neon 或 SVE 指令机会的术语是什么？**

A. 自动向量化。

## 10. 相关信息

Neon 相关资源：Arm Neon technology、ACLE、Neon Programmer's Guide for Armv8-A、架构探索工具、Armv8-A 的 ARM、Neon Intrinsics Reference。

SVE 相关资源：SVE 的 ARM 补充、Arm HPC tools for SVE、Arm 指令仿真器、SVE 的 ACLE、SVE Vector Length Agnostic programming、What is the Scalable Vector Extension?

其他资源：Arm C/C++ Compiler Reference、Arm Fortran Compiler Reference、Arm Performance Libraries、编译器标志博客、LLVM-Clang 文档。

## 11. 后续步骤

在本指南中，我们对比了为 SVE 编码和为 Neon 编码之间的重要区别。

如果你熟悉 Neon 并想把应用移植到 SVE，你现在应该理解需要考虑的问题。有关把应用移植到 Arm 并为 Arm SVE 优化的更多信息，请参见 Porting and Optimizing HPC Applications for Arm SVE。
