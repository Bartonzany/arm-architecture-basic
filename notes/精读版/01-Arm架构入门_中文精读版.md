# 读懂 Arm 架构：从配置文件到文档地图（中文精读版）

> 原文：*Learn the architecture - Introducing the Arm architecture*
>
> 版本：Version 2.3　|　文档 ID：102404_0203_02_en　|　Issue 02（2026-01-07）
>
> 版权 © 2019, 2021, 2023–2026 Arm Limited（或其关联公司）。保留所有权利。非保密。
>
> 说明：本文在原中文翻译的基础上重新组织了叙述逻辑与表达方式，技术内容、表格与图表保持原意；个别明显笔误已按 Arm 架构规范订正。

---

## 导读：这篇文章想解决什么问题

如果你刚开始接触 Arm，最容易卡住的往往不是某条指令，而是几个更基础的问题：

- Arm 架构到底“规定”了什么，又不管什么？
- A-Profile、R-Profile、M-Profile 有什么区别？
- Armv8-A、Armv9-A、Armv9.2-A 这些名字怎么读？
- 面对 ARM ARM、TRM、CIM、SoC 数据手册，该查哪一本？

本指南不需要先备知识，但假设你已经熟悉处理器和编程的基本概念。读完之后，你应当能说清 Arm 架构的几种配置文件、区分“架构”与“微架构”，并知道在 Arm 的文档体系里，什么问题该去哪份文档找答案。

## 1. Arm 架构是什么

Arm 架构为处理器或内核的设计奠定基础。这里的“处理器或内核”，在 Arm 的语境中通常称为**处理单元（Processing Element，PE）**。

它被广泛用于智能手机、微型计算机、嵌入式设备、服务器乃至超级计算机等片上系统（System-on-Chip，SoC）。

对软件开发者而言，Arm 架构暴露的是一套通用的指令集和特性，也就是所谓的**程序员模型（Programmer's model）**。这套模型保证了不同实现之间的互操作性：同一份软件，可以运行在不同厂商、不同型号的 Arm 设备上。

## 2. 三种配置文件：A、R、M

Arm 架构是世界上最流行的处理器架构之一，每年出货数十亿台基于 Arm 的设备。

为了覆盖不同市场，架构划分出三种**配置文件（profile）**：

**表 2-1：Arm 架构配置文件**

| 配置文件 | 定位 | 说明 |
| --- | --- | --- |
| A-Profile（Applications，应用） | 高性能、特性丰富 | 旨在运行 Linux、Windows 这类复杂操作系统 |
| R-Profile（Real-Time，实时） | 面向实时或确定性要求 | 常见于网络设备和嵌入式控制系统 |
| M-Profile（Microcontroller，微控制器） | 小型、高能效 | 大量物联网（IoT）设备的核心 |

三者的关系可以这样理解：它们共享一部分基础特性，又各自针对不同场景做取舍，因此 Arm 架构既能保持统一，又能覆盖从微控制器到服务器的广阔范围。

> Arm® Cortex®、Arm® Neoverse™ 和 Arm® SecurCore® 是 Arm 自家 IP 产品的品牌名示例；合作伙伴也会基于 Arm 架构推出自己的处理器品牌。

一个典型的基于 Arm 的系统如图所示：

![图 2-1：关于 Arm 架构](../../asset/figure2-1-about-the-arm-architecture.png)

*图 2-1：关于 Arm 架构*

以一部智能手机为例，里面可能同时存在多种处理器：

- 一个 A-profile 处理器作为主 CPU，运行 Android 之类的富操作系统；
- 一个 R-profile 处理器构成蜂窝调制解调器，负责连接能力；
- 若干 M-profile 处理器负责系统电源管理等任务；
- SIM 卡使用 SecurCore——一种带额外安全特性的 M-profile 处理器，常用于智能卡。

本指南只讨论 A-profile 架构，并且聚焦于它的两个最新版本：Armv8-A 和 Armv9-A。

## 3. “架构”到底规定了什么

这里的“架构（architecture）”指的是**功能规格说明（functional specification）**。对 Arm 而言，就是处理器的功能规格说明：它规定处理器应当如何表现，例如有哪些指令、每条指令做什么。

可以把架构看作硬件与软件之间的一份“契约（contract）”。软件依赖这份契约，相信硬件会提供约定的能力。当然，契约中允许存在可选项，这一点在“架构与微架构”一节会展开。

**表 3-1：Arm 架构定义的关键要素**

| 特性 | 描述 |
| --- | --- |
| 指令集 | 可用指令的列表；每条指令的功能，包括它可能触发的任何异常；指令在内存中的表示方式，即其编码 |
| 寄存器组 | 可用寄存器的集合；寄存器的大小；寄存器的功能；这些寄存器的复位行为 |
| 异常模型 | 不同级别的特权以及每个特权级别可用的功能；异常的类型以及触发它们的原因；进入异常或从异常返回时会发生什么 |
| 内存模型 | 内存访问如何被排序；缓存如何行为，以及软件何时、如何执行显式维护 |
| 调试、跟踪与性能分析 | 断点如何被设置和触发；跟踪工具可以捕获哪些信息以及以何种格式 |

## 4. 架构与微架构：做什么，怎么做

一句话区分：

- **架构（architecture）** 告诉你处理器“做什么”；
- **微架构（microarchitecture）** 告诉你处理器“怎么做”。

符合 Arm 架构之所以重要，是因为它保证了与庞大 Arm 软件/工具生态的兼容性，也保证了所有符合 Arm 的处理器之间的互操作性。

由此可以得到一条硬性要求：每一个符合 Arm 的处理器，都必须实现架构规定的每一条指令，同时不能自行引入架构之外的额外指令。

微架构则留给实现者发挥，包括：

- 流水线的长度与布局；
- 缓存的数量与大小；
- 单条指令的周期数；
- 实现了哪些可选特性。

以 Arm® Cortex®-A53 和 Cortex®-A72 为例，两者都是 Armv8-A 架构的实现，架构相同，微架构却差别很大：

![图 4-1：Arm Cortex-A53 与 Arm Cortex-A72 内核](../../asset/figure4-1-cortex-a53-a72.png)

*图 4-1：Arm Cortex-A53 与 Arm Cortex-A72 内核*

**表 4-1：Arm Cortex-A53 与 Arm Cortex-A72 之间微架构差异示例**

| 项目 | Cortex-A53 | Cortex-A72 |
| --- | --- | --- |
| 目标 | 针对能效优化 | 针对性能优化 |
| 流水线 | 8 级，顺序（in-order） | 15+ 级，乱序（out-of-order） |
| 缓存 | L1 I 缓存：8KB–64KB；L1 D 缓存：8KB–64KB；L2 缓存：可选，最高 2MB | L1 I 缓存：48KB 固定；L1 D 缓存：32KB 固定；L2 缓存：强制，最高 2MB |

也正因如此，架构上合规的软件无需修改，就能在 Cortex-A53 或 Cortex-A72 上运行——它们实现的是同一套架构。

## 5. 架构如何演进

Arm 架构是逐步演进的，每个版本都建立在前一版本之上。

版本命名可以先记住一个例子：

**Armv9-A**（v9-A）表示“第 9 版架构，面向 A-profile”。

对 A-profile 来说，当前两个主要版本是 Armv8 和 Armv9。

### 5.1 Armv8-A 与 Armv9-A

**Armv8-A** 于 2011 年发布，是 Arm 架构的第一个 64 位版本。基于 Armv8-A 的设备已经覆盖从手机到超级计算机的广泛领域。

**Armv9-A** 是面向 A-profile 的最新版本，在 Armv8-A 基础上构建，新增特性包括：

- 可扩展向量扩展第 2 版（SVE2）；
- 可扩展矩阵扩展（SME、SME2）；
- Realm Management Extension（RME）；
- 受保护的控制栈（GCS）；
- 分支记录缓冲扩展（BRBE）；
- 嵌入式跟踪扩展（ETE）；
- 跟踪缓冲扩展（TRBE）。

Armv9-A 同时也调整了一些既有特性，例如把 AArch32 的支持限制在 EL0。

### 5.2 年度更新与版本对齐

Arm 每年发布一次架构更新，加入新特性、扩展已有特性。年度更新用次版本号标识，例如 Armv9.3-A 于 2022 年发布。

Armv9-A 刚引入时，Armv9.0-A 与 Armv8.5-A 是对齐的：Armv9.0-A 包含 Armv8.5-A 的全部特性，再加上仅属于 Armv9 的特性（如 SVE2）。

Armv8-A 与 Armv9-A 并行更新。加入 Armv8 的特性会被对应版本的 Armv9 继承，但反过来不成立——有些特性只存在于 Armv9-A。二者的对齐关系及部分特性示例见图 5-1。

![图 5-1：版本发布](../../asset/figure5-1-releases.png)

*图 5-1：版本发布*

有时为了表达方便，会按发布年份统称 Armv8 和 Armv9 的更新。例如 Armv8.9-A 和 Armv9.4-A 合称“2022 扩展（2022 extensions）”。

每次年度更新加入了哪些特性，可以在《Arm Architecture Reference Manual》以及 Arm 网站的博客文章中查到：

- Arm A-Profile Architecture Developments 2024
- Arm A-Profile Architecture Developments 2023
- Arm A-Profile Architecture Developments 2022
- Arm A-Profile Architecture Developments 2021
- Arm A-Profile Architecture Developments 2020
- Developments in the Arm A-Profile Architecture: Armv8.6-A

### 5.3 用 FEAT 标识特性

Arm 文档用一套专门记法来标识特性：

```text
FEAT_<name>
```

通常，一个高层特性由一组 FEAT 组成。例如 Realm Management Extension（RME）就包含：

- FEAT_RME
- FEAT_RME_GDI
- FEAT_RME_GPC2
- FEAT_RME_GPC3

对每个 FEAT，架构会定义：

- 它在哪个架构版本中引入；
- 从哪个架构版本起允许被实现；
- 某些情况下，从哪个版本起成为强制要求；
- 该特性的约束，例如 FEAT_RME_GPC2 依赖 FEAT_RME。

一个特性往往可以在“引入版本”之前就开始实现。以 FEAT_MOPS 为例：

- 在 Armv8.8-A 中引入，属于 2022 扩展；
- 允许从 Armv8.7-A 或 Armv9.2-A 起实现；
- 从 Armv8.8-A 和 Armv9.3-A 起成为强制要求。

完整特性列表可以在 Arm 网站上以 HTML 或 JSON 形式查阅。

## 6. 系统架构与组件架构

《Arm Architecture Reference Manual》（常简称 Arm ARM）是最广为人知的 Arm 规格说明，但它描述的是 PE，而不是整个系统。

现实中的系统远不止一个处理器内核：还需要内存、I/O 设备、电源控制、监控工具、调试能力，以及把这些部件连接起来的硬件；此外还要有操作系统之类的系统软件来管理它们。Arm ARM 给设计者留出了很大自由度，以便针对从微型嵌入式到超级计算机的不同场景做优化。

为了让硬件和软件能够复用，Arm 及其合作伙伴定义了一系列**系统架构**：为各类组件规定软硬件接口，解决共性问题，并在此基础上鼓励创新。它们通过适配 Arm 的独特特性，对其他行业标准形成补充。

系统架构的分组方式如图 6-1 所示。

![图 6-1：系统架构分组](../../asset/figure6-1-system-architecture-groups.png)

*图 6-1：系统架构分组*

**硬件规格说明**

除了 Arm ARM 这类处理器规格说明，系统架构还包括针对 SoC 其他要素的硬件规格说明。它们描述与相邻硬件组件的接口，以及软件如何管理这些组件（例如通过寄存器）。主要包括：

- 支撑处理器内核的标准系统组件，特别是通用中断控制器（GIC）、系统内存管理单元（SMMU），以及 CoreSight 调试与跟踪；
- 电源管理：一个专门子系统，内含嵌入式系统控制处理器（SCP），负责控制和监控整个 SoC 的电源，并通过系统控制与管理接口（SCMI）与主系统软件对接；
- AMBA 接口家族：提供对内存和内存映射设备（如外设）的高效访问。AMBA 被广泛采用、特性丰富且与架构无关，拥有成熟且活跃的第三方生态，能提供兼容的产品与方案。

**固件规格说明**

固件是低层软件，通常随 SoC 或开发板提供，用来支撑操作系统或虚拟机监控器等上层系统软件。固件通常需要与具体实现的硬件能力对齐。面向 A-profile 系统的固件接口规格包括：

- 处理器内核在特权异常级别与安全状态之间切换所需的接口，相关规格有 SMC 调用约定（SMCCC）、A-profile 固件框架（FF-A）以及 Arm 机密计算架构（CCA）；
- 一组标准的安全与监控接口，例如管理 CPU 与系统电源状态的电源状态协调接口（PSCI）；
- 面向行业标准“高级配置与电源接口（ACPI）”的 Arm 专属补充。

这些硬件与固件规格说明，为系统设计者提供了一套可独立取用的“积木”。不过，一些常见的高层兼容性问题要求对能力集合做标准化，因此部分系统架构规格说明会定义一组基础硬件与固件功能。这些基础标准定义了 Arm 专属规格与行业规格的配置文件，设计者可以在此基础上继续增值。例如基础启动要求（BBR）、基础系统架构（BSA）及其面向特定市场的补充，就使得标准现成操作系统能够直接获得支持。

系统架构的整体概览，见《Learn the architecture - Arm System Architectures》。

## 7. 读懂 Arm 文档

Arm 的文档数量很多。这一节解决两个实际问题：文档在哪里，以及哪份文档回答哪类问题。

### 7.1 文档在哪里找

Arm 架构和处理器手册可以从 Arm Developer 网站下载。开发中的问题可以在 Arm Community 提出，也能在那里找到 Arm 专家撰写的专题文章和博客。

### 7.2 每类文档负责什么

- **Arm Architecture Reference Manuals**：描述 PE 架构，与该架构的任何实现都相关。某些组件（如 GIC、SMMU）有各自独立的架构规格说明。
- **技术参考手册（TRM）**：每款 Arm Cortex 或 Arm Neoverse 处理器都有一份，描述该处理器特有的特性。通常，TRM 不会重复 Arm ARM 中已经写过的内容。部分处理器还提供**软件优化指南（Software Optimization Guide）**，包含指令时序与调度信息。
- **配置与集成手册（CIM）**：每款 Cortex 或 Neoverse 处理器通常都有一份，说明如何把该处理器集成进系统。通常只与 SoC 设计者相关。
- **系统架构规格说明**：在适用时给出系统或软件层面的规则与定义。

> CIM 仅向 IP 授权方（licensee）开放；TRM 可以从 Arm Developer 下载。

### 7.3 一个例子：使用 Neoverse V3 时需要哪些文档

如果你要查某款处理器的具体信息，往往需要同时翻阅多份文档。以 Neoverse V3 为例：

![图 7-1：架构文档](../../asset/figure7-1-architecture-documentation.png)

*图 7-1：架构文档*

《Arm® Neoverse™ V3 Core Technical Reference Manual》会说明该处理器实现了 Armv9.2-A，以及 RME 等可选特性。但具体细节——包括软件如何检测某特性是否启用——仍要查面向 A-profile 的《Arm Architecture Reference Manual》。TRM 还会给出其他微架构信息，例如缓存的数量和大小。

如果你正基于 Neoverse V3 及配套 Arm IP 设计 SoC，还需要注意：

- TRM 描述可用的硬件接口，如 GICv4.1 CPU 接口和 AMBA CHI Issue E；你需要连接符合对应架构规格的 IP 模块。
- 某些可选特性（如 RME）依赖内核之外组件的能力。要使用 RME，必须确保系统满足 RME 系统架构规格说明的要求；否则应查阅《Arm® Neoverse™ V3 Core Configuration and Integration Manual》，确认 RME 已被禁用。
- 如果要支持标准现成操作系统镜像，可能需要确保整体硬件设计符合基础系统架构（BSA），并按需符合其面向特定市场的补充；同时还要确保 SoC 随附固件符合基础启动要求（BBR）。

如果你用的是现成 SoC，还会用到 SoC 厂商的文档，通常称为**数据手册（datasheet）**，其中给出该 SoC 特有的信息。

### 7.4 哪类文档里能找到什么

**表 7-1：不同 Arm 文档类型的示例内容**

| 特性 | Arm Architecture Reference Manual | GIC 规格说明 | AMBA 规格说明 | TRM | CIM | SoC 数据手册 |
| --- | --- | --- | --- | --- | --- | --- |
| 指令集 | ✓ | — | — | — | — | — |
| 指令周期时序 | — | — | — | ✓ 或单独文档 | — | — |
| 架构寄存器 | ✓ | ✓ | — | — | — | — |
| 处理器特定寄存器 | — | — | — | ✓ | — | — |
| 内存模型 | ✓ | — | — | — | — | — |
| 异常模型 | ✓ | — | — | — | — | — |
| 对可选特性的支持 | — | — | — | ✓ | ✓（部分可能为综合选项） | — |
| 缓存与 TLB 的大小 | — | — | — | ✓ | ✓（部分可能为综合选项） | — |
| 电源管理 | — | — | — | ✓ | — | — |
| 总线端口 | — | — | — | ✓ | ✓ | — |
| 总线事务描述 | — | — | ✓ | — | — | — |
| 处理器产生的总线事务 | — | — | — | ✓ | — | — |
| 内存映射 | — | — | — | — | — | ✓ |
| 外设 | — | — | — | — | — | ✓ |
| SoC 引脚定义 | — | — | — | — | — | ✓ |

### 7.5 参考手册不是教程

Arm Architecture Reference Manual、TRM、CIM 都属于**参考手册**，它们不负责教你“怎么用处理器”。例如，Arm ARM 里不会有一章叫“如何开启 MMU”。

这种分工是有意为之：架构要求的技术细节放在参考手册里；更通用的指导则放在其他文档中，比如本指南。通用指导又分两类，一类讲概念，一类给出可照做的步骤。

## 8. 常见架构术语

Arm 文档中的术语（有时大写、有时小写）往往有非常具体的含义，每一个都在 Arm ARM 中有定义。下面挑出程序员最常遇到的几个。

### 8.1 PE（Processing Element，处理单元）

PE 是对“Arm 架构实现”的通用称呼。更直白地说：任何拥有自己的程序计数器（PC）、能够独立执行程序的实体，都可以看作一个 PE。

面向 A-profile 的 Arm ARM 是这样描述的：

> “决定 PE 如何运行的状态，包括当前异常级别和安全状态，以及在 AArch32 状态下的 PE 模式（PE mode）。”

之所以用 PE 这种抽象说法，是因为底层微架构可以千差万别：

- Cortex-A8 是单核单线程，整个处理器就是一个 PE；
- Cortex-A53 是多核、每核单线程，每个核是一个 PE；
- Cortex-A65AE 是多核、每核双线程，每个线程是一个 PE。

用 PE 这个词，架构就能与具体处理器中的设计选择解耦。

### 8.2 IMPLEMENTATION DEFINED（实现定义）

标记为 IMPLEMENTATION DEFINED（IMP DEF）的特性，由具体微架构决定。实现必须给出一致的行为或取值。

例如，缓存大小就是 IMP DEF：架构规定了软件查询缓存大小的机制，但具体多大由设计者决定。

再如，密码学指令是否支持也是 IMP DEF：软件可以通过相应寄存器判断这些指令是否存在。

这两个例子的共同点是——选择是**静态**的。对某颗处理器来说，特性要么支持、要么不支持，不会在运行时变化。

对 Cortex 和 Neoverse 处理器而言，有些 IMP DEF 选项是固定的，有些是综合（synthesis）选项。以 Cortex-A57 为例，L1 缓存大小固定，L2 缓存大小属于综合选项；但无论哪种，决策都在设计阶段完成，运行时依然静态。

具体处理器的 TRM 会给出 IMP DEF 选项的完整信息。

### 8.3 UNPREDICTABLE 与 CONSTRAINED UNPREDICTABLE

这两个词描述的是软件**不应该**做的事。一旦行为被标为 UNPREDICTABLE 或 CONSTRAINED UNPREDICTABLE，软件就不能依赖处理器的具体表现；如果反复执行错误动作，处理器甚至可能表现出不同的行为。

例如，提供未对齐的转换表属于 CONSTRAINED UNPREDICTABLE。这属于“不良软件”——违反了转换表必须遵守的架构规则。

与 IMP DEF 不同，TRM 通常不会描述所有 UNPREDICTABLE 行为。

### 8.4 Deprecated（已弃用）

Arm 有时会从架构中移除某个特性，原因可能是它已不常用、不再必要。但遗留软件可能仍依赖它，所以在彻底移除之前，会先标记为 DEPRECATED。

DEPRECATED 相当于给开发者的预警：该特性未来会被移除，建议尽快从代码中去除。

通常还会同时加入一个控制位，允许禁用该特性。开发者可以借助它测试遗留代码是否还在使用该特性。

### 8.5 RES0 与 RES1

- **RES0**：Reserved, should be Zero，保留，应为 0；
- **RES1**：Reserved, should be One，保留，应为 1。

它们用于描述未被使用、对处理器没有功能影响的字段。

保留可能是**有条件**的：例如某字段只有在对应特性被软件禁用或未实现时才是 RES0。反过来，当前为 RESx 的字段，也可能在架构的未来版本中被赋予明确用途。

还要注意：RES0 字段读出来不一定总是 0，RES1 字段读出来也不一定总是 1。它们只说明“该字段未被使用”。

有时 RES0/RES1 字段必须是有状态（stateful）的，也就是读回最后写入的值。

### 8.6 Future Architecture Technologies（未来架构技术）

Arm 有时会提前公布尚未正式发布的架构特性，目的是支持软件和工具尽早开发。这类发布标记为 Future Architecture Technologies（FAT）。

## 9. 知识自测

**如果文档中看到 Armv7-R，它指哪个版本、哪个配置文件？**

第 7 版，R-Profile。

**64 位支持是在哪个版本的 A-Profile 中引入的？**

第 8 版，Armv8-A。

**下面三项分别属于架构还是微架构：指令编码、缓存大小、内存排序？**

- 指令编码：架构；
- 缓存大小：微架构；
- 内存排序：架构。

**什么是 PE？**

PE 即 Processing Element（处理单元），指实现 Arm 架构的机器实体。

## 10. 延伸资料

- Learn the Architecture Guides
- Arm Architecture Reference Manual for A-profile architecture
- All Arm Architecture Reference Manuals
- Learn the architecture - Arm System Architectures
- Arm Community：提出开发问题，查找 Arm 专家针对特定主题的文章与博客

## 11. 小结与下一步

本指南回答了四个基础问题：

- Arm 架构是什么：一套功能规格说明，是软硬件之间的契约；
- 它如何划分：A/R/M 三种配置文件，共享基础特性，各自面向不同场景；
- 它如何演进：Armv8-A 与 Armv9-A 并行更新，用次版本号标识年度更新，用 FEAT 标识具体特性；
- 它如何落地：架构与微架构分工明确，系统架构补足处理器之外的组件与固件接口。

此外，你还了解了 Arm 文档体系的分工——Arm ARM、TRM、CIM、系统架构规格、SoC 数据手册各自回答什么问题，以及 PE、IMP DEF、UNPREDICTABLE、DEPRECATED、RES0/RES1、FAT 等关键术语。

这些概念是后续学习 Arm 技术的基础。本系列的其他指南会继续展开架构的各个方面，并配以示例和注释。
