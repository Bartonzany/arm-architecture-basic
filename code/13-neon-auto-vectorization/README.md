# 为 Neon 自动向量化编译 —— 知识点验证代码

本目录针对《13 - 为 Neon 自动向量化编译（中文精读版）》整理，用两个示例演示
**同一个 C 循环，在不同优化级别下编译器如何自动生成 Neon 向量指令**。

> 文档原文：《Learn the architecture - Compiling for Neon with auto-vectorization》
> 版本：v1.0

## 一、核心验证点

自动向量化的核心就是：`FADD s0`（标量，一次加一个）→ `FADD v0.4s`（向量，一次加四个）。

| 示例 | 文档章节 | 验证点 |
| --- | --- | --- |
| `vec_add.c` | §4 向量加法 | -O1 标量 `FADD s0` vs -O3 向量 `FADD v0.4s` |
| `cubed.c` | §5 循环中的函数 | 朴素版函数未内联无法向量化；`always_inline` + `restrict` 后向量化 |

## 二、编译器差异说明

文档用 **armclang**（Arm Compiler 6），本目录用 **GCC**（`aarch64-linux-gnu-gcc`），
选项对应关系：

| 文档（armclang） | 本目录（GCC） |
| --- | --- |
| `-O1`（无向量化） | `-O1`（无向量化） |
| `-O1 -fvectorize`（开向量化） | `-O3`（或 `-O2 -ftree-vectorize`） |
| `-fno-vectorize` | `-fno-tree-vectorize` |
| `#pragma clang loop interleave_count(X)` | `#pragma GCC ivdep` |

> armclang 在 `-O2` 即默认开自动向量化；GCC 对本例的循环在 `-O3` 才默认
> 向量化，故用 `-O1` / `-O3` 对照，效果与文档 `-O1` / `-O1 -fvectorize` 一致。

## 三、目录结构

```
13-neon-auto-vectorization/
├── README.md                    # 本文件
├── Makefile                     # make 构建
├── scripts/
│   └── build_all.sh             # 一键交叉编译 + 反汇编（-O1/-O3）
└── src/
    ├── vec_add.c                # 向量加法示例
    └── cubed.c                  # 循环中的函数示例（内联 + restrict）
```

## 四、构建与查看反汇编

```bash
./scripts/build_all.sh           # 编译 -O1/-O3 + 反汇编
./scripts/build_all.sh --run     # 额外用 qemu-aarch64 运行

# 反汇编产物
build/disasm/vec_add_O1.txt      # 标量：fadd s0, s0, s1
build/disasm/vec_add_O3.txt      # 向量：fadd v0.4s, v0.4s, v1.4s
build/disasm/cubed_O1.txt        # 标量：cubed 未内联
build/disasm/cubed_O3.txt        # 向量：FMUL v0.2d（内联 + 向量化）
```

环境准备：

```bash
sudo apt install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu   # 交叉编译
sudo apt install qemu-user                                          # 可选，x86 上运行
```

## 五、运行输出示例

```text
===== cubed_O3 =====
输入:   -5.000000, -4.000000, -3.000000, -2.000000, -1.000000, 0.000000, ...
朴素:   -125.000000, -64.000000, -27.000000, -8.000000, -1.000000, ...
优化:   -125.000000, -64.000000, -27.000000, -8.000000, -1.000000, ...  （结果一致）

===== vec_add_O3 =====
vec_add 结果: 9 9 9 9 9 9 9 9
```
