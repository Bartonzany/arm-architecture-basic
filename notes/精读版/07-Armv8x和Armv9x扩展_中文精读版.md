# 读懂 Armv8.x 和 Armv9.x 扩展（中文精读版）

> 原文：*Learn the architecture - Understanding the Armv8.x and Armv9.x extensions guide*
>
> 版本：Version 2.1　|　文档 ID：102378_0201_03_en　|　Issue 03（2024-05-10）
>
> 版权 © 2019, 2021, 2024 Arm Limited（或其关联公司）。保留所有权利。非保密。
>
> 说明：本文在原中文翻译的基础上重新组织了叙述逻辑与表达方式，技术内容与表格保持原意。

---

## 导读：这篇文章讲什么

Arm 架构的增补，以“扩展（extension）”这种版本增量形式发布。有了扩展，Arm 就能定期推出新特性来响应合作伙伴需求，而不必对主架构大动干戈。Arm 每年发布一个新扩展；Cortex CPU（Arm 对架构的实现）按发布时间使用对应的最新扩展。

读完你应当能：读懂标识扩展的命名方案；知道不同扩展里有哪些特性；判断某个 Arm Cortex CPU 支持某扩展的哪些特性。

## 1. 版本号怎么读：主版本与 .x 扩展

Arm 架构有不同版本，通常写成 ArmvX，X 是版本号。例如 Armv8-A 是 A-profile 架构的第 8 版，Armv9-A 是第 9 版——这是架构的**主版本（major release）**。

主版本之上还有**次要版本**，被称为 **.x 扩展**。例如 Armv8.1-A 表示“A-profile 第 8 版，叠加 .1 扩展”。

## 2. 为什么需要 .x 扩展

一个主版本的开发可能耗时多年：Armv7-A 于 2007 年发布，Armv8-A 六年后（2013 年）才推出。架构在主版本之间也需要演进、加入新特性，于是就有了次要版本——.x 扩展。

自 Armv8-A 发布起，“在主版本之间加内容”这件事被规范化：现在每年发布一个 .x 扩展。从 Armv8.0-A（基础规格说明）开始，Armv8.1-A 扩展于 2015 年加入，Armv8.2-A 于 2016 年加入。每个 .x 扩展都建立在前一个之上——例如 Armv8.2-A 包含 Armv8.1-A 的全部特性，再叠加新特性。这个过程延续到了 Armv9-A。

## 3. 特性如何命名：FEAT

架构中的每个特性都有形如 `FEAT_<name>` 的标识符：

- `FEAT_SHA256`：标识 Armv8.2-A 中加入的 SHA256 操作支持；
- `FEAT_BRBE`：标识 Armv9.2-A 中加入的分支记录缓冲扩展支持。

完整特性名列表见 Arm Developer 网站。

这些标识符在架构文档里被广泛使用。例如 AMCFGR 寄存器的描述：

> 此寄存器仅在实现 FEAT_AMUv1 时存在。否则，对 AMCFGR 的直接访问是 UNDEFINED 的。

《Arm Architecture Reference Manual》会说明，在 Armv8.x-A 和 Armv9.x-A 各版本中，哪些特性可选、哪些强制。

## 4. 处理器如何声明“我实现了哪个扩展”

每个 .x 扩展包含一组特性，其中有些强制、有些可选。处理器只要实现了**该扩展号的所有强制特性**，以及**此前所有扩展的强制特性**，就称它实现了该 .x 扩展。

例如，被描述为“实现 Armv8.2-A”的处理器，必须实现以下版本与扩展的全部强制特性：

- Armv8.0-A：基础规格说明、原始发布；
- Armv8.1-A：先前的扩展；
- Armv8.2-A：新扩展。

被描述为“实现 Armv9.1-A”的处理器，则必须实现：

- Armv9.0-A：基础规格说明、原始发布；
- Armv9.1-A：新扩展。

> 特性可能一开始可选、后来变强制。例如点积（Dot Product）指令在 Armv8.0-A 到 Armv8.3-A 一直可选，到 Armv8.4-A 才强制。类似地，某特性可能在 Armv8-A 可选、在 Armv9-A 强制。

一个 Armv8.x-A 处理器**可以**实现来自下一个 .x 扩展的特性，但**不能**实现任何更晚扩展的特性。

以“实现 Armv8.1-A”的处理器为例：

- 必须实现 Armv8.0-A 和 Armv8.1-A 的所有强制特性；
- 允许实现 Armv8.2-A 的部分特性；
- 不允许实现 Armv8.3-A 及更晚扩展的特性。

## 5. 各扩展新增特性速览

下面按扩展总结新增特性（只列最重要的，并非完整清单）。注意有些特性仅限 AArch64，有些在 AArch32 和 AArch64 都可用。

> AArch32 是 32 位执行状态，Armv8-A 之前的各版本都支持；AArch64 是 64 位执行状态，仅在 Armv8-A 及之后支持。

**Armv8.1-A**

- 原子内存访问指令（AArch64）
- 有限顺序区域（Limited Order regions）（AArch64）
- 增大虚拟机标识符（VMID）大小、虚拟化宿主扩展（AArch64）
- 特权访问禁止（Privileged Access Never，PAN）（AArch32 和 AArch64）

**Armv8.2-A**

- 对 52 位地址的支持（AArch64）
- PE 共享 TLB 表项的能力（AArch32 和 AArch64）
- FP16 数据处理指令（AArch32 和 AArch64）
- 统计分析（Statistical profiling）（AArch64）
- 可靠性、可用性、可服务性（RAS）支持变为强制（AArch32 和 AArch64）

**Armv8.3-A**

- 指针认证（Pointer authentication）（AArch64）
- 嵌套虚拟化（AArch64）
- 高级 SIMD 复数支持（AArch32 和 AArch64）
- 改进的 JavaScript 数据类型转换支持（AArch32 和 AArch64）
- 一项内存一致性模型改变（AArch64）
- 对更大的系统可见缓存提供 ID 机制支持（AArch32 和 AArch64）

**Armv8.4-A**

- 安全虚拟化（AArch64）
- 嵌套虚拟化增强（AArch64）
- 小翻译表支持（AArch64）
- 放宽的对齐限制（AArch32 和 AArch64）
- 内存分区与监控（MPAM）（AArch32 和 AArch64）
- 额外加密支持（AArch32 和 AArch64）
- 通用计数器缩放（AArch32 和 AArch64）
- 加速 SHA 的指令

**Armv8.5-A 和 Armv9.0-A**

- 内存标记（Memory Tagging）（AArch64）
- 分支目标识别（Branch Target Identification）（AArch64）
- 随机数生成器指令（AArch64）
- 清理缓存到深持久化点（Cache Clean to Point of Deep Persistence）（AArch64）

**Armv8.6-A 和 Armv9.1-A**

- 通用矩阵乘法（GEMM）指令（AArch64）
- 用于虚拟化的细粒度陷阱（AArch64）
- 高精度通用定时器
- 数据收集提示（Data Gathering Hint）（AArch64）

**Armv8.7-A 和 Armv9.2-A**

- 对 PCIe 热插拔的增强支持（AArch64）
- 到加速器的原子 64 字节加载和存储（AArch64）
- 带超时的 WFI/WFE（AArch64）
- 分支记录记录（Branch-Record recording）（仅 Armv9.2）

**Armv8.8-A 和 Armv9.3-A**

- 不可屏蔽中断（NMI）（AArch64）
- 优化 memcpy()/memset() 风格操作的指令（AArch64）
- PAC 的增强（AArch64）
- 带提示的条件分支（Hinted conditional branches）（AArch64）

## 6. 我的处理器实现了哪个扩展

Arm 架构提供一组**特性寄存器**，报告处理器支持的特性。对 .x 扩展新增的每个特性（包括可选特性），这些寄存器里都有一个字段报告它是否受支持。

例如 `ID_AA64MMFR2_EL1.AT` 告诉你是否支持 Armv8.4-A 中放宽的对齐要求。但**没有**字段直接报告“处理器是不是 Armv8.1-A”——软件要逐个读取 8.1-A 强制特性对应的字段，若全部存在，才能判定处理器符合 Armv8.1-A。

## 7. SBSA 对扩展的要求

服务器基础系统架构（SBSA）为服务器制定硬件要求，确保操作系统、hypervisor 和固件能正确运行。对标准化要求高的服务器，SBSA 规定了必须实现架构的哪些扩展。

下表总结与 Armv8.x-A 扩展相关的 SBSA 要求：

| 版本 | 特性 | SBSA 要求 |
| --- | --- | --- |
| Armv8.0-A | Advanced SIMD | 强制 |
| | Crypto 指令 | 强制（受出口限制约束） |
| | CRC | 强制 |
| | 4KB 和 64KB 颗粒 | 强制 |
| | 16 位 ASID | 强制 |
| | EL2 和 EL3 | 强制 |
| | 所有异常级别的 AArch64 | 强制 |
| | 至少 6 个 PMU 计数器 | 强制 |
| | 至少 6 个断点和 4 个同步观察点 | 强制 |
| Armv8.1-A | 16 位 VMID | 强制 |
| | 虚拟化宿主扩展 | 强制 |
| Armv8.2-A | RAS | 强制（至少最小实现） |
| | 持久内存 | 可选（有限制） |
| Armv8.3-A | 嵌套虚拟化 | 可选（有限制） |
| | 指针认证 | Level 4 强制，Level 3/5 可选（有限制） |
| Armv8.4-A | Stage 2 类型覆盖 | 强制 |
| | 增强嵌套虚拟化 | 强制 |
| | 活动监视器（Activity monitors） | 强制 |
| | MPAM | 可选（有限制） |
| | SHA3 和 SHA512 | 强制（受出口限制约束） |
| | 通用计数器缩放 | 强制 |

## 8. 知识自测

**Armv8.3 中的主版本和 .x 扩展分别是什么？**

8 是版本，3 是 .x 扩展。

**安全虚拟化是在 Armv8.4-A 加入的，Armv8.1-A 处理器能实现它吗？**

不能。Armv8.1 处理器必须实现 8.1 的强制特性，可实现 Armv8.2-A 的特性，但不允许实现 Armv8.3、Armv8.4 或更高版本的特性（除非特殊让步）。

**判断对错：只有可选特性才有报告其存在的字段。**

错。强制和可选特性都有字段报告存在性。

**Cortex-A55 实现了哪个主版本和扩展？**

Armv8.2-A。

**SBSA 哪个（些）级别要求支持 16 位 VMID？**

Level 3 及以上。

## 9. 延伸资料

相关安全指南：

- Introduction to security
- TrustZone for AArch64
- Providing protection for complex software
- Armv8-A Virtualization

其他资源：

- Arm architecture and reference manuals（Armv8.1-A、Armv8.2-A、Armv8.3-A、Armv8.4-A 信息）
- Arm community（提问，查找 Arm 专家专题文章与博客）

主题相关：

- Armv8.x-A 与 SBSA：Server Base System Architecture (SBSA)

## 10. 小结

Arm 每年为主架构发布一个扩展，提供满足合作伙伴需求的新特性。本指南解释了 Armv8.x 架构的扩展：如何阅读和使用它们，以及各扩展支持的若干特性。

复习之后，你应当理解扩展如何表达、哪些特性在哪些扩展中可用，以及如何确定某个 Arm Cortex CPU 支持哪些特性。要继续学习 Armv8-A 和 Armv9-A 架构，可参阅本系列其他指南。