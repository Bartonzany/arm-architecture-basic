# AArch64 异常模型 —— 知识点验证代码

本目录针对《03 - AArch64 异常模型（中文精读版）》整理的验证代码与概念说明。
该指南绝大部分是特权级/系统级内容，可验证部分分两个层次：

- **用户态（EL0）**：同步异常 → Linux 信号、异步中断模拟（`src/*.cpp`）
- **裸机（EL1，QEMU）**：16 入口向量表 + SVC/IRQ 异常处理器（`src/bare-metal/`）

## 目录结构

```
03-aarch64-exception-model/
├── README.md                # 本文件（入口 + 导航）
├── docs/exception-notes.md  # ★ 概念全解 + 概念↔代码映射 + 用户态/裸机汇编对照
├── src/
│   ├── 01_sync_exceptions.cpp    # 同步异常 → 信号
│   ├── 02_async_interrupt.cpp    # 异步中断模拟
│   ├── test_util.hpp
│   └── bare-metal/               # 裸机：向量表 + 异常处理器（源码 + Makefile）
├── CMakeLists.txt / Makefile
├── scripts/build_all.sh
└── build/                   # 产物（gitignore）
```

## 构建与运行

```bash
./scripts/build_all.sh          # 编译用户态 demo + 裸机示例（产物进 build/）
./scripts/build_all.sh --run    # qemu 运行
make bare-metal                 # 单独构建裸机示例
```

依赖：`aarch64-linux-gnu-g++`、`qemu-system-aarch64`（`apt install qemu-system-arm`）。

## 文档导航

| 想了解 | 看这里 |
|--------|--------|
| 概念全解 + 概念↔代码映射 + 用户态/裸机汇编对照 | [`docs/exception-notes.md`](docs/exception-notes.md) |
| 源码 | [`src/`](src/) |