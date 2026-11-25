# AArch64 异常模型 —— 知识点验证代码

本目录针对《03 - AArch64 异常模型（中文精读版）》整理的验证代码与概念说明。
全部概念用一个裸机示例（`src/bare-metal/`）验证，运行在 QEMU `-machine virt` 上，
覆盖文档里的四大块内容：

- **异常级别 EL0–EL3**：复位从 EL3 逐级 `ERET` 降级到 EL0，每级打印自身级别与
  关键寄存器（`SCR_EL3`/`HCR_EL2`/`SCTLR_EL1`/`VBAR_ELx`）。
- **EL 跳转（SVC/HVC/SMC）**：EL0 发 `svc` → EL1 转发 `hvc` → EL2 转发 `smc` →
  EL3，再逐级 `ERET` 返回，完整演示“进入异常只升、异常返回只降”。
- **同步异常**：SVC（系统调用）、BRK（调试异常）、UDF（无效指令），处理器读
  `ESR_ELx.EC` 识别类型并处理。
- **异步异常**：EL0 启动定时器，IRQ 被送入 EL1 的 `+0x480` 向量，读 GIC 的 IAR/
  写 EOIR 完成处理；FIQ/SError 处理器已就位（QEMU 上不主动触发）。

## 目录结构

```
03-aarch64-exception-model/
├── README.md                # 本文件（入口 + 导航）
├── docs/exception-notes.md  # ★ 概念全解 + 概念↔代码映射 + 汇编对照
├── src/
│   └── bare-metal/          # 唯一代码文件夹（源码 + Makefile）
│       ├── boot.S           #   EL3 -> EL2 -> EL1 -> EL0 降级引导
│       ├── vector_table.S   #   16 入口共享向量表 + 异常处理器
│       ├── main.c           #   各级别报告 + 同步/异步异常演示
│       ├── linker.ld        #   链接脚本（4 个独立栈）
│       └── Makefile
├── scripts/build_all.sh
└── build/                   # 产物（gitignore）
```

## 构建与运行

```bash
make -C src/bare-metal build    # 编译（产物进 build/bare-metal/）
make -C src/bare-metal run      # qemu 运行（程序末尾死循环，按 Ctrl-C 退出）
./scripts/build_all.sh          # 一键构建 + 反汇编
```

依赖：`aarch64-linux-gnu-gcc`、`qemu-system-aarch64`（`apt install gcc-aarch64-linux-gnu qemu-system-arm`）。

运行要点：`-machine virt,secure=on,virtualization=on` 使 CPU 实现 EL3/EL2 并复位
进入 EL3；裸机从 EL3 开始逐级降级，最终在 EL0 完成各异常演示。

## 文档导航

| 想了解 | 看这里 |
|--------|--------|
| 概念全解 + 概念↔代码映射 + 汇编对照 | [`docs/exception-notes.md`](docs/exception-notes.md) |
| 源码 | [`src/bare-metal/`](src/bare-metal/) |
