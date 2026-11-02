# MMU 初始化示例 —— 知识点验证代码与概念说明

本目录收录 Arm 官方《Basic MMU Initialization Sequences》的 10 个
AArch64 MMU 初始化示例，并按本仓库统一约定整理成 `src/` / `docs/` /
`scripts/` 结构。这些示例不是可运行的完整应用，而是"最小 MMU 打开流程"骨架，
用于对照理解 EL1/EL2/EL3 在不同页表级数（L0/L1/L2）、stage1/stage2、
VHE(E2H)、硬件 AF 更新等配置下的差异。

## 目录结构

```
08-mmu-examples/
├── README.md                     # 本文件（入口 + 导航）
├── docs/
│   ├── mmu-notes.md              # ★ MMU 概念 + 10 示例索引 + 逐示例说明
│   └── upstream-ReadMe.md        # Arm 原始说明（FVP 运行参数，已转 Markdown）
├── src/                          # 10 个示例（每个含 startup.S + 原版 Makefile）
│   ├── el1_stage1/
│   ├── el1_stage1_and_stage2/
│   ├── el2_stage1_l1andl2/
│   ├── el2_stage1_l1only/
│   ├── el2_stage1_l1only_e2h/
│   ├── el3_AF_update/
│   ├── el3_stage1_full_mem_map/
│   ├── el3_stage1_l0_to_l2/
│   ├── el3_stage1_l1andl2/
│   └── el3_stage1_l1only/
├── Makefile / scripts/build_all.sh
└── build/                        # 产物（gitignore）
```

## 构建与验证

```bash
./scripts/build_all.sh            # 汇编 + 链接 + 反汇编全部 10 个示例
make                              # 或 make 方式
```

依赖：`aarch64-linux-gnu-as` / `aarch64-linux-gnu-ld` /
`aarch64-linux-gnu-objdump`（`apt install gcc-aarch64-linux-gnu`）。

产物输出到 `build/<示例名>/image.elf`（链接后 ELF）与 `image.dis`（反汇编），
供静态阅读对照。

## 运行环境（重要）

原示例面向 **Arm FVP Base Platform AEM**（需 `armclang` 及特定 `-C` 参数，见
[`docs/upstream-ReadMe.md`](docs/upstream-ReadMe.md)），**不能**在 QEMU 上原样
运行。本仓库只提供 GNU 工具链的汇编/链接/反汇编，用于静态学习。

## 文档导航

| 想了解 | 看这里 |
|--------|--------|
| MMU 概念 + 10 示例索引 + 逐示例说明 | [`docs/mmu-notes.md`](docs/mmu-notes.md) |
| Arm 原始说明（FVP 参数） | [`docs/upstream-ReadMe.md`](docs/upstream-ReadMe.md) |
| 源码 | [`src/`](src/) |
