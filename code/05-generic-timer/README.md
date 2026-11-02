# Arm 通用定时器 —— 知识点验证代码与概念说明

本目录针对《05 - Arm 通用定时器（中文精读版）》整理。通用定时器的核心机制
（频率、计数、CVAL/TVAL 两种配置、定时器中断）都可在 **EL1 裸机**环境真实
验证。

## 目录结构

```
05-generic-timer/
├── README.md                # 本文件（入口 + 导航）
├── docs/timer-notes.md      # ★ 概念 + 文段/代码/结果/为什么
├── src/bare-metal/          # EL1 裸机示例
│   ├── boot.S               # 设栈 + VBAR_EL1 + 跳 main
│   ├── vector_table.S       # 16 入口向量表 + IRQ 处理器
│   ├── main.c               # 读频率/计数 + CVAL/TVAL 验证 + 定时器中断
│   ├── linker.ld / Makefile
├── Makefile / scripts/build_all.sh
└── build/                   # 产物（gitignore）
```

## 构建与运行

```bash
./scripts/build_all.sh            # 交叉编译 + 反汇编
./scripts/build_all.sh --run      # qemu-system-aarch64 运行
make run                          # 或 make 方式
```

依赖：`aarch64-linux-gnu-g++`、`qemu-system-aarch64`（`apt install qemu-system-arm`）。

## 文档导航

| 想了解 | 看这里 |
|--------|--------|
| 概念 + 文段/代码/结果/为什么 | [`docs/timer-notes.md`](docs/timer-notes.md) |
| 源码 | [`src/bare-metal/`](src/bare-metal/) |