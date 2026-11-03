# 用 Neon 内建函数优化 C 代码 —— 知识点验证代码

本目录针对《12 - 用 Neon 内建函数优化 C 代码（中文精读版）》整理的可验证示例，
把文档里的三个例子做成 3 个自包含 demo，每个都能用 AArch64 交叉编译，并对照
`-O2` 反汇编看"C/C++ 里的内建函数如何变成 Neon 指令"。

> 文档原文：《Learn the architecture - Optimizing C code with Neon intrinsics》
> 版本：v2.1

## 一、文档脉络与示例

这份文档的主线是"一步步把 C 函数改成 Neon 内建函数版本"，核心结论是：
**光换指令不够，数据布局（是否交错）决定向量化能榨出多少性能**。

| 章节 | 示例 | 要点 | 对应 demo |
| --- | --- | --- | --- |
| §4 | RGB 去交错 | `vld3q_u8` → LD3 一次交错加载 3×16 字节 | 01 |
| §5 | 矩阵乘法 | 4×4 分块 + `vfmaq_laneq_f32` → FMLA 融合乘加 | 02 |
| §6 | 碰撞检测 | 朴素 / 基本向量化 / 去交错（近 3 倍提速） | 03 |
| §7 | 编程约定 | 宏、类型 `baseWxLxN_t`、函数命名 | （见本 README） |

## 二、Demo → 文档映射

| # | 源文件 | 文档章节 | 验证机制 |
| --- | --- | --- | --- |
| 01 | `01_rgb_deinterleave.cpp` | §4 | 朴素逐字节 vs `vld3q_u8`/`vst1q_u8`，正确性 + 性能对比；反汇编看 `ld3` |
| 02 | `02_matrix_multiply.cpp` | §5 | `matrix_multiply_c` vs `matrix_multiply_neon`，正确性 + 性能对比；反汇编看 `fmla` |
| 03 | `03_collision_detection.cpp` | §6 | 三种实现：标量 / `float32x2_t` 基本向量化 / 去交错 `float32x4_t`，性能对比 |

## 三、目录结构

```
12-neon-intrinsics/
├── README.md                    # 本文件
├── Makefile                     # make 构建（交叉编译 + 汇编 + 反汇编）
├── scripts/
│   └── build_all.sh             # 一键交叉编译 + 汇编 + 反汇编
└── src/                         # 3 个自包含 demo（含 main）+ 共享基准工具
    ├── bench_util.hpp           # 共享性能基准工具（统一计时 + 表格输出）
    ├── 01_rgb_deinterleave.cpp
    ├── 02_matrix_multiply.cpp
    └── 03_collision_detection.cpp
```

## 四、构建与运行

```bash
# 一键交叉编译 + 汇编 + 反汇编（需 aarch64-linux-gnu-g++ / objdump）
./scripts/build_all.sh

# 额外用 qemu-aarch64 在 x86 主机上运行（需 qemu-user）
./scripts/build_all.sh --run

# 或 make 方式
make && make asm && make disasm && make run
```

环境准备：

```bash
sudo apt install g++-aarch64-linux-gnu binutils-aarch64-linux-gnu   # 交叉编译
sudo apt install qemu-user                                          # 可选，x86 上运行
```

## 五、运行输出（工程化日志）

每个 demo 运行后输出两部分：**正确性对比**（C vs Neon 结果是否一致）与
**性能对比**（统一的"函数 / 每次调用耗时 / 加速比"表格）。性能部分由共享
工具 `src/bench_util.hpp` 统一生成：计时取平均（含预热）、归一化到"单次逻辑
操作"、自动以第一个函数为 1x 基准算加速比。

示例（`qemu-aarch64` 下，数值随硬件/仿真环境而异，看相对趋势）：

```text
===== 03_collision_detection =====
两个圆(圆心距5,半径和3): 朴素=不碰撞  Neon=不碰撞  (一致 PASS)

===== 性能对比（每次操作 = 1 次碰撞检测） =====
函数                            每次调用耗时   加速比
--------------------------------  --------------  ----------
does_collide                           106.099 ns      1.000x
does_collide_neon                       61.616 ns      1.722x
does_collide_neon_deinterleaved         41.701 ns      2.544x
```

> 真机（如 Cortex-A）上趋势会更贴近文档：`does_collide_neon` 约 1x（基本
> 向量化收益被交错布局抵消），去交错版约 2~3x。`qemu` 仿真下每条指令开销更
> 均匀，基本向量化版会显得偏快，属预期。

## 六、关键内建函数速查

| 内建函数 | Neon 指令 | 作用 |
| --- | --- | --- |
| `vld3q_u8` | LD3 | 交错加载 3×16 字节到三个寄存器 |
| `vst1q_u8` | ST1 | 存满一个 128 位寄存器的字节 |
| `vld1q_f32` | LD1 | 加载四个 32 位浮点 |
| `vfmaq_laneq_f32` | FMLA | 融合乘加：向量 × 另一向量某 lane 的标量，再加到累加器 |
| `vsub_f32` / `vsubq_f32` | FSUB | 逐通道相减（2/4 通道） |
| `vmul_f32` / `vmulq_f32` | FMUL | 逐通道相乘 |
| `vaddq_f32` | FADD | 逐通道相加 |
| `vpadds_f32` | FADDP | 跨通道成对求和（得标量） |
| `vdupq_n_f32` | DUP | 标量广播到全部通道 |
| `vcltq_f32` | FCMGT(反) | 逐通道小于比较，成立则通道全 1 |
| `vgetq_lane_u32` | UMOV | 提取单个通道 |

> 注：文档第 5 节小结表把融合乘加写成 `vfmaq_lane_f32`，系笔误；
> 从 `float32x4_t` 的 lane 取标量应使用 `vfmaq_laneq_f32`，本目录代码已订正。
