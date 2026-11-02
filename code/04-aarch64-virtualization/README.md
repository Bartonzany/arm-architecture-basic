# AArch64 虚拟化 —— 知识点验证代码与概念说明

本目录针对《04 - AArch64 虚拟化指南（中文精读版）》整理。该指南全部是
EL2/EL3 特权级内容，用户态无法验证；但借助 QEMU 的 EL2 支持
（`-machine virt,virtualization=on`），核心机制之一 **陷阱（trap）** 可在
裸机 EL2 环境真实验证。

## 目录结构

```
04-aarch64-virtualization/
├── README.md                    # 本文件（入口 + 导航）
├── docs/virtualization-notes.md # ★ 概念全解 + 陷阱机制（文段/代码/结果/为什么）
├── src/bare-metal/              # EL2 裸机示例（hypervisor + guest）
│   ├── boot.S                   # EL2 启动：配 HCR_EL2、ERET 切 EL1
│   ├── vector_table.S           # EL2 向量表 + WFI 陷阱处理器
│   ├── guest.c                  # guest(EL1) 代码 + EL2 陷阱处理器 C 部分
│   ├── linker.ld / Makefile
├── Makefile / scripts/build_all.sh
└── build/                       # 产物（gitignore）
```

## 构建与运行

```bash
./scripts/build_all.sh            # 交叉编译 + 反汇编
./scripts/build_all.sh --run      # qemu-system-aarch64 运行
make run                          # 或 make 方式
```

依赖：`qemu-system-aarch64`（`apt install qemu-system-arm`）。

## 文档导航

| 想了解 | 看这里 |
|--------|--------|
| 概念全解 + 陷阱机制代码/结果/因果 | [`docs/virtualization-notes.md`](docs/virtualization-notes.md) |
| 源码 | [`src/bare-metal/`](src/bare-metal/) |