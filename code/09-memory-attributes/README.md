# AArch64 内存属性与特性 —— 知识点验证代码与概念说明

本目录针对《09 - AArch64 内存属性与特性（中文精读版）》整理。该文档几乎全是
特权级/MMU 内容，故用 **EL1 裸机（QEMU virt）** 真实验证核心机制：内存类型
（Normal vs Device）、MAIR 间接寄存器、权限（AP）、执行权限（PXN）、对齐
（SCTLR.A）、访问标志（AF 软件更新）。

## 目录结构

```
09-memory-attributes/
├── README.md                        # 本文件（入口 + 导航）
├── docs/memory-attributes-notes.md  # ★ 概念 + 文段/代码/结果/为什么
├── src/bare-metal/                  # EL1 裸机示例
│   ├── boot.S                       # 设栈 + VBAR_EL1 + 跳 main
│   ├── vector_table.S               # 16 入口向量表 + 同步异常处理器
│   ├── main.c                       # 建页表 + 开 MMU + 各验证用例 + 异常处理
│   ├── linker.ld                    # 三级页表 + 4 个测试页（固定地址）
│   └── Makefile
├── Makefile / scripts/build_all.sh
└── build/                           # 产物（gitignore）
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
| [ B ] | 两种内存类型 + MAIR 间接寄存器（§3/§5/§6）| — |
| [ A ] | Device 内存（nGnRnE）映射（§3/§4）| — |
| [ C ] | 权限 AP=只读 → 权限故障（§7）| EC=0x25 FSC=0x0F |
| [ D ] | 执行权限 PXN → 指令中止（§7.2）| EC=0x21 FSC=0x0F |
| [ E ] | 对齐 SCTLR.A 开关（§10）| EC=0x25 FSC=0x21 |
| [ F ] | 访问标志 AF 软件更新（§9）| EC=0x25 FSC=0x0B |

## 文档导航

| 想了解 | 看这里 |
|--------|--------|
| 概念 + 文段/代码/结果/为什么 | [`docs/memory-attributes-notes.md`](docs/memory-attributes-notes.md) |
| 源码 | [`src/bare-metal/`](src/bare-metal/) |
