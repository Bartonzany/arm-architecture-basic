# Arm SMMUv3 系统内存管理单元 —— 知识点验证代码与概念说明

本目录针对《26 - SMMU 软件指南（中文精读版）》整理。SMMUv3 的编程模型
（寄存器识别、流表、命令/事件队列、最小配置七步）都可在 **EL1 裸机
（QEMU `-machine virt,iommu=smmuv3`）** 环境真实验证。

## 目录结构

```
26-smmu/
├── README.md                # 本文件（入口 + 导航）
├── docs/smmu-notes.md       # ★ 概念 + 文段/代码/结果/为什么
├── src/                     # EL1 裸机示例
│   ├── boot.S               # 设栈 + VBAR_EL1 + 跳 main
│   ├── vector_table.S       # 16 入口向量表（本例不取中断）
│   ├── main.c               # 识别 IDR + 流表 + 队列 + 最小配置七步
│   ├── linker.ld / Makefile
├── Makefile / scripts/build_all.sh
└── build/                   # 产物（gitignore）
```

## 构建与运行

```bash
./scripts/build_all.sh            # 交叉编译 + 反汇编
./scripts/build_all.sh --run      # qemu-system-aarch64（iommu=smmuv3）
make run                          # 或 make 方式
```

依赖：`aarch64-linux-gnu-gcc`、`qemu-system-aarch64`（`apt install qemu-system-arm`）。

## 验证了什么

| 章节 | 概念 | 验证方式 |
| --- | --- | --- |
| §4.1 | 寄存器识别 | 读 `SMMU_IDR0/1/3/5`（匹配 MMU-600 模型 / 16 位 SID / 44 位 OAS） |
| §4.2 | 流表 | 配 `STRTAB_BASE`/`STRTAB_BASE_CFG`（线性流表，STE 无效） |
| §4.8 | 命令队列 | 配 `CMDQ_BASE/PROD/CONS`，发 TLBI+SYNC，`CONS` 从 0 推进到 2 |
| §4.9 | 事件队列 | 配 `EVENTQ_BASE/PROD/CONS`，确认无事件时空闲 |
| §4.11 | 最小配置七步 | `CR0` 使能队列/转换 + `CR0ACK` 轮询握手 + DSB |
| §4.10 | 全局错误 | `GERROR=0` 无错误 |
| §5.1 | I/O 一致性 | COHACC（QEMU 模型为一致访问） |

## 文档导航

| 想了解 | 看这里 |
| --- | --- |
| 概念 + 文段/代码/结果/为什么 | [`docs/smmu-notes.md`](docs/smmu-notes.md) |
| 源码 | [`src/`](src/) |