# 验证代码

本目录存放《arm-architecture-basic》笔记中各文档对应的**知识点验证代码**。

每个文档一个子目录，子目录**自包含**：有独立的 `src/`（C++ 源码）、
`README.md`（框架分析 + 知识点映射）、`docs/asm-mapping.md`（C++ ↔ 汇编逐行解释）、
`CMakeLists.txt`、`Makefile` 与 `scripts/build_all.sh` 构建脚本。

## 子目录索引

| 子目录 | 对应文档 | 状态 |
|--------|----------|------|
| [`02-a64-instruction-set/`](02-a64-instruction-set/) | 02 - A64 指令集架构指南 | ✅ 17 个 demo |

## 统一约定

所有子目录遵循同一套规则：

1. **语言**：现代 C++（C++14+），优先 Intrinsic（`<arm_neon.h>`、`<arm_acle.h>`），
   仅在无等价物时用内联汇编（屏障、系统调用）。
2. **架构**：仅 AArch64，不混用 ARM32 语法。
3. **工具链**：`aarch64-linux-gnu-g++`（交叉编译），
   `-O2 -march=armv8-a+simd -fno-stack-protector`。
4. **验证方式**：交叉编译生成汇编/反汇编，对照 `docs/asm-mapping.md` 逐行理解；
   有 AArch64 真机或 `qemu-aarch64` 时可运行功能自检。
5. **汇编说明**：汇编以 GCC 13.x `-O2` 为基准，不同环境可能不同，
   但 C++ 操作 → 指令族的**语义对应关系**稳定。

## 快速开始

```bash
# 以 02 文档为例
cd 02-a64-instruction-set
./scripts/build_all.sh            # 编译 + 生成汇编 + 反汇编
./scripts/build_all.sh --run      # （可选）qemu 运行全部 demo
```
