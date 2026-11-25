# Arm 通用定时器 —— 知识点验证代码

本目录针对《05 - Arm 通用定时器（中文精读版）》整理，代码参考 ARM 官方
generic_timer 示例（`src/generic_timer.zip`）。核心机制（系统计数器、CVAL/TVAL
两种配置、定时器中断）在 **EL3 裸机**环境真实验证。

验证内容：
- **系统计数器**：`CNTFRQ_EL0`（频率，EL3 填入）、`CNTPCT_EL0`（物理计数）、
  `CNTVCT_EL0`（虚拟计数）；
- **CVAL vs TVAL**：两种配置方式（写 TVAL 内部填充 CVAL = 计数 + TVAL）；
- **两个定时器中断**（参考官方示例）：
  - 安全物理定时器 `CNTPS`：CVAL 设绝对时间，INTID 29；
  - 非安全 EL1 物理定时器 `CNTP`：TVAL 设相对 X tick，INTID 30；
  - 二者中断配成 Group 0（FIQ），重路由到 EL3，由 FIQ 处理器读 IAR / 按 INTID
    分派 / 清 ENABLE / 写 EOIR。

## 目录结构

```
05-generic-timer/
├── README.md                # 本文件（入口 + 导航）
├── docs/timer-notes.md      # ★ 概念 + 文段/代码/结果/为什么
├── src/
│   ├── bare-metal/          # EL3 裸机示例（源码 + Makefile）
│   │   ├── boot.S           #   EL3 设 CNTFRQ + SCR_EL3
│   │   ├── vector_table.S   #   EL3 向量表 + FIQ 处理器
│   │   ├── main.c           #   系统计数器 + CVAL/TVAL + 两定时器 + FIQ
│   │   ├── linker.ld / Makefile
│   └── generic_timer.zip    # ARM 官方参考示例（FVP 版）
├── Makefile / scripts/build_all.sh
└── build/                   # 产物（gitignore）
```

## 构建与运行

```bash
make -C src/bare-metal build    # 编译（产物进 build/bare-metal/）
make -C src/bare-metal run      # qemu 运行（程序末尾死循环，按 Ctrl-C 退出）
./scripts/build_all.sh          # 一键构建 + 反汇编
```

依赖：`aarch64-linux-gnu-gcc`、`qemu-system-aarch64`
（`apt install gcc-aarch64-linux-gnu qemu-system-arm`）。

运行要点：`-machine virt,secure=on` 使 CPU 复位进入 EL3（Secure 世界），全程在
EL3 运行；定时器中断配成 Group 0（FIQ），经 `SCR_EL3.FIQ=1` 路由到 EL3。

## 文档导航

| 想了解 | 看这里 |
|--------|--------|
| 概念 + 文段/代码/结果/为什么 | [`docs/timer-notes.md`](docs/timer-notes.md) |
| 源码 | [`src/bare-metal/`](src/bare-metal/) |