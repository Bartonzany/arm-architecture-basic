# Neon 迁移到 SVE —— 迁移步骤 + 什么时候保留 Neon

本目录针对《21 - Neon 迁移到 SVE（中文精读版）》整理。由于本文档的矩阵乘法示例
（C/Neon/SVE 三实现）与《18 - SVE 与 Neon 编码对比》完全相同，已放在
[`../18-sve-neon-compared`](../18-sve-neon-compared) 中；本目录只补本文档**独有**
的内容——「什么时候保留 Neon」的反例。

> 文档原文：《Learn the architecture - Migrate Neon to SVE》（102131_0100_04_en）

## 一、迁移三步（本文档核心）

1. 更新编译器选项（含自动向量化选项：`-march=armv8-a+sve` / `-mcpu=native`）。
2. 参考 SVE 规范，把 Neon 内建函数替换为 SVE 内建函数（`arm_neon.h` → `arm_sve.h`）。
3. 链接数学库的 SVE 变体（Arm Performance Libraries）。

## 二、什么时候别迁：固定小循环的 VLA 开销

文档第 5 章给出两个"保留 Neon 反而更好"的反例。本 demo 演示其中一个——**一般 VLA
开销**：固定 8 次循环，编译器知道迭代次数，可以完全展开 Neon 循环、省掉谓词；SVE
VLA 写法（`svcntw` + `svwhilelt`）反而要谓词和循环控制。

反汇编对照（`build/disasm/keep_neon.txt`）：

```asm
; square_scalar（强制 -march=armv8-a，Neon）：完全展开，无谓词、无循环
fmul v1.4s, v1.4s, v1.4s
fmul v0.4s, v0.4s, v0.4s

; square_sve_vla（SVE VLA）：whilelt 谓词 + 循环控制
whilelt p0.s, x0, x4
ld1w   { z0.s }, p0/z, [x2, x0, lsl #2]
fmul   z0.s, p0/m, z0.s, z0.s
st1w   { z0.s }, p0, [x1, x0, lsl #2]
```

> 结论（文档原话）：有时 VLA 代码的开销不优于高度优化的 Neon 代码。**Neon 的强项是
> "小而固定"的循环（能完全展开、省寄存器、无谓词），SVE 的强项是"大而动态"的循环**。

## 三、目录结构

```
21-migrate-neon-to-sve/
├── README.md                    # 本文件
├── Makefile
├── scripts/build_all.sh
└── src/
    ├── bench_util.h
    └── keep_neon.c              # 固定小循环：Neon 完全展开 vs SVE 谓词
```

## 四、构建与运行

```bash
./scripts/build_all.sh           # 编译 + 反汇编
./scripts/build_all.sh --run     # 用 qemu-aarch64 -cpu max 运行
```

> 说明：`square_scalar` 用 `__attribute__((target("arch=armv8-a")))` 强制编译成 Neon，
> `square_sve_vla` 随全局 `-march=armv8-a+sve` 编译成 SVE，从而得到真正的 Neon vs SVE
> 对照。qemu 逐条模拟向量指令，性能对比不反映真机（真机上小循环的谓词开销可测，
> qemu 下未必），本 demo 的价值在**反汇编结构差异**，性能对比仅供参考。
