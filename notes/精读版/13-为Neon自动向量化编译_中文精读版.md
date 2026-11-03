# 读懂为 Neon 自动向量化编译（中文精读版）

> 原文：*Learn the architecture - Compiling for Neon with auto-vectorization*
>
> 版本：Version 1.0　|　文档 ID：102525_0100_00_en　|　Issue 00（2019-06-18）
>
> 版权 © 2019 Arm Limited（或其关联公司）。保留所有权利。非保密。
>
> 说明：本文在原中文翻译的基础上重新组织了叙述逻辑与表达方式，技术内容、表格与图表保持原意；个别明显笔误已按 Arm 架构规范订正。

---

## 导读：这篇文章讲什么

上一本《用 Neon 内建函数优化 C 代码》教你**手动**用内建函数写 SIMD；这一本反过来——**不写一行内建函数**，让编译器自己把普通 C 循环变成 Neon 指令。这就是**自动向量化（auto-vectorization）**。

核心就一句话：**选对目标、开够优化级别，编译器就会在循环里自动生成 `FADD v0.4s` 这种向量指令**。文章用两个例子演示这件事，并教你遇到"编译器不配合"时怎么改代码或加 `#pragma`/`restrict` 推它一把。

读完你要能：说出哪些编译器选项能开 Neon 代码生成；写出利于自动向量化的 C/C++；知道什么时候必须改代码、什么时候用 pragma。面向所有做 Arm 开发、又不想碰汇编的人。

## 1. 为什么把向量化交给编译器

手工写汇编内核或 Neon 内建函数，控制力最强，但代价是**可移植性差、工程复杂**。多数情况下，一个好编译器生成的代码"一样好"，却省下大量设计时间。让编译器自动找 SIMD 机会，就叫自动向量化，它包含两种技术：

- **循环向量化**：展开循环，减少迭代次数、每次多做几个操作。
- **SLP 向量化（超字级并行）**：把相邻的标量操作"打包"进一条完整宽度的 SIMD 指令。

好处：高级语言代码可移植；现代编译器自动做高级优化；换微架构目标可能只改一个选项。代价也要认：最佳代码生成可能要**迁就特定编译器**；源码或选项的**微小改动**可能让生成代码发生**不可预测的大变**。

结论：**大多数项目让编译器生成 Neon 就够了**。只有生成的代码不达标、或高级语言不支持某硬件特性（比如配系统寄存器控制浮点，必须写汇编）时，才退回手工。

## 2. 两个开关：目标 + 优化级别

要开自动向量化，做两件事：

**① 以有 Neon 的目标编译**。Neon 是所有标准 Armv8-A 实现的必选项，所以以任何 Armv8-A 架构/处理器为目标都行。只跑某颗芯片就 `-mcpu=...`（性能最优但只保证这颗能跑）；要广兼容就以架构为目标（到处能跑但性能打折）。AArch64 目标示例：

```shell
armclang --target=aarch64-arm-none-eabi
```

**② 选到含自动向量化的优化级别**。规则很清晰：

| 级别 | 自动向量化 |
| --- | --- |
| -O0 | 从不（即使加 -fvectorize 也被忽略） |
| -O1 | 默认禁用，`-fvectorize` 可开启 |
| -O2 / -O3 / -Os / -Oz / -Ofast / -Omax | 默认启用，`-fno-vectorize` 可关闭 |

想看清编译器怎么优化的循环，再加 `-Rpass=loop`，它会打印向量化宽度和交错计数（[COMMUNITY] 特性）。

## 3. 示例一：向量加法 —— 看标量 FADD 变向量 FADD

一个极简的数组加法：

```c
void vec_add(float *vec_A, float *vec_B, float *vec_C, int len_vec) {
    int i;
    for (i=0; i<len_vec; i++) {
        vec_C[i] = vec_A[i] + vec_B[i];
    }
}
```

**关掉自动向量化**（`-O1`）编译，反汇编是标量循环：

```asm
|L3.12|
    LDR   s0, [x0], #4
    LDR   s1, [x1], #4
    FADD  s0, s0, s1        ; 一次只加一个（标量寄存器 s0/s1）
    STR   s0, [x2], #4
    B.NE  |L3.12|
```

`FADD s0, s0, s1` 操作的是标量寄存器 S0/S1——没用到 Neon。

**打开自动向量化**（`-O1 -fvectorize`），反汇编里出现了：

```asm
|L3.108|
    LDR   q0, [x13], #0x10
    LDR   q1, [x12], #0x10
    FADD  v0.4s, v0.4s, v1.4s   ; 一次加四个 32 位浮点
    STR   q0, [x11], #0x10
    B.NE  |L3.108|
```

`FADD v0.4s` 一次加四个 32 位浮点——SLP 向量化成功。

**代价**：为处理"数组长度不是 4 的倍数"的剩余部分，编译器多生成了一大段重叠检查 + 标量尾循环，代码体积显著变大。手机应用无所谓，RAM 紧张的嵌入式就难说了——这正是"代码大小 vs 速度"的经典权衡。

## 4. 示例二：循环里的函数 —— 内联 + restrict + pragma

有些代码编译器就是不肯向量化。看这个求立方的：

```c
double cubed(double x) { return x*x*x; }
void vec_cubed(double *x_vec, double *y_vec, int len_vec) {
    int i;
    for (i=0; i<len_vec; i++) {
        y_vec[i] = cubed(x_vec[i]);   // 调用外部函数
    }
}
```

`-O1 -fvectorize` 编译反汇编：**既没向量化，也没内联 `cubed`**，还加了数组重叠检查（`BL cubed` 逐个调用 + 标量 `FMUL d0`）。问题有两个：函数没内联、指针没声明不重叠。

改代码（不改编译器选项）三招：

1. **`__attribute__((always_inline))`**（Arm Compiler 扩展）强制内联。`cubed` 本身只处理标量 double，单独没法向量化；内联后编译器发现它是在数组上循环操作，就能用 ASIMD 向量化了。
2. **`restrict`**（标准 C/C++ 关键字）声明数组指向互不重叠的内存，省掉运行时重叠检查。
3. **`#pragma clang loop interleave_count(X)`** 控制向量化宽度和交错计数。

```c
__always_inline double cubed(double x) { return x*x*x; }
void vec_cubed(double *restrict x_vec, double *restrict y_vec, int len_vec) {
    int i;
    #pragma clang loop interleave_count(2)
    for (i=0; i<len_vec; i++) {
        y_vec[i] = cubed(x_vec[i]);
    }
}
```

再编译反汇编，核心循环变成 `FMUL v2.2D, v0.2D, v0.2D` 这类向量指令——**内联 + SLP + 循环向量化全成功，restrict 消除了重叠检查**。代价是多了处理"余数"的尾循环。

再进一步：如果你**确定循环次数永远是 4 的倍数**，用 `i < (len_vec & ~3)` 把低位掩掉，编译器就**不再生成尾循环**，代码更小：

```c
#pragma clang loop interleave_count(1)
for (i=0; i<(len_vec & ~3); i++) {
    y_vec[i] = cubed_i(x_vec[i]);
}
```

> 这个例子简单到 `-O2` 无需改代码就能全优化；但更复杂的代码，就得靠这类调优从编译器榨性能了。

## 5. 编码最佳实践

代码越复杂，编译器越难自动向量化。三类"难向量化"的循环要尽量改掉：

- 迭代之间**有依赖**的循环；
- 带 **break** 的循环（循环次数在开始时未知 → 阻断向量化）；
- **条件复杂**的循环。

改不掉 break，就**把循环拆成"可向量化"和"不可向量化"两段**。

两个最常用的控制 pragma（[COMMUNITY] 特性）：

- `#pragma clang loop vectorize(enable)` —— 提示做 SLP 向量化
- `#pragma clang loop interleave(enable)` —— 提示做循环向量化

## 6. 知识自测

- **什么是 Neon？** Arm 架构 Advanced SIMD 扩展的实现；额外 32 个 128 位寄存器 + 8/16/32/64 位通道指令。
- **怎么开 Neon 代码生成？** `--target=aarch64-arm-none-eabi` 目标 + 合适优化级别（`-O1 -fvectorize` 或 `-O2` 及以上）。
- **怎么把编译器默认的展开深度 2 改成 4？** `#pragma clang loop interleave_count(4)`，只作用于该循环。
- **`vec_dot` 想辅助编译器优化，怎么做？** 用 `-O2`/`-fvectorize` 编译；循环前加 `#pragma clang loop vectorize(enable)`；此例没改向量、`restrict` 无用（输入是否重叠无影响）；SLP 会增代码，按硬件/输入长度权衡是否接受。

## 7. 相关资源

编译器文档（Arm Compiler 6 / Arm C/C++ Compiler / LLVM-clang / GCC）；Architecture Exploration Tools；ARMv8-A 的 ARM；以及上一篇《Optimizing C Code with Neon Intrinsics》。
