# Learn the architecture — 为 Neon 自动向量化编译（中文翻译）

> 原文：*Learn the architecture - Compiling for Neon with auto-vectorization*
>
> 版本：Version 1.0　|　文档 ID：102525_0100_00_en　|　Issue 00（2019-06-18）
>
> 版权 © 2019 Arm Limited（或其关联公司）。保留所有权利。非保密。

---

## 1. 概述

作为程序员，你有多种方式可以利用 Neon 技术：

- 支持 Neon 的开源库（如 Arm Compute Library）是利用 Neon 最简单的方式之一。
- 编译器中的自动向量化特性可以自动优化你的代码以利用 Neon。
- Neon 内建函数是编译器会用相应 Neon 指令替换的函数调用。这让你可以直接、底层地访问你想要的精确 Neon 指令，而且全部通过 C/C++ 代码完成。
- 对于追求极高性能的场景，手写 Neon 汇编对有经验的程序员来说可以是一种备选方法。

本指南展示如何使用 Arm Compiler 6 中的自动向量化特性，来自动生成包含 Armv8 Advanced SIMD 指令的代码。它包含若干探索 Neon 代码生成的示例，并强调帮助编译器产出最佳结果的编码最佳实践。

本指南对每个从事 Arm 开发的人都有用，尤其对那些想使用 Neon 技术但不必用汇编编程的人更有用。

在本指南末尾，你将达成以下目标：

- 知道哪些 Arm Compiler 命令行选项能启用 Advanced SIMD 代码生成。
- 能够编写利用 Arm Compiler 6 各种优化特性的 C/C++ 代码。
- 知道在哪里能找到不同编译器的文档。

如果你还不熟悉 Neon，应在开始本指南之前阅读《Introducing Neon for Armv8-A》。

本指南中的示例使用 Arm Compiler 6，它是为运行在裸机设备上的嵌入式应用开发而设计的。如果你还没有 Arm Compiler 6，它包含在 Arm Development Studio Gold Edition 的 30 天免费试用中。

尽管本指南使用 Arm Compiler 6，你仍然可以轻松地为其他编译器改编这些示例。你需要查阅编译器文档，找出示例中等价的编译器选项。能够生成 Neon 代码的自动向量化编译器包括：

- Arm Compiler 6，为运行在裸机设备上的嵌入式应用开发而设计。这是本指南示例中使用的编译器。
- Arm C/C++ Compiler，为 Linux 用户空间应用开发而设计，最初用于高性能计算。
- LLVM-clang，开源的基于 LLVM 的工具链。
- GCC，开源的 GNU 工具链。

## 2. 为什么依赖编译器做自动向量化？

编写手工优化的汇编内核，或编写包含 Neon 内建函数的 C 代码，能对你的软件中的 Neon 代码提供很高的控制力。然而，这些方法会带来显著的可移植性和工程复杂性成本。

在许多情况下，一个高质量的编译器能生成同样好的代码，但需要的设计时间却少得多。让编译器自动识别代码中使用 Advanced SIMD 指令的机会，这个过程被称为自动向量化（auto-vectorization）。

就具体的编译技术而言，自动向量化包括：

- 循环向量化（Loop vectorization）：展开循环以减少迭代次数，同时每次迭代执行更多操作。
- 超字级并行（Superword-Level Parallelism，SLP）向量化：把标量操作捆绑在一起，以利用完整宽度的 Advanced SIMD 指令。

自动向量化编译器包括 Arm Compiler 6、Arm C/C++ Compiler、LLVM-clang 和 GCC。

依赖编译器自动向量化的好处包括：

- 用高级语言实现的程序是可移植的，只要没有架构特定的代码元素（如内联汇编或内建函数）。
- 现代编译器能够自动执行高级优化。
- 针对给定的微架构进行优化，可以像设置单个编译器选项一样简单；而优化汇编程序则需要深入了解目标硬件。

然而，自动向量化并非在所有情况下都合适：

- 虽然源代码可以做到架构无关，但为了获得最佳的代码生成，它可能需要针对特定编译器。
- 高级语言或编译器选项的微小变化，可能导致生成代码的显著且不可预测的变化。

使用编译器生成 Neon 代码对大多数项目都是合适的。只有当生成的代码无法提供必要性能，或特定硬件特性不受高级语言支持时，才需要其他利用 Neon 的方法。例如，配置系统寄存器以控制浮点功能，就必须在汇编代码中完成。

## 3. 用 Arm Compiler 6 为 Neon 编译

要启用自动向量化，你必须指定适当的编译器选项来完成以下工作：

- 以具有 Neon 能力的处理器为目标。
- 指定包含自动向量化的优化级别。

此外，指定 `-Rpass=loop` 编译器选项会显示编译器关于它如何优化特定循环的有用诊断信息。这些信息包括向量化宽度和交错计数。

> `-Rpass=loop` 是 Arm Compiler 的一个 [COMMUNITY] 特性。

**指定具有 Neon 能力的目标**

Neon 在所有标准 Armv8-A 实现中都是必需的，所以以任何 Armv8-A 架构或处理器为目标，都允许生成 Neon 代码。

如果你只想在某个特定处理器上运行代码，可以以该特定处理器为目标。性能会针对该处理器的微架构特性进行优化，但代码只保证在该处理器上运行。

如果你想让代码在广泛的处理器上运行，可以以架构为目标。生成的代码能运行在该目标架构的任何处理器实现上，但性能可能受到影响。

以 Armv8-A AArch64 状态为目标：

```shell
armclang --target=aarch64-arm-none-eabi
```

以 AArch32 状态下的 Cortex-A53 为目标：

```shell
armclang --target=arm-arm-none-eabi -mcpu=cortex-a53
```

对于较旧的 Armv7 架构（Neon 是可选的），你可以使用 -mcpu、-march 和 -mfpu 选项来指定 Neon 可用。

**指定自动向量化的优化级别**

Arm Compiler 6 通过 -O 选项提供了广泛的优化级别：

| 选项 | 含义 | 自动向量化 |
| --- | --- | --- |
| -O0 | 最小优化 | 从不 |
| -O1 | 受限优化 | 默认禁用 |
| -O2 | 高优化 | 默认启用 |
| -O3 | 极高优化 | 默认启用 |
| -Os | 减小代码大小，在代码大小与速度之间权衡 | 默认启用 |
| -Oz | 尽可能小的代码大小 | 默认启用 |
| -Ofast | 超越 -O3 的高性能优化 | 默认启用 |
| -Omax | 超越 -Ofast 的高性能优化 | 默认启用 |

有关这些选项的更多细节，请参见《Arm Compiler User Guide》中的"选择优化选项"和《Arm Compiler armclang Reference Guide》中的 -O。

自动向量化在优化级别 -O2 及更高时默认启用。-fno-vectorize 选项可以禁用自动向量化。

在优化级别 -O1，自动向量化默认禁用。-fvectorize 选项可以启用自动向量化。

在优化级别 -O0，自动向量化始终禁用。如果你指定 -fvectorize 选项，编译器会忽略它。

## 4. 示例：向量加法

让我们看看如何使用编译器选项来自动向量化和优化一个简单的 C 程序。

1. 新建一个包含以下函数的文件 vec_add.c。这个函数把两个 32 位浮点值数组相加。

```c
void vec_add(float *vec_A, float *vec_B, float *vec_C, int len_vec) {
          int i;
          for (i=0; i<len_vec; i++) {
                  vec_C[i] = vec_A[i] + vec_B[i];
          }
  }
```

2. 在不使用自动向量化的情况下编译代码：

```shell
armclang --target=aarch64-arm-none-eabi -g -c -O1 vec_add.c
```

3. 反汇编生成的目标文件，查看生成的指令：

```shell
fromelf --disassemble vec_add.o -o disassembly_vec_off.txt
```

反汇编后的代码大致如下：

```asm
vec_add                  ; 备用入口点
          CMP      w3,#1
          B.LT     |L3.36|
          MOV      w8,w3
  |L3.12|
          LDR      s0,[x0],#4
          LDR      s1,[x1],#4
          SUBS     x8,x8,#1
          FADD     s0,s0,s1
          STR      s0,[x2],#4
          B.NE     |L3.12|
  |L3.36|
          RET
```

这里我们可以看到函数的标签名 vec_add，后面是构成该函数的生成汇编指令。FADD 指令执行操作的核心部分，但代码没有利用 Neon，因为一次只执行一个加法操作。我们能看到这一点，因为 FADD 指令操作的是标量寄存器 S0 和 S1。

4. 这次使用自动向量化重新编译代码：

```shell
armclang --target=aarch64-arm-none-eabi -g -c -O1 vec_add.c -fvectorize
```

5. 反汇编生成的目标文件，查看生成的指令：

```shell
fromelf --disassemble vec_add.o -o disassembly_vec_on.txt
```

反汇编后的代码大致如下：

```asm
vec_add                  ; 备用入口点
          CMP      w3,#1
          B.LT     |L3.184|
          CMP      w3,#4
          MOV      w8,w3
          MOV      x9,xzr
          B.CC     |L3.140|
          LSL      x10,x8,#2
          ADD      x12,x0,x10
          ADD      x11,x2,x10
          CMP      x12,x2
          ADD      x10,x1,x10
          CSET     w12,HI
          CMP      x11,x0
          CSET     w13,HI
          CMP      x10,x2
          CSET     w10,HI
          CMP      x11,x1
          AND      w12,w12,w13
          CSET     w11,HI
          TBNZ     w12,#0,|L3.140|
          AND      w10,w10,w11
          TBNZ     w10,#0,|L3.140|
          AND      x9,x8,#0xfffffffc
          MOV      x10,x9
          MOV      x11,x2
          MOV      x12,x1
          MOV      x13,x0
  |L3.108|
          LDR      q0,[x13],#0x10
          LDR      q1,[x12],#0x10
          SUBS     x10,x10,#4
          FADD     v0.4S,v0.4S,v1.4S
          STR      q0,[x11],#0x10
          B.NE     |L3.108|
          CMP      x9,x8
          B.EQ     |L3.184|
  |L3.140|
          LSL      x12,x9,#2
          ADD      x10,x2,x12
          ADD      x11,x1,x12
          ADD      x12,x0,x12
          SUB      x8,x8,x9
  |L3.160|
          LDR      s0,[x12],#4
          LDR      s1,[x11],#4
          SUBS     x8,x8,#1
          FADD     s0,s0,s1
          STR      s0,[x10],#4
          B.NE     |L3.160|
  |L3.184|
          RET
```

SLP 自动向量化成功了，从指令 `FADD v0.4S,v0.4S,v1.4S` 可以看到——它对打包进 SIMD 寄存器的四个 32 位浮点数执行加法。然而这是以显著增加代码大小为代价的，因为它必须检测 SIMD 宽度不能整除数组长度的情况。这种代码大小的增加是否可接受，取决于项目和目标硬件。对于手机应用，代码大小的变化相对于可用内存来说微不足道，这可能是可容忍的；但对于 RAM 很小的嵌入式应用来说，这可能是不可接受的。

下面包含完整的代码清单。在不同优化级别下编译和反汇编，观察对生成代码的影响。

## 5. 示例：循环中的函数

有时，如果你想使用编译器的特定优化特性，对源代码的修改是不可避免的。当代码过于复杂、编译器无法自动向量化时，或者当你想覆盖编译器关于如何优化特定代码段的决策时，就会发生这种情况。

1. 新建一个包含以下函数的文件 cubed.c。这个函数计算一个值数组的立方。

```c
double cubed(double x) {
          return x*x*x;
  }
  void vec_cubed(double *x_vec, double *y_vec, int len_vec) {
          int i;
          for (i=0; i<len_vec; i++) {
                  y_vec[i] = cubed(x_vec[i]);
          }
  }
```

2. 使用自动向量化编译代码：

```shell
armclang --target=aarch64-arm-none-eabi -g -c -O1 -fvectorize cubed.c
```

3. 反汇编生成的目标文件，查看生成的指令：

```shell
fromelf --disassemble cubed.o -o disassembly.txt
```

反汇编后的代码大致如下：

```asm
cubed                  ; 备用入口点
          FMUL     d1,d0,d0
          FMUL     d0,d1,d0
          RET
          AREA ||.text.vec_cubed||, CODE, READONLY, ALIGN=2
  vec_cubed                  ; 备用入口点
          STP      x21,x20,[sp,#-0x20]!
          STP      x19,x30,[sp,#0x10]
          CMP      w2,#1
          B.LT     |L4.48|
          MOV      x19,x1
          MOV      x20,x0
          MOV      w21,w2
  |L4.28|
          LDR      d0,[x20],#8
          BL       cubed
          SUBS     x21,x21,#1
          STR      d0,[x19],#8
          B.NE     |L4.28|
  |L4.48|
          LDP      x19,x30,[sp,#0x10]
          LDP      x21,x20,[sp],#0x20
          RET
```

这段代码存在几个问题：

- 编译器没有执行循环向量化或 SLP 向量化，也没有内联我们的 cubed 函数。
- 代码需要对输入指针执行检查，以验证数组不重叠。

这些问题可以通过多种方式解决，例如在更高的优化级别编译，但让我们专注于在不改变编译器选项的情况下可以做出哪些代码修改。

4. 向代码添加以下宏和限定符，以覆盖编译器的一些决策。

- `__attribute__((always_inline))` 是 Arm Compiler 扩展，表示编译器始终尝试内联该函数。在这个例子中，不仅函数被内联了，编译器还能执行 SLP 向量化。

  在内联之前，cubed 函数只处理标量 double，所以该函数本身没有必要也没有办法执行 SLP 向量化。

  当 cubed 函数被内联时，编译器能检测到它的操作是在数组上执行的，并使用可用的 ASIMD 指令向量化代码。

- `restrict` 是标准的 C/C++ 关键字，向编译器表明给定的数组对应唯一的内存区域。这消除了对重叠数组的运行时检查。

- `#pragma clang loop interleave_count(X)` 是 Clang 语言扩展，让你通过指定向量宽度和交错计数来控制自动向量化。这个 pragma 是 Arm Compiler 的一个 [COMMUNITY] 特性。

向量化宏的完整参考可以在 clang 文档中找到。

```c
__always_inline double cubed(double x) {
          return x*x*x;
  }
  void vec_cubed(double *restrict x_vec, double *restrict y_vec, int len_vec) {
          int i;
          #pragma clang loop interleave_count(2)
          for (i=0; i<len_vec; i++) {
                  y_vec[i] = cubed(x_vec[i]);
          }
  }
```

5. 用我们之前相同的命令编译和反汇编。这产生以下代码：

```asm
vec_cubed                  ; 备用入口点
          CMP      w2,#1
          B.LT     |L4.132|
          CMP      w2,#4
          MOV      w8,w2
          B.CS     |L4.28|
          MOV      x9,xzr
          B        |L4.92|
  |L4.28|
          AND      x9,x8,#0xfffffffc
          ADD      x10,x0,#0x10
          ADD      x11,x1,#0x10
          MOV      x12,x9
  |L4.44|
          LDP      q0,q1,[x10,#-0x10]
          ADD      x10,x10,#0x20
          SUBS     x12,x12,#4
          FMUL     v2.2D,v0.2D,v0.2D
          FMUL     v3.2D,v1.2D,v1.2D
          FMUL     v0.2D,v0.2D,v2.2D
          FMUL     v1.2D,v1.2D,v3.2D
          STP      q0,q1,[x11,#-0x10]
          ADD      x11,x11,#0x20
          B.NE     |L4.44|
          CMP      x9,x8
          B.EQ     |L4.132|
  |L4.92|
          LSL      x11,x9,#3
          ADD      x10,x1,x11
          ADD      x11,x0,x11
          SUB      x8,x8,x9
  |L4.108|
          LDR      d0,[x11],#8
          SUBS     x8,x8,#1
          FMUL     d1,d0,d0
          FMUL     d0,d0,d1
          STR      d0,[x10],#8
          B.NE     |L4.108|
  |L4.132|
          RET
```

这个反汇编显示，内联、SLP 向量化和循环向量化都成功了。使用 restrict 指针消除了运行时重叠检查。

代码大小略有增加，这是由于循环尾部处理了当总循环计数不是四的倍数（有效展开深度）时剩余的迭代。循环展开深度是二，SLP 宽度是二，所以有效展开深度是四。在下一步中，我们来看一个优化——如果我们知道循环计数始终是四的倍数，可以做的优化。

6. 假设我们的循环计数始终是四的倍数。我们可以通过屏蔽循环计数器低位来向编译器传达这一点：

```c
void vec_cubed(double *restrict x_vec, double *restrict y_vec, int len_vec) {
          int i;
          #pragma clang loop interleave_count(1)
          for (i=0; i<(len_vec & ~3); i++) {
                  y_vec[i] = cubed_i(x_vec[i]);
          }
  }
```

7. 用我们之前相同的命令编译和反汇编。这产生以下代码：

```asm
vec_cubed                  ; 备用入口点
          AND      w8,w2,#0xfffffffc
          CMP      w8,#1
          B.LT     |L13.40|
          MOV      w8,w8
  |L13.16|
          LDR      q0,[x0],#0x10
          SUBS     x8,x8,#2
          FMUL     v1.2D,v0.2D,v0.2D
          FMUL     v0.2D,v0.2D,v1.2D
          STR      q0,[x1],#0x10
          B.NE     |L13.16|
  |L13.40|
          RET
```

代码大小减小了，因为编译器知道它不再需要测试和处理不是四的倍数的剩余迭代。向编译器承诺我们提供的数据始终是向量长度的倍数，从而产生了优化代码。

这个例子足够简单，用 -O2 编译无需任何代码修改就能执行所有这些优化，但更复杂的代码可能需要这种类型的调优才能从编译器获得最多收益。

下面包含完整的代码清单。你可以在各种优化级别和展开深度下编译和反汇编，观察编译器的自动向量化行为。

**完整源代码示例：循环中的函数**

```c
/*
 * Copyright (C) Arm Limited, 2019 All rights reserved.
 *
 * 在与基于 Arm 的技术（包括但不限于编程教程）打交道时，
 * 该示例代码作为学习辅助提供给你。
 * 在遵守本许可条款的前提下，Arm 特此授予你一项非独占、不可转让、
 * 不可再许可、免费的许可，仅用于演示和评估目的来使用和复制本软件。
 *
 * 你接受本软件未经 Arm 测试，因此本软件按"原样"提供，不附带任何明示或
 * 暗示的保证。在任何情况下，作者或版权持有人均不对因本软件或软件的使用
 * 而产生或与之相关的任何索赔、损害或其他责任负责，无论是基于诉讼、合同、
 * 侵权还是其他原因。
 */
#include <stdio.h>
void vec_init(double *vec, int len_vec, double init_val) {
        int i;
        for (i=0; i<len_vec; i++) {
                vec[i] = init_val*i - len_vec/2;
        }
}
void vec_print(double *vec, int len_vec) {
        int i;
        for (i=0; i<len_vec; i++) {
                printf("%f, ", vec[i]);
        }
        printf("\n");
}
double cubed(double x) {
        return x*x*x;
}
void vec_cubed(double *x_vec, double *y_vec, int len_vec) {
        int i;
        for (i=0; i<len_vec; i++) {
                y_vec[i] = cubed(x_vec[i]);
        }
}
__attribute__((always_inline)) double cubed_i(double x) {
        return x*x*x;
}
void vec_cubed_opt(double *restrict x_vec, double *restrict y_vec, int len_vec) {
        int i;
        #pragma clang loop interleave_count(1)
        for (i=0; i<len_vec; i++) {
                y_vec[i] = cubed_i(x_vec[i]);
        }
}
int main() {
        int N = 10;
        double X[N];
        double Y[N];
        vec_init(X, N, 1);
        vec_print(X, N);
        vec_cubed(X, Y, 10);
        vec_print(Y, N);
        vec_cubed_opt(X, Y, 10);
        vec_print(Y, N);
        return 0;
}
```

## 6. 自动向量化的编码最佳实践

随着实现变得越来越复杂，编译器能自动向量化代码的可能性会降低。例如，具有以下特征的循环特别难（或不可能）向量化：

- 不同循环迭代之间存在相互依赖的循环。
- 带有 break 子句的循环。
- 带有复杂条件的循环。

Arm 建议修改你的源代码实现来消除这些情况。

例如，自动向量化的一个必要条件是：循环大小的迭代次数必须在循环开始时已知。break 条件意味着循环大小可能在循环开始时无法得知，这会阻止自动向量化。如果无法完全避免 break 条件，可能值得把循环拆分成多个可向量化和不可向量化的部分。

关于用于控制循环向量化的编译器指令的完整讨论，可以在 LLVM-Clang 文档中找到，但最重要的两个是：

- `#pragma clang loop vectorize(enable)`
- `#pragma clang loop interleave(enable)`

这些 pragma 是给编译器的提示，分别执行 SLP 和循环向量化。它们是 Arm Compiler 的 [COMMUNITY] 特性。

针对 Arm C/C++ Compiler Linux 用户空间编译器，有更详细的自动向量化指南，尽管许多要点适用于各种 LLVM-Clang 变体：

- Arm C/C++ Compiler：自动向量化编码最佳实践
- Arm C/C++ Compiler：使用 pragma 控制自动向量化

## 7. 知识自测

以下问题帮助你检验所学知识：

**什么是 Neon？**

Neon 是 Arm 架构 Advanced SIMD 扩展的实现。所有符合 Armv8-A 架构的处理器（例如 Cortex-A76 或 Cortex-A57）都包含 Neon。在程序员看来，Neon 提供额外的 32 个 128 位寄存器，以及对这些寄存器内 8、16、32 或 64 位通道进行操作的指令。

**如何用 Arm Compiler 启用 Neon 代码生成？**

用 --target=aarch64-arm-none-eabi 以 AArch64 为目标，并指定合适的优化级别，如 -O1 -fvectorize 或 -O2 及更高。

**假设 Arm 编译器自动把一个循环展开到深度 2。你如何强制编译器展开到深度 4？**

`#pragma clang loop interleave_count(4)` 可以达到这个目的，它只应用于该特定循环。

**如何最好地编写源代码来辅助编译器优化？**

考虑以下用 -O1 编译器选项编译的函数：

```c
float vec_dot(float *vec_A, float *vec_B, int len_vec) {
          float ret = 0;
          int i;
          for (i=0; i<len_vec; i++) {
                  ret += vec_A[i]*vec_B[i];
          }
          return ret;
  }
```

你可以做以下修改来辅助编译器优化：

- 用 -O2 或更高（或 -fvectorize）编译。
- 在循环前指定 `#pragma clang loop vectorize(enable)` 作为给编译器的提示。
- 注意，我们在过程中没有修改向量，所以在这里添加 restrict 关键字不会有任何作用；输入数组是否重叠无关紧要。
- 在这种情况下，SLP 向量化会带来代码增加。根据硬件限制和预期的输入数组长度，这可能是可接受的。

这里是优化后的源代码：

```c
float vec_dot(float *vec_A, float *vec_B, int len_vec) {
          float ret = 0;
          int i;
          #pragma clang loop vectorize(enable)
          for (i=0; i<len_vec; i++) {
                  ret += vec_A[i]*vec_B[i];
          }
          return ret;
  }
```

## 8. 相关信息

以下是与本指南内容相关的一些资源：

- Arm Compiler 6 文档提供关于裸机编译器的信息。
- Arm C/C++ Compiler 文档提供关于 Linux 用户空间编译器的信息。
- LLVM-clang 文档提供关于开源 LLVM 工具链的信息。
- GCC 文档提供关于开源 GNU 工具链的信息。
- 架构探索工具（Architecture Exploration Tools）让你研究并进一步了解 Advanced SIMD 指令集。
- 《Arm Architecture Reference Manual Armv8, for Armv8-A architecture profile》提供 Advanced SIMD 指令集的完整规范。
- 《Optimizing C Code with Neon Intrinsics》指南展示如何在 C 或 C++ 代码中使用 Neon 内建函数，以利用 Armv8 架构中的 Advanced SIMD 技术。
