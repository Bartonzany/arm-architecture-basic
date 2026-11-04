# SVE/SVE2 编程示例 —— 知识点验证代码

本目录针对《17 - SVE/SVE2 编程示例（中文精读版）》整理，抽取 Part B 的通用向量/矩阵
运算示例，每个示例都是「朴素 C 参考实现 + SVE2 内建函数实现」，并给出**正确性对比**和
**性能对比**，交叉编译 + 反汇编 + qemu 运行验证。

> 文档原文：《SVE Programming Examples》（ARM-DAI-0548 REL-01.2）

## 一、示例清单

| 文件 | 文档章节 | 验证点 | 关键 SVE2 指令 |
| --- | --- | --- | --- |
| `01_vecmax.c` | B1 向量最大值 | 谓词循环 + 归约 | `whilelt`/`ld1h`/`smax`/`smaxv` |
| `02_dot_product.c` | A3.5/B3 实数点积 | SDOT 点积 | `sdot`/`uaddv` |
| `03_vecmul_complex.c` | B2 向量乘法 | 复数 16 位整数乘 | `sqrdcmlah`（旋转 0/90） |
| `04_vecdot_complex.c` | B3.2 复数点积 | 复数 SP 点积 | `fcmla`（旋转 0/90）+ `uzp`/`faddv` |
| `05_fir_filter.c` | B4 FIR 滤波器 | 跨输出位置向量化 | `fmla` + `dup` 广播 |
| `06_matmul_f64.c` | B5.1 矩阵乘法 | 双精度矩阵乘 | `fmla` + `dup` 广播 |

文档 Part C（Sobel/积分图/直方图）和 Part D（字符串处理）的示例，代码见翻译版与
原文 PDF；其 SVE2 实现以汇编形式给出，此处未逐条转为内建函数版本。

## 二、目录结构

```
17-sve-examples/
├── README.md                    # 本文件
├── Makefile                     # make 构建
├── scripts/
│   └── build_all.sh             # 一键交叉编译 + 反汇编 + 运行
└── src/
    ├── bench_util.h             # 共享性能基准工具（计时 + 表格输出）
    ├── 01_vecmax.c
    ├── 02_dot_product.c
    ├── 03_vecmul_complex.c
    ├── 04_vecdot_complex.c
    ├── 05_fir_filter.c
    └── 06_matmul_f64.c
```

## 三、构建与运行

```bash
./scripts/build_all.sh           # 编译（-march=armv8-a+sve2）+ 反汇编
./scripts/build_all.sh --run     # 用 qemu-aarch64 -cpu max 运行

# 反汇编产物在 build/disasm/*.txt
```

环境准备：

```bash
sudo apt install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu   # GCC 10+ 支持 SVE2
sudo apt install qemu-user                                          # qemu-aarch64 -cpu max 运行 SVE2
```

## 四、运行输出示例

每个 demo 输出两部分：**正确性对比**（C vs SVE2 结果一致）与**性能对比**
（"函数 / 每次操作耗时 / 加速比"表格，由共享工具 `bench_util.h` 统一生成）。

```text
01 向量最大值: 朴素 C = 999, SVE2 = 999  (一致 PASS)

===== 性能对比（每次操作 = 1 次乘加 MAC）=====   # 06_matmul_f64
函数                        每次操作耗时   加速比
----------------------------  --------------  ----------
matmul_f64_c                         18.098 ns      1.000x
matmul_f64_sve                       12.671 ns      1.428x
```

> 说明：qemu 逐条模拟 SVE 指令，向量化的收益被仿真开销掩盖，故部分示例（如归约类
> 的 vecmax）在 qemu 下反而偏慢。真机（SVE 硬件）上向量化版本普遍更快，结论以文档为准。

## 五、实现要点（易错处）

- **复数元素计数**：每个复数是 2 个基础元素（如 2 个 int16 或 2 个 float），
  谓词 `svwhilelt` 和循环步进要按**基础元素**计（`total = 2*n`），否则只处理一半。
- **复数归约去交错**：复数结果按 [re,im,re,im,...] 交替存放，去交错要用
  `svuzp1(acc, zero)`/`svuzp2(acc, zero)`（配零向量），不能用 `svuzp1(acc, acc)`
  （那会重复元素、结果翻倍）。
- **FIR/矩阵乘**：按"输出位置"向量化（每个通道算一个输出），内层沿 k/j 用
  `svdup` 广播 + `svmla` 累加，而不是对单个输出点做标量归约。
