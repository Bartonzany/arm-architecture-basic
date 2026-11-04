# SVE 优化指南 —— SAXPY 多级优化对比

本目录针对《16 - SVE 优化指南（中文精读版）》整理，用 **SAXPY**（`y = a*x + y`，L1 BLAS
向量加法）演示四条优化路线，并给出**多级优化反汇编**和**性能对比**。

> 文档原文：《SVE Optimization Guide》（102699，Issue 01）

## 一、核心验证点

| 实现 | 文档章节 | 关键指令 |
| --- | --- | --- |
| `saxpy_c_O1`（`__attribute__((optimize("O1")))`） | §3 基准 | 标量 `fmul` + `fadd` |
| `saxpy_c`（全局 -O3） | §3 自动向量化 | `whilelo`/`ld1w`/`st1w`（+ `fmad`） |
| `saxpy_sve` | §4 内建函数 | `svlen_f32`→CNTW、`svwhilelt`→WHILELO、`svmla`→FMLA |
| `saxpy_asm` | §5 汇编 | `whilelo`/`ld1w`/`fmla`/`st1w`/`incw` |

关键点：`svlen_f32` **运行时**获取向量长度、`svwhilelt` 谓词防越界——**向量长度无关**编程。

## 二、目录结构

```
16-sve-optimization/
├── README.md                    # 本文件
├── Makefile                     # make 构建
├── scripts/
│   └── build_all.sh             # 编译 + 多级优化反汇编 + 运行
└── src/
    ├── saxpy_example.c          # 四种实现 + 正确性 + 性能对比
    ├── saxpy_asm.S              # SVE 汇编内核
    └── saxpy_c_only.c           # 仅 saxpy_c，用于 -O1/-O2/-O3/-Ofast 反汇编对照
```

## 三、构建与运行

```bash
./scripts/build_all.sh           # 编译 + 多级优化反汇编
./scripts/build_all.sh --run     # 额外运行（正确性 + 性能对比）
```

反汇编产物（`build/disasm/`）：

- `saxpy_c_O1.txt` / `_O2.txt` / `_O3.txt` / `_Ofast.txt` —— saxpy_c 在各优化级别下的反汇编
- `saxpy_example.txt` —— 主程序（含四种实现）
- `saxpy_asm.txt` —— 汇编内核

环境准备：

```bash
sudo apt install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu   # GCC 10+ 支持 SVE
sudo apt install qemu-user                                          # qemu-aarch64 -cpu max 运行 SVE
```

## 四、多级优化反汇编对照

```text
-O1    标量: fmul s2, s0, s2 + fadd s1, s1, s2      （无向量化）
-O2    标量: fmad（融合乘加，但仍未向量化，GCC 对本例需 -O3 才向量化）
-O3    向量: whilelo + ld1w + st1w + fmad           （SVE 向量循环）
-Ofast 向量: ptrue + whilelo + ld1w + st1w + fmad   （更强融合）
```

> 注：文档用 armclang（`-O2` 即默认向量化），本目录用 GCC（本循环需 `-O3` 才默认
> 向量化），两者生成指令一致、触发级别不同。

## 五、运行输出示例（qemu-aarch64 -cpu max）

```text
正确性: 四种实现 y[0] = 5.0/5.0/5.0/5.0  (一致 PASS)

方法                      每次操作耗时      加速比
---------------------------------------------------------
C 无向量化 (-O1)            27.670 ns       1x
C 自动向量化 (-O3)          27.677 ns       1.000x
SVE 内建函数                22.300 ns       1.241x
SVE 汇编                    12.392 ns       2.233x
```

> 文档第 7 章在 SVE 真机用 perf 实测：自动向量化 +27.5%、内建函数 +33.2%、汇编 +34.7%。
> qemu 仿真下绝对数值与真机不同（向量指令被逐条模拟、无法体现真实吞吐），但**排名一致**：
> 汇编 > 内建函数 > 自动向量化 > 无向量化。
