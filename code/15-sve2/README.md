# SVE2 入门 —— 知识点验证代码

本目录针对《15 - SVE2 入门（中文精读版）》整理，用两个最小示例演示 SVE2 的
**逐通道谓词**与**加宽加法内建函数**，交叉编译后看反汇编、用 qemu 运行。

> 文档原文：《Learn the architecture - Introducing SVE2 guide》
> 版本：v1.0

## 一、核心验证点

SVE2 与 Neon 的本质区别：向量长度**可变（128～2048 位）**，代码"编译一次到处跑"。

| 示例 | 文档章节 | 验证点 |
| --- | --- | --- |
| `01_vector_add.c` | §3 逐通道谓词 | `svwhilelt_b32` 谓词 + `svadd_x` 谓词加法 → `whilelo` / `add z0.s, p0/m` |
| `02_uaddlb.c` | §5 使用指令函数 | `svaddlb` 内建函数 → `uaddlb z0.d, z0.s, z1.s`（加宽加） |

## 二、目录结构

```
15-sve2/
├── README.md                    # 本文件
├── Makefile                     # make 构建
├── scripts/
│   └── build_all.sh             # 一键交叉编译 + 反汇编 + 运行
└── src/
    ├── 01_vector_add.c          # SVE2 谓词向量加法（向量长度无关）
    └── 02_uaddlb.c              # UADDLB 加宽加法内建函数
```

## 三、构建与运行

```bash
./scripts/build_all.sh           # 编译（-march=armv8-a+sve2）+ 反汇编
./scripts/build_all.sh --run     # 用 qemu-aarch64 -cpu max 运行

# 反汇编产物
build/disasm/01_vector_add.txt   # whilelo / ld1w / add z0.s, p0/m / st1w
build/disasm/02_uaddlb.txt       # uaddlb z0.d, z0.s, z1.s
```

环境准备：

```bash
sudo apt install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu   # GCC 10+ 支持 SVE2
sudo apt install qemu-user                                          # qemu-aarch64 -cpu max 运行 SVE2
```

> 文档用 armclang（`-march=armv8-a+sve2`），本目录用 GCC 同款选项；两者生成的
> SVE2 指令一致。

## 四、运行输出示例

```text
===== 01_vector_add =====
vector_add 结果 (a[i] + b[i]):
  0 + 0 = 0
  1 + 10 = 11
  ...
  15 + 150 = 165

===== 02_uaddlb =====
uaddlb 结果 (100 + 23 的加宽加): 123, 123
```
