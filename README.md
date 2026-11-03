# ARM 架构学习笔记（arm-architecture-basic）

本仓库围绕 Arm 官方 **Learn the Architecture** 系列（A-profile，Armv8-A / Armv9-A）与《Cortex-A 系列程序员指南》整理学习资料：原始 PDF、中文翻译、中文精读、插图，以及配套的可编译验证代码。

> 官方原文入口：<https://www.arm.com/zh-cn/architecture/learn-the-architecture/a-profile>

## 目录结构

```
arm-architecture-basic/
├── docs/                      # 官方 PDF 原文（15 份）
├── notes/
│   ├── 翻译版/                # 忠实中文翻译（保留原文结构）
│   └── 精读版/                # 重组逻辑的中文精读（导读 + 去翻译腔）
├── asset/                     # 从 PDF 提取的插图（figureN-M-*.png）
├── code/                      # 各文档的知识点验证代码（C++/裸机）
├── .clang-format              # C++ 代码风格
├── ISA_A64_xml_v88A-*.pdf     # A64 指令集 XML 规范
└── README.md
```

## 两种中文版本的区别

| 版本 | 目录 | 特点 |
| --- | --- | --- |
| **翻译版** | `notes/翻译版/` | 忠实对应原文，保留原始章节编号与结构，代码/表格/图注原样。 |
| **精读版** | `notes/精读版/` | 面向"真正读懂"重新组织叙述：开头加「导读」，去翻译腔、按中文逻辑分节，保留全部技术事实、表格、代码与插图。 |

两份文档共用 `asset/` 中的插图，图中以 `![图 X-Y：说明](../../asset/figureX-Y-*.png)` 引用。

## 文档清单

文档按主题分为三组：**架构基础知识（01–07）**、**内存管理与内存模型（08–10）**、**Neon / Advanced SIMD（11–12）**。

### 01–07 · 架构基础知识

| 编号 | 原文档（docs/） | 翻译版 | 精读版 |
| --- | --- | --- | --- |
| 01 | introducing the arm architecture | Arm架构入门_中文翻译.md | 01-Arm架构入门_中文精读版.md |
| 02 | aarch64_exception_model | AArch64_异常模型_中文翻译.md | 03-AArch64_异常模型_中文精读版.md |
| 03 | a64_instruction_set_architecture_guide | A64指令集架构指南_中文翻译.md | 02-A64指令集架构指南_中文精读版.md |
| 04 | aarch64_virtualization_guide | AArch64虚拟化指南_中文翻译.md | 04-AArch64虚拟化指南_中文精读版.md |
| 05 | generic_timer | 通用定时器_中文翻译.md | 05-通用定时器_中文精读版.md |
| 06 | architecture_Programmers_Guide | Cortex-A系列程序员指南_中文翻译.md | 06-Cortex-A系列程序员指南_中文精读版.md |
| 07 | understanding_armv8.x_and_armv9.x_extensions | Armv8x和Armv9x扩展_中文翻译.md | 07-Armv8x和Armv9x扩展_中文精读版.md |

### 08–10 · 内存管理与内存模型

| 编号 | 原文档（docs/） | 翻译版 | 精读版 |
| --- | --- | --- | --- |
| 08a | aarch64_memory_management_guide | AArch64内存管理_中文翻译.md | 08a-AArch64内存管理_中文精读版.md |
| 08b | aarch64_memory_management_examples | AArch64内存管理示例_中文翻译.md | 08b-AArch64内存管理示例_中文精读版.md |
| 09 | aarch64_memory_attributes_and_properties | AArch64内存属性与特性_中文翻译.md | 09-AArch64内存属性与特性_中文精读版.md |
| 10 | memory_systems_ordering_and_barriers | 内存系统排序与屏障_中文翻译.md | 10-内存系统排序与屏障_中文精读版.md |

### 11–13 · Neon / Advanced SIMD

| 编号 | 原文档（docs/） | 翻译版 | 精读版 |
| --- | --- | --- | --- |
| 11 | introducing_neon | Neon入门_中文翻译.md | 11-Neon入门_中文精读版.md |
| 12 | optimizing_c_code_with_neon_intrinsics | 用Neon内建函数优化C代码_中文翻译.md | 12-用Neon内建函数优化C代码_中文精读版.md |
| 13 | compiling_for_neon_with_auto-vectorization | 为Neon自动向量化编译_中文翻译.md | 13-为Neon自动向量化编译_中文精读版.md |

尚未整理（docs/ 中已有 PDF）：

- `DEN0018A_neon_programmers_guide.pdf`
- `ISA_A64_xml_v88A-2021-12_OPT_RETIRED.pdf`

## 配套验证代码

`code/` 下每个目录对应一份文档，把其中的关键知识点写成最小可复现程序，配合 `-O2` 交叉编译 + 反汇编（或 QEMU 裸机）验证"代码 → 指令/硬件行为"的对应关系。各目录内含独立的 `README.md` 说明。

| 目录 | 对应文档 | 验证方式 |
| --- | --- | --- |
| `02-a64-instruction-set` | A64 指令集架构指南 | 17 个 C++ demo + `-O2` AArch64 交叉编译反汇编对照 |
| `03-aarch64-exception-model` | AArch64 异常模型 | 同步/异步异常复现 |
| `04-aarch64-virtualization` | AArch64 虚拟化指南 | 虚拟化机制验证 |
| `05-generic-timer` | 通用定时器 | 裸机定时器验证 |
| `08-mmu-examples` | AArch64 内存管理示例 | EL1/EL2/EL3 各阶段页表转换示例 |
| `09-memory-attributes` | AArch64 内存属性与特性 | EL1 裸机（QEMU virt）验证 Normal/Device、MAIR、权限等 |
| `10-memory-ordering` | 内存系统排序与屏障 | EL1 裸机验证排序与屏障 |
| `12-neon-intrinsics` | 用 Neon 内建函数优化 C 代码 | 3 个 C++ demo（RGB 去交错 / 矩阵乘法 / 碰撞检测）+ `-O2` 交叉编译反汇编 + qemu 运行 |
| `13-neon-auto-vectorization` | 为 Neon 自动向量化编译 | 2 个 C 示例，`-O1`（标量）vs `-O3`（向量）反汇编对照 + qemu 运行 |

## 使用方式

```bash
# 进入某个验证代码目录
cd code/02-a64-instruction-set

# 交叉编译 + 反汇编
./scripts/build_all.sh

# 裸机示例（如 09/10）可直接在 QEMU 上运行
./scripts/build_all.sh --run
```

> 需安装 `aarch64-linux-gnu-g++`（交叉编译）与 `qemu-system-aarch64`（裸机运行）。

## 术语约定

翻译与精读统一使用一套术语，例如：Exception level→异常级别、Execution state→执行状态、Normal/Device memory→普通/设备内存、translation table→转换表、barrier→屏障、cache coherency→缓存一致性、lane→通道、intrinsic→内建函数、aliasing→别名等；寄存器名、指令助记符、型号与版本号（如 `Cortex-A57`、`AArch64`、`FEAT_*`）保留原文。

## 参考

- Arm Learn the Architecture（A-profile）：<https://www.arm.com/zh-cn/architecture/learn-the-architecture/a-profile>
- Arm Architecture Reference Manual（ARM ARM）
- 各处理器 Technical Reference Manual / Software Optimization Guide
