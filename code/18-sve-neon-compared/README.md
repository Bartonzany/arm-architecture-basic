# SVE 与 Neon 编码对比 —— 矩阵乘法三实现

本目录针对《18 - SVE 与 Neon 编码对比（中文精读版）》整理，用**矩阵乘法**（列主序）
演示同一个算法在 **C / Neon / SVE** 三种写法下的区别，交叉编译 + 反汇编 + qemu 运行。

> 文档原文：《SVE and Neon coding compared》（102131，Issue 01）

## 一、核心验证点

| 实现 | 关键内建函数 | 反汇编特征 |
| --- | --- | --- |
| `matrix_multiply_c` | 无（标量循环） | 标量 `fmul`/`fadd` |
| `matrix_multiply_neon` | `vld1q_f32`/`vfmaq_laneq_f32`/`vst1q_f32` | `fmla v0.4s`（固定 128 位） |
| `matrix_multiply_sve` | `svld1`/`svmla_lane`/`svst1` + `svwhilelt` 谓词 | `whilelt`/`ld1w`/`fmla z3.s`（可扩展向量） |

核心区别：**Neon 把"向量多宽"写死在类型里（`float32x4_t`，固定 128 位）；SVE 用
`svfloat32_t`（无大小）+ 谓词，n 维度不必是 4 的倍数（`svwhilelt` 防越界）**。

## 二、目录结构

```
18-sve-neon-compared/
├── README.md                    # 本文件
├── Makefile                     # make 构建
├── scripts/
│   └── build_all.sh             # 一键交叉编译 + 反汇编 + 运行
└── src/
    ├── bench_util.h             # 共享性能基准工具
    └── matrix_multiply.c        # C / Neon / SVE 三实现 + 正确性 + 性能对比
```

## 三、构建与运行

```bash
./scripts/build_all.sh           # 编译（-march=armv8-a+sve）+ 反汇编
./scripts/build_all.sh --run     # 用 qemu-aarch64 -cpu max 运行
```

环境准备：

```bash
sudo apt install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu   # GCC 10+ 支持 SVE
sudo apt install qemu-user                                          # qemu-aarch64 -cpu max 运行 SVE
```

## 四、运行输出示例

```text
矩阵乘法: C=0.00 Neon=0.00 SVE=0.00  (一致 PASS)

===== 性能对比（每次操作 = 1 次乘加 MAC）=====
函数                        每次操作耗时   加速比
----------------------------  --------------  ----------
matrix_multiply_c                   18.354 ns      1.000x
matrix_multiply_neon                13.655 ns      1.344x
matrix_multiply_sve                 14.364 ns      1.278x
```

> qemu 逐条模拟向量指令，Neon 与 SVE 的绝对/相对数值与真机不同；真机上 SVE 因
> 向量长度可扩展而更有优势。此 demo 重点是**正确性一致 + 反汇编对照**（`fmla v0.4s`
> vs `fmla z3.s`），性能对比仅供参考趋势。

## 五、实现要点

- **列主序**：矩阵 M 存为数组 `M_array`，`M[i][j] = M_array[n*j + i]`。地址计算
  `a_idx = i + n*k_idx`、`b_idx = k*j + k_idx` 都基于此。
- **Neon 固定 4×4 块**：`n/m/k` 必须是 4 的倍数；SVE 用 `svcntw()` + `svwhilelt`
  使 `n` 不必是 4 的倍数（但 `m/k` 仍须是 4 的倍数，因为 `svmla_lane` 按 128 位段索引）。
- **SVE 广播加载**：`svld1rq_f32`（LD1RQW）把同一 128 位段复制填满向量，供
  `svmla_lane_f32` 按 128 位段取 lane。
