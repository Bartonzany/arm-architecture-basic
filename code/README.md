# 验证代码

本目录存放《arm-architecture-basic》笔记中各文档对应的**知识点验证代码**。

每个文档一个子目录，子目录**自包含**：有独立的 `src/`（C++ 源码）、
`README.md`（框架分析 + 知识点映射）、`docs/`（C++ ↔ 汇编逐行解释或概念说明）、
`CMakeLists.txt`、`Makefile` 与 `scripts/build_all.sh` 构建脚本。

## 子目录索引

| 子目录 | 对应文档 | 状态 |
|--------|----------|------|
| [`02-a64-instruction-set/`](02-a64-instruction-set/) | 02 - A64 指令集架构指南 | ✅ 18 个 demo |
| [`03-aarch64-exception-model/`](03-aarch64-exception-model/) | 03 - AArch64 异常模型 | ✅ 2 个 demo + EL1 裸机（向量表/异常）|
| [`04-aarch64-virtualization/`](04-aarch64-virtualization/) | 04 - AArch64 虚拟化指南 | ✅ EL2 裸机（陷阱机制）+ 概念说明 |
| [`05-generic-timer/`](05-generic-timer/) | 05 - Arm 通用定时器 | ✅ EL1 裸机（频率/计数/CVAL·TVAL/中断）|
| [`08-mmu-examples/`](08-mmu-examples/) | 08b - AArch64 内存管理示例 | ✅ 10 个 MMU 初始化示例（EL1/EL2/EL3，纯汇编）|

## 统一约定

所有子目录遵循同一套规则：

1. **语言**：现代 C++（C++14+），优先 Intrinsic（`<arm_neon.h>`、`<arm_acle.h>`），
   仅在无等价物时用内联汇编（屏障、系统调用）。
2. **架构**：仅 AArch64，不混用 ARM32 语法。
3. **工具链**：`aarch64-linux-gnu-g++`（交叉编译），
   `-O2 -march=armv8-a+simd -fno-stack-protector`；纯汇编子目录
   （如 `08-mmu-examples`）改用 `aarch64-linux-gnu-as`/`ld`/`objdump`。
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
