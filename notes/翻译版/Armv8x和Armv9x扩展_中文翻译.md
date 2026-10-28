# Learn the architecture — 理解 Armv8.x 和 Armv9.x 扩展（中文翻译）

> 原文：*Learn the architecture - Understanding the Armv8.x and Armv9.x extensions guide*
>
> 版本：Version 2.1　|　文档 ID：102378_0201_03_en　|　Issue 03（2024-05-10）
>
> 版权 © 2019, 2021, 2024 Arm Limited（或其关联公司）。保留所有权利。非保密。

---

## 1. 概述

对 Arm 架构的增补以被称为“扩展（extension）”的版本增量形式提供。扩展使我们能够定期发布新特性，以响应合作伙伴的需求，而无需对主架构做出重大改变。

Arm 每年发布一个新的扩展。Cortex CPU（Arm 对架构的实现）根据其发布时间使用最新的扩展。

本指南解释对 Arm 架构的扩展，并提供关于如何阅读和使用它们的指导。

在本指南末尾，你可以检验你的知识。你将学到以下内容：

- 用于标识扩展的命名方案。
- 不同扩展中可用的特性。
- 如何确定 Arm Cortex CPU 实现支持某个扩展的哪些特性。

## 2. Armv8.x-A 和 Armv9.x 是什么意思？

Arm 架构有不同的版本。这些不同版本通常显示为 ArmvX，其中 X 是版本号。例如，Armv8-A 表示 Arm A-profile 架构的第 8 版，Armv9-A 表示 A-profile 架构的第 9 版。像 Armv8-A 这样的版本是架构的一个主要版本（major release）。

然而，还有被添加到主要版本中的次要版本。这些次要版本被称为 .x 扩展。例如，Armv8.1-A 表示 A-profile 架构的第 8 版，由 .1 扩展进行了扩展。

## 3. 为什么需要 .x 扩展？

Arm 架构主要版本的开发可能需要很多年。例如，Armv7-A 于 2007 年发布，Armv8-A 在六年后（2013 年）紧随其后。因为架构需要在主要版本之间演进以添加新特性，所以添加了次要版本，即 .x 扩展。

自 Armv8-A 发布以来，在主要版本之间向架构添加内容的过程已经规范化。现在每年发布一个 .x 扩展。从 Armv8.0-A（基础规格说明）开始，Armv8.1-A 扩展于 2015 年添加，然后 Armv8.2-A 扩展于 2016 年添加。每个 .x 扩展都建立在前一个之上。例如，Armv8.2-A 包含 Armv8.1-A 的所有特性，并添加了新特性。这个过程在 Armv9-A 中继续。

**扩展命名**

架构中的特性有一个标识符，形式为 FEAT_\<name\>。例如：

- FEAT_SHA256 标识对 Armv8.2-A 中添加的 SHA256 操作的支持。
- FEAT_BRBE 标识对 Armv9.2-A 中添加的分支记录缓冲扩展的支持。

你可以在 Arm Developer 网站上找到完整的特性名称列表。

Arm 架构文档广泛使用这些标识符。例如，以下是 AMCFGR 寄存器的描述：

> 此寄存器仅在实现 FEAT_AMUv1 时存在。否则，对 AMCFGR 的直接访问是 UNDEFINED 的。

《Arm Architecture Reference Manual》提供了关于不同版本 Armv8.x-A 和 Armv9.x-A 架构中哪些特性是可选或强制的信息。

## 4. 处理器实现

每个 .x 扩展包含一组特性，其中一些是强制的，一些是可选的。如果一个处理器实现了该扩展号的所有强制特性，以及所有先前扩展的强制特性，那么它就实现了该 .x 扩展。

例如，一个被描述为实现 Armv8.2-A 的处理器，必须实现以下架构发布和扩展的所有强制特性：

- Armv8.0-A —— 基础规格说明和原始发布
- Armv8.1-A —— 先前的扩展
- Armv8.2-A —— 新的扩展

类似地，一个被描述为实现 Armv9.1-A 的处理器，必须实现以下架构发布和扩展的所有强制特性：

- Armv9.0-A —— 基础规格说明和原始发布
- Armv9.1-A —— 新的扩展

> 一个特性可能最初是可选的，但后来变为强制。例如，点积（Dot Product）指令在 Armv8.0-A 到 Armv8.3-A 的所有扩展中都是可选的，但在 Armv8.4-A 中变为强制。类似地，一个特性可能在 Armv8-A 中是可选的，但在 Armv9-A 中是强制的。

一个 Armv8.x-A 处理器可以实现来自下一个 .x 扩展的任何特性。然而，它不能实现来自任何更晚 .x 扩展的特性。

例如，一个被描述为实现 Armv8.1-A 的处理器：

- 必须实现 Armv8.0-A 和 Armv8.1-A 的所有强制特性
- 被允许实现来自 Armv8.2-A 的一些特性
- 不被允许实现来自 Armv8.3-A 及更晚扩展的特性

## 5. Armv8.x 和 Armv9.x 扩展与特性

在本节中，我们总结在每个 Armv8.x-A 和 Armv9.x-A 扩展中添加的新特性。我们不提供完整的列表，但包含最重要的特性。请注意，有些特性仅限于 AArch64 状态，而其他特性在 AArch32 和 AArch64 状态中都可用。

> AArch32 是 32 位执行状态，在 Armv8-A 之前的所有版本 Arm 架构中都受支持。AArch64 是 64 位执行状态，仅在 Armv8-A 架构中受支持。

**Armv8.1-A**

- 原子内存访问指令（AArch64）
- 有限顺序区域（Limited Order regions）（AArch64）
- 增大虚拟机标识符（VMID）大小，以及虚拟化宿主扩展（AArch64）
- 特权访问禁止（Privileged Access Never，PAN）（AArch32 和 AArch64）

**Armv8.2-A**

- 对 52 位地址的支持（AArch64）
- PE 共享翻译后备缓冲器（TLB）表项的能力（AArch32 和 AArch64）
- FP16 数据处理指令（AArch32 和 AArch64）
- 统计分析（Statistical profiling）（AArch64）
- 可靠性、可用性、可服务性（RAS）支持变为强制（AArch32 和 AArch64）

**Armv8.3-A**

- 指针认证（Pointer authentication）（AArch64）
- 嵌套虚拟化（AArch64）
- 高级 SIMD 复数支持（AArch32 和 AArch64）
- 改进的 JavaScript 数据类型转换支持（AArch32 和 AArch64）
- 对内存一致性模型的一项改变（AArch64）
- 对更大的系统可见缓存的 ID 机制支持（AArch32 和 AArch64）

**Armv8.4-A**

- 安全虚拟化（AArch64）
- 嵌套虚拟化增强（AArch64）
- 小翻译表支持（AArch64）
- 放宽的对齐限制（AArch32 和 AArch64）
- 内存分区与监控（MPAM）（AArch32 和 AArch64）
- 额外的加密支持（AArch32 和 AArch64）
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
- 带超时的等待指令（WFI）和等待事件（WFE）（AArch64）
- 分支记录记录（Branch-Record recording）（仅 Armv9.2）

**Armv8.8-A 和 Armv9.3-A**

- 不可屏蔽中断（NMI）（AArch64）
- 优化 memcpy() 和 memset() 风格操作的指令（AArch64）
- PAC 的增强（AArch64）
- 带提示的条件分支（Hinted conditional branches）（AArch64）

## 6. 我的处理器实现了哪个扩展？

Arm 架构包含一组特性寄存器，报告处理器支持的特性。对于 .x 扩展添加的每个新特性（甚至可选特性），这些特性寄存器中的一个字段报告它是否受支持。

例如，ID_AA64MMFR2_EL1.AT 告诉你是否支持 Armv8.4-A 中放宽的对齐要求。没有字段报告处理器是否是 Armv8.1-A。相反，软件读取强制 8.1-A 特性的特性字段，如果它们全部存在，则处理器符合 Armv8.1-A。

## 7. Armv8.x-A 与 SBSA

服务器基础系统架构（SBSA）为服务器提供硬件要求。SBSA 确保操作系统、hypervisor 和固件正确运行。对于标准化程度很重要的服务器，SBSA 包含关于必须实现架构的哪些扩展的规则。

下表总结了与 Armv8.x-A 扩展相关的 SBSA 要求：

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

**问：Armv8.3 中的主要版本和 .x 扩展分别是什么？**

8 是版本，3 是 .x 扩展。

**问：安全虚拟化是在 Armv8.4-A 中添加的。Armv8.1-A 处理器会被允许实现它吗？**

不会。Armv8.1 处理器必须实现 Armv8.1 的强制特性，并且可以实现来自 Armv8.2-A 的特性。但它不被允许实现来自 Armv8.3、Armv8.4 或更高版本的特性，除非做出了特殊让步。

**问：这句话是对还是错？只有可选特性才有报告其存在的字段。**

错。强制和可选特性都有报告其存在的字段。

**问：Cortex-A55 实现了 Arm 架构的哪个主要版本和扩展？**

Armv8.2-A。

**问：SBSA 的哪个或哪些级别要求支持 16 位 VMID？**

Level 3 及以上。

## 9. 相关资料

要了解更多关于本指南中引用的以下特性的信息，请参考：

- 安全指南：
  - Introduction to security
  - TrustZone for AArch64
  - Providing protection for complex software
- Armv8-A Virtualization

以下是与本指南内容相关的一些资源：

- Arm architecture and reference manuals（关于 Armv8.1-A、Armv8.2-A、Armv8.3-A、Armv8.4-A 的信息）
- Arm community（提出开发问题，并查找 Arm 专家针对特定主题的文章和博客）

以下是与本指南主题相关的资源：

- Armv8.x-A 与 SBSA：
  - Server Base System Architecture (SBSA)

## 10. 下一步

每年，Arm 都会发布其主要架构的扩展，提供支持合作伙伴需求的新特性。在本指南中，我们解释了 Armv8.x 架构的扩展，描述了如何阅读和使用这些扩展，并概述了这些扩展所支持的一些特性。

复习本指南后，你应该理解扩展是如何表达的、哪些特性在哪些扩展中可用，以及如何确定 Arm Cortex CPU 实现支持哪些特性。

要继续学习 Armv8-A 和 Armv9-A 架构，请参阅我们的系列指南。
