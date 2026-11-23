# Arm GICv3 中断控制器 —— 知识点验证代码与概念说明

本目录针对《22 - GICv3 和 v4 概述（中文精读版）》整理。GICv3 的核心机制
（三大编程接口、初始化流程、IAR/EOI、运行优先级、SGI）都可在 **EL1 裸机
（QEMU `gic-version=3`）** 环境真实验证。

## 目录结构

```
22-gic/
├── README.md                # 本文件（入口 + 导航）
├── docs/gic-notes.md        # ★ 概念 + 文段/代码/结果/为什么
├── src/bare-metal/          # EL1 裸机示例
│   ├── boot.S               # 设栈 + VBAR_EL1 + 跳 main
│   ├── vector_table.S       # 16 入口向量表 + IRQ 处理器
│   ├── main.c               # 识别 GIC + 初始化 + 定时器 PPI + SGI
│   ├── linker.ld / Makefile
├── Makefile / scripts/build_all.sh
└── build/                   # 产物（gitignore）
```

## 构建与运行

```bash
./scripts/build_all.sh            # 交叉编译 + 反汇编
./scripts/build_all.sh --run      # qemu-system-aarch64 运行（gic-version=3）
make run                          # 或 make 方式
```

依赖：`aarch64-linux-gnu-gcc`、`qemu-system-aarch64`（`apt install qemu-system-arm`）。

## 验证了什么

| 章节 | 概念 | 验证方式 |
| --- | --- | --- |
| §7 | 三大编程接口（GICD/GICR/ICC_*） | 读 `GICD_TYPER`/`GICD_IIDR` 识别，`ICC_*` 用 MRS/MSR |
| §8.1 | 全局 GICD_CTLR | ARE + 三个 Enable 位 |
| §8.2 | 唤醒再分发器 | `GICR_WAKER` 清 ProcessorSleep |
| §8.2 | CPU 接口系统寄存器 | `ICC_SRE/PMR/BPR1/IGRPEN1/CTLR` |
| §8.3 | 每 INTID 配置 | 分组 + 优先级 + 使能（INTID 30/1） |
| §9.2 | 接受中断 | 读 `ICC_IAR1_EL1` 拿 INTID |
| §9.4 | 运行优先级 | `ICC_RPR_EL1`：确认后=优先级，EOI 后回 0xFF |
| §9.5 | 中断结束 | `EOImode=0` 写 `ICC_EOIR1_EL1` 一步完成 |
| §9.6 | 查询挂起 | `ICC_HPPIR1_EL1`（空闲=1023） |
| §10 | SGI | 写 `ICC_SGI1R_EL1` 给自己发 INTID 1 |

## 文档导航

| 想了解 | 看这里 |
| --- | --- |
| 概念 + 文段/代码/结果/为什么 | [`docs/gic-notes.md`](docs/gic-notes.md) |
| 源码 | [`src/bare-metal/`](src/bare-metal/) |
