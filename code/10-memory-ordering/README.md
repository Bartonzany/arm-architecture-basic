# AArch64 内存系统、排序与屏障 —— 知识点验证代码与概念说明

本目录针对《10 - AArch64 内存系统、排序与屏障（中文精读版）》整理。该文档核心
是**跨观察者（多核）的内存排序**，QEMU TCG 单核无法复现；故本示例聚焦**可确定性
验证**的两部分：LDAR/STLR/LDAXP 的**对齐要求**（§3）与 **ISB 上下文同步**（§4）。

## 目录结构

```
10-memory-ordering/
├── README.md                      # 本文件（入口 + 导航）
├── docs/ordering-barriers-notes.md# ★ 概念 + 文段/代码/结果/为什么
├── src/                           # EL1 裸机源码
│   ├── boot.S                     # 设栈 + VBAR_EL1 + 跳 main
│   ├── vector_table.S             # 16 入口向量表 + 同步异常处理器
│   ├── main.c                     # 对齐故障 + ISB 上下文同步验证用例
│   ├── linker.ld / Makefile
├── Makefile / scripts/build_all.sh
└── build/                         # 产物（gitignore）
```

## 构建与运行

```bash
./scripts/build_all.sh            # 交叉编译 + 反汇编
./scripts/build_all.sh --run      # qemu-system-aarch64 运行
make run                          # 或 make 方式
```

依赖：`aarch64-linux-gnu-gcc`、`qemu-system-aarch64`（`apt install qemu-system-arm`）。

## 验证点对照

| 用例 | 验证的知识点 | 观察到的异常（ESR_EL1）|
|------|--------------|------------------------|
| [ A ] | LDAR/STLR 对齐访问正常（§3）| — |
| [ B ] | 非对齐 LDAR → 对齐故障（§3）| EC=0x25 FSC=0x21 |
| [ C ] | 非对齐 LDAXP → 对齐故障（§3）| EC=0x25 FSC=0x21 |
| [ D ] | 非对齐 STLR → 对齐故障（§3）| EC=0x25 FSC=0x21 |
| [ E ] | CPACR.FPEN + ISB 上下文同步（§4）| EC=0x07（FP/SIMD 捕获）|

## 文档导航

| 想了解 | 看这里 |
|--------|--------|
| 概念 + 文段/代码/结果/为什么 | [`docs/ordering-barriers-notes.md`](docs/ordering-barriers-notes.md) |
| 源码 | [`src/`](src/) |
