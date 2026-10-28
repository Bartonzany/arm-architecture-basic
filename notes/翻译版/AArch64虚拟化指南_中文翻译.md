# Learn the architecture — AArch64 虚拟化指南（中文翻译）

> 原文：*Learn the architecture - AArch64 virtualization Guide*
>
> 版本：Version 1.0　|　文档 ID：102142_0100_06_en　|　Issue 06（2025-07-24）
>
> 版权 © 2019, 2022, 2024–2025 Arm Limited（或其关联公司）。保留所有权利。非保密。

---

## 1. 概述

本指南描述 Armv8-A AArch64 中的虚拟化支持。涵盖的主题包括第二阶段（stage 2）翻译、虚拟异常和捕获（trapping）。

本指南包含一些作为引言的基础虚拟化理论，并给出了一些虚拟机监控器（hypervisor）如何使用它所描述特性的示例。它不涵盖某个特定 hypervisor 的操作，也不试图解释如何从头编写你自己的 hypervisor。这两个主题都超出了本指南的范围。

在本指南末尾，你可以进行“知识自测”。你将学习两种类型的 hypervisor，以及它们如何映射到 Arm 异常级别。你将能够解释陷阱（trap）的操作，以及它们如何被用于模拟操作。你还将能够列出 hypervisor 可以生成哪些虚拟异常，并描述实现这一点的机制。

**开始之前**

我们假设你对虚拟化有基本的理解，包括什么是虚拟机（VM）以及 hypervisor 的角色。请熟悉“异常模型”和“内存管理”中的地址翻译。

## 2. 虚拟化简介

在这里，我们将介绍一些入门级的 hypervisor 和虚拟化理论。如果你已经熟悉这些概念，可能想跳过这部分内容。

在本指南中，我们使用术语 hypervisor 来表示负责创建、管理和调度虚拟机（VM）的软件。

**为什么虚拟化很重要**

虚拟化是一种被广泛使用的技术，支撑着几乎所有现代云计算和企业基础设施。开发者使用虚拟化在单台机器上运行多个操作系统（OS），并在不损坏主计算环境风险的情况下测试软件。

虚拟化在服务器系统中很受欢迎，对虚拟化的支持是大多数服务器级处理器的要求。这是因为虚拟化为数据中心提供了非常理想的特性的，包括：

- **隔离（Isolation）**：虚拟化的核心是在单个物理系统上运行的虚拟机之间提供隔离。这种隔离允许相互不信任的计算环境共享一个物理系统。例如，两个竞争对手可以共享数据中心中的同一台物理机器，而无法访问彼此的数据。
- **高可用性（High Availability）**：虚拟化允许工作负载在物理机器之间进行无缝且透明的迁移。这种技术通常用于将工作负载从可能需要维护和更换的故障硬件平台上迁移走。
- **工作负载均衡（Workload balancing）**：为了优化数据中心的硬件和功耗预算，尽可能充分地使用每个硬件平台是很重要的。同样，这可以通过迁移虚拟机、或在物理机器上共同托管合适的工作负载来实现。这意味着物理机器被尽可能地利用其全部容量。这为数据中心提供商提供了最佳的功耗预算，并为租户提供了最佳的性能。
- **沙箱（Sandboxing）**：VM 可以用于为可能干扰其运行所在机器其余部分的应用程序提供沙箱。此类应用程序的例子包括遗留应用程序，或正在开发中的软件。在 VM 中运行这些应用程序可以防止应用程序中的缺陷或恶意部分干扰物理机器上的其他应用程序或数据。

**独立式和托管式 hypervisor**

Hypervisor 可以分为两大类：独立式（standalone，或称 Type 1）hypervisor 和托管式（hosted，或称 Type 2）hypervisor。

我们首先看托管式（Type 2）hypervisor。在 Type 2 hypervisor 配置中，宿主操作系统（Host OS）完全控制硬件平台及其所有资源，包括 CPU 和物理内存。下图说明了一个托管式（Type 2）hypervisor。

如果你以前使用过 Virtual Box 或 VMware Workstation 之类的软件，那么你所运行的就是这种类型的 hypervisor。一个被称为宿主操作系统（Host OS）的操作系统被安装在平台上，hypervisor 运行在 Host OS 之中，利用现有功能来管理硬件。然后 hypervisor 可以托管虚拟机，这些虚拟机本身运行一个 OS。我们将其称为客户操作系统（Guest OS）。

接下来，我们看独立式（Type 1）hypervisor：

![图 2-1：独立式或 Type 1 hypervisor](../../asset/figure2-1-standalone-type1-hypervisor.png)

*图 2-1：独立式或 Type 1 hypervisor*

你可以看到，在这种 hypervisor 设计中没有 Host OS。Hypervisor 直接运行在硬件上，完全控制硬件平台及其所有资源，包括 CPU 和物理内存。与托管式 hypervisor 一样，独立式 hypervisor 可以托管虚拟机。这些虚拟机可以运行一个或多个完整的 Guest OS。

在 Arm 平台上最常用的两个开源 hypervisor 是 Xen（独立式，Type 1）和 KVM（托管式，Type 2）。我们将使用这些 hypervisor 来说明本指南中的一些要点。不过，还有许多其他 hypervisor 可用，包括开源和专有的。

**全虚拟化与半虚拟化**

VM 的经典定义是一个独立的、隔离的计算环境，与真实的物理机器无法区分。虽然在基于 Arm 的系统上完全模拟真实机器是可能的，但这通常不是一种高效的做法。因此，这种模拟并不常做。例如，模拟一个真实的以太网设备很慢，因为 Guest OS 对模拟寄存器的每一次访问都必须由 hypervisor 在软件中处理。这种处理可能比访问物理设备上的寄存器昂贵得多。

一种通常用于提高性能的优选替代方案是“点醒（enlighten）”Guest OS。通过让 Guest OS 意识到它运行在 VM 中，并提供专为在 hypervisor 中高效模拟、并由 Guest OS 访问而设计的虚拟设备，Guest OS 即使对于 I/O 也能获得良好的性能。

严格来说，全系统虚拟化模拟一台真实的物理机器。另一方面，Xen（开源项目）普及了“半虚拟化（paravirtualization）”这一术语，在该方案中，Guest OS 的核心部分被修改为在虚拟硬件平台上运行，而不是物理机器。进行这种修改是为了提高性能。

如今，在大多数具有硬件虚拟化支持的架构上（包括 Arm），Guest OS 大多未经修改地运行。Guest OS 认为它运行在真实硬件上，除了块存储和网络等 I/O 外设的驱动程序使用半虚拟化设备和设备驱动。此类半虚拟化 I/O 设备的例子是 Virtio 和 Xen PV Bus。

**虚拟机与虚拟 CPU**

理解虚拟机（VM）和虚拟 CPU（vCPU）之间的区别很重要。一个 VM 将包含一个或多个 vCPU，如下图所示：

![图 2-2：虚拟机与虚拟 CPU](../../asset/figure2-2-vm-and-vcpus.png)

*图 2-2：虚拟机与虚拟 CPU*

当我们查看本指南中其他一些主题时，VM 和 vCPU 之间的区别将变得重要。例如，一页内存可能被分配给一个 VM，因此该 VM 中的所有 vCPU 都可以访问。然而，虚拟中断是面向特定 vCPU 的，只能到达该 vCPU。

> 严格来说，我们应该称其为虚拟处理单元（virtual Processing Element，vPE），而不是 vCPU。请记住，处理单元（PE）是实现 Arm 架构的机器的通用术语。本指南使用 vCPU 而不是 vPE，因为 vCPU 是大多数人熟悉的术语。不过，在架构规格说明中，使用的是术语 vPE。

## 3. AArch64 中的虚拟化

运行在 EL2 或更高层的软件可以访问若干用于虚拟化的控制：

- 第二阶段（stage 2）翻译
- EL1/0 指令和寄存器访问捕获
- 虚拟异常生成

非安全和安全状态下的异常级别（EL）如下所示：

![图 3-1：AArch64 中的虚拟化](../../asset/figure3-1-virtualization-in-aarch64.png)

*图 3-1：AArch64 中的虚拟化*

在图中，安全 EL2 以灰色显示。这是因为安全状态下的 EL2 支持并非总是可用。这在“安全虚拟化”一节中讨论。

- 安全虚拟化
- 托管式（Type 2）hypervisor
- 嵌套虚拟化

## 4. 第二阶段翻译

本章介绍第二阶段（stage 2）翻译以及控制内存访问的方法。

**什么是第二阶段翻译**

Stage 2 翻译允许 hypervisor 控制虚拟机（VM）中的内存视图。具体来说，它允许 hypervisor 控制 VM 可以访问哪些内存映射的系统资源，以及这些资源出现在 VM 地址空间中的位置。

这种控制内存访问的能力对于隔离和沙箱很重要。Stage 2 翻译可以用于确保 VM 只能看到分配给它的资源，而不能看到分配给其他 VM 或 hypervisor 的资源。

对于内存地址翻译来说，stage 2 翻译是第二阶段的翻译。为了支持这一点，需要一组新的翻译表，称为 Stage 2 表，如下所示：

![图 4-1：VA 到 IPA 到 PA 的地址翻译](../../asset/figure4-1-va-ipa-pa-translation.png)

*图 4-1：VA 到 IPA 到 PA 的地址翻译*

操作系统（OS）控制一组翻译表，从虚拟地址空间映射到它所认为的物理地址空间。然而，这个过程会进行第二次翻译，进入真正的物理地址空间。这第二阶段由 hypervisor 控制。OS 控制的翻译被称为 stage 1 翻译，hypervisor 控制的翻译被称为 stage 2 翻译。OS 所认为的物理内存的地址空间被称为中间物理地址（Intermediate Physical Address，IPA）空间。

> 有关地址翻译如何工作的介绍，请参见我们的“内存管理”指南。

用于 stage 2 的翻译表格式与 stage 1 使用的非常相似。然而，有些属性在 stage 2 中的处理方式不同，并且类型（Normal 或 Device）直接编码到表项中，而不是通过 MAIR_ELx 寄存器。

**VMID**

每个 VM 被分配一个虚拟机标识符（VMID）。VMID 用于标记翻译后备缓冲器（TLB）表项，以识别每个表项属于哪个 VM。这种标记允许多个不同 VM 的翻译同时存在于 TLB 中。

VMID 存储在 VTTBR_EL2 中，可以是 8 位或 16 位。VMID 由 VTCR_EL2.VS 位控制。对 16 位 VMID 的支持是可选的，于 Armv8.1-A 中加入。

> EL2 和 EL3 翻译机制的翻译不用 VMID 标记，因为它们不经过 stage 2 翻译。

**VMID 与 ASID 的交互**

TLB 表项也可以用地址空间标识符（ASID）标记。一个应用程序由 OS 分配一个 ASID，该应用程序中的所有 TLB 表项都用该 ASID 标记。这意味着不同应用程序的 TLB 表项能够在 TLB 中共存，而不会出现一个应用程序使用属于另一个应用程序的 TLB 表项的可能性。

每个 VM 都有自己的 ASID 命名空间。例如，两个 VM 可能都使用 ASID 5，但它们将其用于不同的事情。重要的是 ASID 和 VMID 的组合。

**属性组合与覆盖**

Stage 1 和 stage 2 映射都包含属性，例如类型和访问权限。内存管理单元（MMU）将两个阶段的属性组合起来，得到一个最终的有效值。MMU 通过选择更严格的那个阶段来做到这一点，如下所示：

![图 4-2：组合 stage 1 和 stage 2 属性](../../asset/figure4-2-combining-attributes.png)

*图 4-2：组合 stage 1 和 stage 2 属性*

在这个例子中，Device 类型比 Normal 类型更严格。因此，结果的类型是 Device。如果我们反转这个例子，使 stage 1 = Normal、stage 2 = Device，结果将是相同的。

这种组合属性的方法适用于大多数使用场景，但有时 hypervisor 可能想要覆盖这种行为。例如，在 VM 的早期启动期间。对于这些情况，有一些控制位可以覆盖正常行为：

- `HCR_EL2.CD`：使所有 stage 1 属性变为非缓存（Non-cacheable）。
- `HCR_EL2.DC`：强制 stage 1 属性为 Normal、写回缓存（Write-Back Cacheable）。
- `HCR_EL2.FWB`：允许 stage 2 覆盖 stage 1 属性，而不是常规的属性组合。

> `HCR_EL2.FWB` 于 Armv8.4-A 引入。

**模拟内存映射输入/输出（MMIO）**

与物理机器上的物理地址空间一样，VM 中的 IPA 空间包含用于访问内存和外设的区域，如下所示：

![图 4-3：VM 的 IPA 空间框图](../../asset/figure4-3-ipa-space-of-vm.png)

*图 4-3：VM 的 IPA 空间框图*

VM 可以使用外设区域来访问真实物理外设（通常称为直接分配的外设）和虚拟外设。

虚拟外设完全由 hypervisor 在软件中模拟，如下图所示：

![图 4-4：虚拟外设和分配外设的 stage 2 映射](../../asset/figure4-4-stage2-virtual-assigned.png)

*图 4-4：虚拟外设和分配外设的 stage 2 映射*

分配的外设（assigned peripheral）是被分配给 VM、并映射到其 IPA 空间的真实物理设备。这允许 VM 内运行的软件直接与外设交互。

虚拟外设（virtual peripheral）是 hypervisor 将在软件中模拟的外设。相应的 stage 2 表项会被标记为故障（fault）。VM 中的软件认为它可以直接与外设通信，但每次访问都会触发 stage 2 故障，由 hypervisor 在异常处理器中模拟外设访问。

为了模拟外设，hypervisor 不仅需要知道访问了哪个外设，还需要知道访问了该外设中的哪个寄存器、访问是读还是写、访问的大小，以及用于传输数据的寄存器。

“异常模型”指南介绍了 FAR_ELx 寄存器。处理 stage 1 故障时，这些寄存器报告触发异常的虚拟地址。虚拟地址对 hypervisor 没有帮助，因为 hypervisor 通常不知道 Guest OS 如何配置了它的虚拟地址空间。对于 stage 2 故障，有一个额外的寄存器 HPFAR_EL2，它报告中止地址的 IPA。因为 IPA 空间由 hypervisor 控制，它可以使用此信息来确定需要模拟的寄存器。

对于触发 stage 2 故障的单通用寄存器加载或存储，会提供额外的综合（syndrome）信息。此信息包括访问的大小和源或目标寄存器，允许 hypervisor 确定正在对虚拟外设进行的访问类型。

下图说明了捕获然后模拟访问的过程：

![图 4-5：模拟 MMIO 访问的示例](../../asset/figure4-5-emulating-mmio-access.png)

*图 4-5：模拟 MMIO 访问的示例*

该过程通过这些步骤描述：

1. VM 中的软件尝试访问虚拟外设。在这个例子中，是虚拟 UART 的接收 FIFO。
2. 该访问在 stage 2 翻译处被阻止，导致一个中止（abort）被路由到 EL2。
   - 该中止用异常信息填充 ESR_EL2，包括访问的字节数、目标寄存器以及它是加载还是存储。
   - 该中止还用中止访问的 IPA 填充 HPFAR_EL2。
3. Hypervisor 使用来自 ESR_EL2 和 HPFAR_EL2 的信息来识别所访问的虚拟外设寄存器。此信息允许 hypervisor 模拟该操作。然后它通过 ERET 返回 vCPU。
   - 执行在 LDR 之后的指令处重新开始。

**系统内存管理单元（SMMU）**

到目前为止，我们考虑了来自处理器的不同类型的访问。系统中的其他主机（master），例如 DMA 控制器，可能被分配供 VM 使用。我们需要某种方法将 stage 2 保护也扩展到这些主机。

考虑一个使用不使用虚拟化的 DMA 控制器的系统，如下图所示：

![图 4-6：不使用虚拟化的 DMA 控制器](../../asset/figure4-6-dma-without-virtualization.png)

*图 4-6：不使用虚拟化的 DMA 控制器*

DMA 控制器将通过驱动程序进行编程，通常在内核空间。该内核空间驱动程序可以确保 OS 级别的内存保护不被破坏。这意味着一个应用程序不能使用 DMA 来访问它本不应看到的内存。

让我们考虑相同的系统，但 OS 在 VM 内运行，如下图所示：

![图 4-7：OS 在 VM 内运行的框图](../../asset/figure4-7-os-in-vm.png)

*图 4-7：OS 在 VM 内运行的框图*

在这个系统中，hypervisor 使用 stage 2 来提供 VM 之间的隔离。软件查看内存的能力受到 hypervisor 控制的 stage 2 表的限制。

允许 VM 中的驱动程序直接与 DMA 控制器交互会产生两个问题：

- **隔离**：DMA 控制器不经过 stage 2 表，可能被用来破坏 VM 的沙箱。
- **地址空间**：有了两阶段翻译，内核所认为的 PA 其实是 IPA。DMA 控制器看到的仍然是 PA，因此内核和 DMA 控制器对内存有不同的视图。为了克服这个问题，hypervisor 可以捕获 VM 与 DMA 控制器之间的每次交互，提供必要的翻译。当内存碎片化时，这个过程是低效且有问题的。

捕获和模拟驱动程序访问的替代方案是将 stage 2 机制也扩展到覆盖其他主机，例如我们的 DMA 控制器。当这发生时，那些主机也需要一个 MMU。这被称为系统内存管理单元（SMMU，有时也称为 IOMMU）：

![图 4-8：系统内存管理单元](../../asset/figure4-8-smmu.png)

*图 4-8：系统内存管理单元*

Hypervisor 将负责编程 SMMU，使上游主机（在我们的例子中是 DMA）看到与分配给它的 VM 相同的内存视图。

这个过程解决了我们确定的两个问题。SMMU 可以强制执行 VM 之间的隔离，确保外部主机不能用于破坏沙箱。SMMU 还为 VM 中的软件和分配给 VM 的外部主机提供了一致的内存视图。

虚拟化并不是 SMMU 的唯一用例。还有许多其他用例不在本指南的范围内。

## 5. 指令的捕获与模拟

有时 hypervisor 需要模拟虚拟机（VM）内的操作。例如，VM 内的软件可能尝试配置与电源管理或缓存一致性相关的低层处理器控制。通常，你不想让 VM 直接访问这些控制，因为它们可能被用来破坏隔离，或影响系统中的其他 VM。

陷阱（trap）在给定动作（例如读取寄存器）被执行时引起异常。Hypervisor 需要能够捕获 VM 中的操作（例如配置低层控制的操作）并模拟它们，而不影响其他 VM。

架构包含陷阱控制，供你捕获 VM 中的操作并模拟它们。当设置了陷阱时，执行某个通常会被允许的特定动作会导致进入更高异常级别的异常。Hypervisor 可以使用这些陷阱来模拟 VM 内的操作。

例如，执行等待中断（Wait For Interrupt，WFI）指令通常会使 CPU 进入低功耗状态。通过断言 TWI 位（即 HCR_EL2.TWI==1），在 EL0 或 EL1 执行 WFI 将改为导致进入 EL2 的异常。

> 陷阱不仅仅用于虚拟化。也有 EL3 和 EL1 控制的陷阱。然而，陷阱对虚拟化软件特别有用。本指南只讨论通常与虚拟化相关的陷阱。

在我们的 WFI 示例中，OS 通常会作为空闲循环的一部分执行 WFI。对于 VM 内的 Guest OS，hypervisor 可以捕获此操作并改而调度不同的 vCPU，如下图所示：

![图 5-1：从 EL1 到 EL2 捕获 WFI 的示例](../../asset/figure5-1-trapping-wfi.png)

*图 5-1：从 EL1 到 EL2 捕获 WFI 的示例*

**呈现寄存器的虚拟值**

使用陷阱的另一个例子是呈现寄存器的虚拟值。例如，ID_AA64MMFR0_EL1 报告处理器中内存系统相关特性的支持情况。OS 可能在启动时读取此寄存器，以确定启用内核中的哪些特性。Hypervisor 可能想要向 Guest OS 呈现一个不同的值，称为虚拟值。

为此，hypervisor 启用了覆盖该寄存器读取的陷阱。在陷阱异常时，hypervisor 确定触发了哪个陷阱，然后模拟该操作。在这个例子中，hypervisor 用 ID_AA64MMFR0_EL1 的虚拟值填充目标寄存器，如下所示：

![图 5-2：捕获并模拟操作的示例](../../asset/figure5-2-trapping-emulating.png)

*图 5-2：捕获并模拟操作的示例*

陷阱也可以用作惰性上下文切换的一部分。例如，OS 通常会在启动期间初始化内存管理单元（MMU）配置寄存器（TTBR\<n\>_EL1、TCR_EL1 和 MAIR_EL1），然后不再重新编程它们。Hypervisor 可以利用这一点来优化其上下文切换例程，只需在上下文切换时恢复寄存器，而无需保存它们。

然而，OS 可能会做一些不寻常的事情，在启动后重新编程寄存器。为了避免这引起任何问题，hypervisor 可以设置 HCR_EL2.TVM 陷阱。此设置会导致对 MMU 相关寄存器的任何写入生成一个进入 EL2 的陷阱，从而允许 hypervisor 检测它是否需要更新这些寄存器的已保存副本。

> 架构使用术语“捕获（trapping）”和“路由（routing）”来表示独立但相关的概念。回顾一下，陷阱在给定动作（例如读取寄存器）被执行时引起异常。路由指的是异常一旦生成后，被送入哪个异常级别。

**MIDR 和 MPIDR**

使用陷阱来虚拟化一个操作需要大量的计算。该操作生成一个进入 EL2 的陷阱异常，hypervisor 确定所需操作，模拟它，然后返回客户机。诸如 ID_AA64MMFR0_EL1 之类的特性寄存器，操作系统并不经常访问。这意味着在捕获对这些寄存器的访问到 hypervisor 中以模拟读取时，计算是可以接受的。

对于访问更频繁、或在性能关键代码中的寄存器，你想要避免这种计算负载。这些寄存器及其值的例子包括：

- `MIDR_EL1`：处理器类型，例如 Cortex-A53
- `MPIDR_EL1`：亲和性，例如处理器 2 的内核 1

Hypervisor 可能希望 Guest OS 看到这些寄存器的虚拟值，而无需捕获每一次单独访问。对于这些寄存器，架构提供了一种替代陷阱的方案：

- `VPIDR_EL2`：这是为 EL1 读取 MIDR_EL1 返回的值。
- `VMPIDR_EL2`：这是为 EL1 读取 MPIDR_EL1 返回的值。

Hypervisor 可以在进入 VM 之前设置这些寄存器。如果 VM 内运行的软件读取 MIDR_EL1 或 MPIDR_EL1，硬件将自动返回虚拟值，而无需陷阱。

> VMPIDR_EL2 和 VPIDR_EL2 没有定义的复位值。它们必须在首次进入 EL1 之前由启动代码初始化。这在裸机环境中尤其重要。

## 6. 异常虚拟化

中断被系统中的硬件用来向软件发出事件信号。例如，GPU 可能发送一个中断来表示它已经完成了一帧的渲染。

使用虚拟化的系统更复杂。有些中断可能由 hypervisor 本身处理。其他中断可能来自分配给虚拟机（VM）的设备，需要由该 VM 内的软件处理。另外，中断所面向的 VM 在收到中断时可能并未运行。

这意味着你需要机制来支持在 EL2 由 hypervisor 处理某些中断。你还需要将其他中断转发到特定 VM 或 VM 中特定虚拟 CPU（vCPU）的机制。

为了支持这些机制，架构包含了对虚拟中断的支持：vIRQ、vFIQ 和 vSError。这些虚拟中断的行为与其物理对应物（IRQ、FIQ 和 SError）类似，但只能在 EL0 和 EL1 执行时发出信号。在 EL2 或 EL3 执行时不可能收到虚拟中断。

> 回顾一下，安全状态下的虚拟化支持是在 Armv8.4-A 中引入的。要在安全 EL0/1 发出虚拟中断信号，需要支持并启用安全 EL2。否则，虚拟中断不会在安全状态下发出信号。

**启用虚拟中断**

要向 EL0/1 发出虚拟中断信号，hypervisor 必须设置 HCR_EL2 中相应的路由位。例如，要启用 vIRQ 信号，hypervisor 必须设置 HCR_EL2.IMO。此设置将物理 IRQ 异常路由到 EL2，并使能向 EL1 发出虚拟异常信号。

虚拟中断按中断类型分别控制。理论上，VM 可以被配置为接收物理 FIQ 和虚拟 IRQ。在实践中，这很不寻常。VM 通常只被配置为接收虚拟中断。

**生成虚拟中断**

有两种生成虚拟中断的机制：

1. 由内核在内部使用 HCR_EL2 中的控制生成。
2. 使用 GICv2 或更高版本的中断控制器。

让我们从机制 1 开始。HCR_EL2 中有三个位控制虚拟中断的生成：

- VI = 设置此位即注册一个 vIRQ。
- VF = 设置此位即注册一个 vFIQ。
- VSE = 设置此位即注册一个 vSError。

设置其中一个位等效于中断控制器向 vCPU 断言一个中断信号。生成的虚拟中断与常规中断一样受 PSTATE 屏蔽的影响。

这种机制使用简单，但缺点是它只提供了一种生成中断本身的方法。然后 hypervisor 需要在 VM 中模拟中断控制器的操作。回顾一下，在软件中捕获和模拟操作涉及开销，对于中断这样的频繁操作最好避免。

第二种选择是使用 Arm 的通用中断控制器（GIC）来生成虚拟中断。从 Arm GICv2 开始，GIC 可以通过提供物理 CPU 接口和虚拟 CPU 接口来发出物理和虚拟中断信号，如下图所示：

![图 6-1：GIC 的虚拟和物理 CPU 接口](../../asset/figure6-1-gic-interfaces.png)

*图 6-1：GIC 的虚拟和物理 CPU 接口*

这两个接口是相同的，除了一个发出物理中断信号，另一个发出虚拟中断信号。Hypervisor 可以将虚拟 CPU 接口映射到 VM 中，允许该 VM 中的软件直接与 GIC 通信。这种方法的优点是 hypervisor 只需要设置虚拟接口，而不需要模拟它。

这种方法减少了执行需要被捕获到 EL2 的次数，因此减少了虚拟化中断的开销。

> 虽然 Arm GICv2 可以用于 Armv8-A 设计，但更常见的是使用 GICv3 或 GICv4。

**将中断转发到 vCPU 的示例**

到目前为止，我们已经了解了虚拟中断是如何被启用和生成的。让我们看一个展示虚拟中断被转发到 vCPU 的示例。在这个例子中，我们将考虑一个已分配给 VM 的物理外设，如下图所示：

![图 6-2：转发虚拟中断的示例序列](../../asset/figure6-2-forwarding-virtual-interrupt.png)

*图 6-2：转发虚拟中断的示例序列*

该图说明了这些步骤：

1. 物理外设向 GIC 断言它的中断信号。
2. GIC 生成一个物理中断异常，IRQ 或 FIQ，通过 HCR_EL2.IMO/FMO 的配置被路由到 EL2。Hypervisor 识别外设，并确定它已被分配给一个 VM。它检查该中断应转发到哪个 vCPU。
3. Hypervisor 配置 GIC，将物理中断作为虚拟中断转发到 vCPU。然后 GIC 将断言 vIRQ 或 vFIQ 信号，但处理器在 EL2 执行时会忽略此信号。
4. Hypervisor 将控制返回给 vCPU。
5. 现在处理器处于 vCPU 中（EL0 或 EL1），来自 GIC 的虚拟中断可以被送入。该虚拟中断受 PSTATE 异常屏蔽的影响。

该示例展示了一个物理中断被转发为虚拟中断。该示例与“第二阶段翻译”一节中描述的分配外设模型相匹配。对于虚拟外设，hypervisor 可以创建虚拟中断，而无需将其链接到物理中断。

**中断屏蔽与虚拟中断**

在“AArch64 异常模型”指南中，我们介绍了 PSTATE 中的中断屏蔽位：PSTATE.I 用于 IRQ，PSTATE.F 用于 FIQ，PSTATE.A 用于 SError。在虚拟化环境中运行时，这些屏蔽的工作方式略有不同。

例如，对于 IRQ，我们已经看到设置 HCR_EL2.IMO 会做两件事：

- 将物理 IRQ 路由到 EL2
- 使能在 EL0 和 EL1 发出 vIRQ 信号

此设置还改变了 PSTATE.I 屏蔽的应用方式。在 EL0 和 EL1 中，如果 HCR_EL2.IMO==1，PSTATE.I 作用于 vIRQ 而不是 pIRQ。

## 7. 通用定时器虚拟化

Arm 架构包含通用定时器（Generic Timer），这是每个处理器中可用的一组标准化定时器。通用定时器由一组比较器组成，它们与一个公共的系统计数（system count）进行比较。当比较器的值等于或小于系统计数时，比较器生成一个中断。在下图中，我们可以看到系统中的通用定时器（橙色），及其比较器和计数器模块的组成部分。

![图 7-1：系统计数器模块和每个核的比较器](../../asset/figure7-1-system-counter-comparators.png)

*图 7-1：系统计数器模块和每个核的比较器*

下图显示了一个示例系统，其中一个 hypervisor 托管两个虚拟 CPU（vCPU）：

![图 7-2：托管两个 vCPU 的 hypervisor 系统](../../asset/figure7-2-two-vcpus.png)

*图 7-2：托管两个 vCPU 的 hypervisor 系统*

> 在这个例子中，我们忽略了运行 hypervisor 在 vCPU 之间进行上下文切换的开销。

经过 4ms 的物理时间（或墙上时钟时间 wall-clock time），每个 vCPU 已运行了 2ms。如果 vCPU0 在 T=0 时设置它的比较器，在 3ms 后生成中断，你期望中断已经触发了吗？

或者，你是想在 2ms 的虚拟时间（vCPU 所经历的时间）之后中断，还是在 2ms 的墙上时钟时间之后中断？

Arm 架构提供了做到两者的能力，取决于虚拟化用于什么。让我们看看它是如何做到的。

在 vCPU 上运行的软件可以访问两个定时器：

- EL1 物理定时器（EL1 Physical Timer）
- EL1 虚拟定时器（EL1 Virtual Timer）

EL1 物理定时器与系统计数器模块生成的计数进行比较。使用这个定时器得到的是墙上时钟时间。

EL1 虚拟定时器与一个虚拟计数（virtual count）进行比较。虚拟计数是物理计数减去一个偏移。Hypervisor 在寄存器中为当前调度的 vCPU 指定偏移。这允许它隐藏 vCPU 未被调度运行期间的时间流逝：

![图 7-3：虚拟计数器是如何生成的](../../asset/figure7-3-virtual-counter.png)

*图 7-3：虚拟计数器是如何生成的*

为了说明这个概念，我们可以扩展前面的示例，如下图所示：

![图 7-4：使用虚拟定时器的示例](../../asset/figure7-4-virtual-timer-example.png)

*图 7-4：使用虚拟定时器的示例*

在 6ms 的时间内，每个 vCPU 得以运行 3ms。Hypervisor 可以使用偏移寄存器来呈现一个只显示 vCPU 运行时间的虚拟计数。或者 hypervisor 可以将偏移保持为 0，这意味着虚拟时间与物理时间相同。

> 该示例将系统计数（System Count）的频率显示为 1ms。在实践中，这个频率值是非常不可能的。我们建议你将系统计数设置为使用 1MHz 到 50MHz 之间的频率。

## 8. 虚拟化宿主扩展

下图显示了我们之前在“异常虚拟化”一节中看到的软件栈和异常级别的简化版本：

![图 8-1：使用 Armv8-A 异常级别的独立式 hypervisor](../../asset/figure8-1-standalone-hypervisor-els.png)

*图 8-1：使用 Armv8-A 异常级别的独立式 hypervisor*

你可以看到独立式 hypervisor 如何映射到 Arm 异常级别。Hypervisor 运行在 EL2，虚拟机（VM）运行在 EL0/1。这种情况对于托管式 hypervisor 来说更有问题，如下图所示：

![图 8-2：VHE 之前的托管式 hypervisor](../../asset/figure8-2-hosted-hypervisor-pre-vhe.png)

*图 8-2：VHE 之前的托管式 hypervisor*

传统上，内核运行在 EL1，但虚拟化控制在 EL2。这意味着大部分 Host OS 位于 EL1，一些运行在 EL2 的桩代码（stub code）用于访问虚拟化控制。这种安排可能效率低下，因为它可能涉及额外的上下文切换。

内核需要处理在 EL1 和 EL2 运行之间的一些差异，但这些仅限于少数子系统，例如早期启动。

> DynamIQ 处理器（Cortex-A55、Cortex-A75 和 Cortex-A76）支持虚拟化宿主扩展（Virtualization Host Extensions，VHE）。

**在 EL2 运行 Host OS**

VHE 由 HCR_EL2 中的两个位控制。这些位可以总结为：

- `E2H`：控制是否启用 VHE。
- `TGE`：当启用 VHE 时，控制 EL0 是 Guest 还是 Host。

下表总结了典型设置：

| 执行位置 | E2H | TGE |
| --- | --- | --- |
| Guest 内核（EL1） | 1 | 0 |
| Guest 应用程序（EL0） | 1 | 0 |
| Host 内核（EL2） | 1 | 1* |
| Host 应用程序（EL0） | 1 | 1 |

> \* 在从 VM 退出到 hypervisor 的异常上，TGE 最初会是 0。软件必须在运行 Host 内核的主要部分之前设置该位。

你可以在下图中看到这些典型设置：

![图 8-3：E2H 和 TGE 的组合](../../asset/figure8-3-e2h-tge-combinations.png)

*图 8-3：E2H 和 TGE 的组合*

**虚拟地址空间**

下图显示了在引入 VHE 之前，EL0/EL1 的虚拟地址空间是什么样的：

![图 8-4：VHE 之前的 EL0/1 和 EL2 虚拟地址空间](../../asset/figure8-4-el01-el2-pre-vhe.png)

*图 8-4：VHE 之前的 EL0/1 和 EL2 虚拟地址空间*

正如“内存管理”中所讨论的，EL0/1 有两个区域。按照惯例，上部区域被称为内核空间，下部区域被称为用户空间。然而，EL2 在地址范围底部只有一个单一区域。这种差异是因为，传统上，hypervisor 不会托管应用程序。这意味着 hypervisor 不需要内核空间和用户空间之间的划分。

> 将内核空间分配到上部区域、用户空间分配到下部区域，只是一种惯例。它不是 Arm 架构强制规定的。

EL0/1 虚拟地址空间还支持地址空间标识符（ASID），但 EL2 不支持。这是因为 hypervisor 通常不会托管应用程序。

为了让我们的 Host OS 在 EL2 高效执行，我们需要添加第二个区域和 ASID 支持。设置 HCR_EL2.E2H 可以解决这些问题，如下图所示：

![图 8-5：E2H==1 时的 EL0/2 虚拟地址空间](../../asset/figure8-5-el02-when-e2h.png)

*图 8-5：E2H==1 时的 EL0/2 虚拟地址空间*

在 EL0 中，HCR_EL2.TGE 控制使用哪个虚拟地址空间：EL1 空间还是 EL2 空间。使用哪个空间取决于应用程序是在 Host OS（TGE==1）还是 Guest OS（TGE==0）下运行。

**重定向寄存器访问**

我们在“通用定时器虚拟化”一节中看到，启用 VHE 会改变 EL2 虚拟地址空间的布局。然而，我们在 MMU 配置方面仍然有一个问题。这是因为我们的内核会尝试访问 _EL1 寄存器，例如 TTBR0_EL1，而不是 TTBR0_EL2 之类的 _EL2 寄存器。

要在 EL2 运行相同的二进制文件，我们需要将访问从 EL1 寄存器重定向到 EL2 等价寄存器。设置 E2H 可以做到这一点，使对 _EL1 系统寄存器的访问被重定向到它们的 EL2 等价寄存器。这种重定向如下图所示：

![图 8-6：E2H 对 EL2 系统寄存器访问的影响](../../asset/figure8-6-e2h-system-registers.png)

*图 8-6：E2H 对 EL2 系统寄存器访问的影响*

然而，这种重定向给我们留下了一个新问题。Hypervisor 仍然需要访问真正的 _EL1 寄存器，以便它能够实现任务切换。为了解决这个问题，引入了一组新的寄存器别名，带 _EL12 或 _EL02 后缀。当在 EL2 使用且 E2H==1 时，这些别名可以访问 EL1 寄存器以进行上下文切换。你可以在下图中看到这一点：

![图 8-7：E2H==1 时从 EL2 访问 EL1 寄存器](../../asset/figure8-7-access-el1-from-el2.png)

*图 8-7：E2H==1 时从 EL2 访问 EL1 寄存器*

**异常**

通常，HCR_EL2.IMO/FMO/AMO 位控制物理异常是路由到 EL1 还是 EL2。当在 EL0 以 TGE==1 执行时，所有物理异常都路由到 EL2，除非它们被 SCR_EL3 路由到 EL3。无论 HCR_EL2 路由位的实际值如何，情况都是如此。这是因为应用程序是作为 Host OS 的子进程执行，而不是 Guest OS。因此，任何异常都应路由到运行在 EL2 的 Host OS。

## 9. 嵌套虚拟化

理论上，hypervisor 可以在虚拟机（VM）内运行。这个概念被称为嵌套虚拟化：

![图 9-1：嵌套虚拟化框图](../../asset/figure9-1-nested-virtualization.png)

*图 9-1：嵌套虚拟化框图*

我们将第一个 hypervisor 称为宿主 Hypervisor（Host Hypervisor），VM 内的 hypervisor 称为客户 Hypervisor（Guest Hypervisor）。

在 Armv8.3-A 发布之前，可以通过在 EL0 运行 Guest Hypervisor 来在 VM 中运行它。然而，这需要大量的软件模拟，实现复杂且性能不佳。借助 Armv8.3-A 中增加的特性，可以在 EL1 运行 Guest Hypervisor。借助 Armv8.4-A 中增加的特性，这一过程甚至更高效，尽管它仍然涉及 Host Hypervisor 中的额外智能。

**Guest Hypervisor 对虚拟化控制的访问**

我们不想给 Guest Hypervisor 直接访问虚拟化控制的权限。这是因为直接访问可能潜在地允许 VM 破坏它的沙箱，或发现关于宿主平台的信息。这个潜在问题类似于前面捕获和模拟示例中展示的问题。

Guest Hypervisor 运行在 EL1。HCR_EL2 中的新控制允许 Host Hypervisor 捕获 Guest Hypervisor 访问虚拟化控制的尝试：

- `HCR_EL2.NV`：启用对嵌套虚拟化的硬件支持
- `HCR_EL2.NV1`：启用一组额外的陷阱
- `HCR_EL2.NV2`：启用对内存的重定向
- `VNCR_EL2`：当 NV2==1 时，指向内存中的一个结构

Armv8.3-A 增加了 NV 和 NV1 控制。从 EL1 对 _EL2 寄存器的访问通常是未定义的，访问会导致进入 EL1 的异常。NV 和 NV1 位会使 EL1 对 _EL2 寄存器的访问改为捕获到 EL2。这允许 Guest Hypervisor 在 EL1 运行，而 Host Hypervisor 在 EL2 模拟它的一些操作。NV 也会捕获来自 EL1 的 ERET。

下图显示了一个 Guest Hypervisor 设置并进入 VM：

![图 9-2：Guest hypervisor 设置并进入 VM](../../asset/figure9-2-guest-hypervisor-setup.png)

*图 9-2：Guest hypervisor 设置并进入 VM*

1. Guest Hypervisor 对 _EL2 寄存器的访问被捕获到 EL2。Host Hypervisor 记录 Guest Hypervisor 正在设置的配置。
2. Guest Hypervisor 尝试进入它的 Guest VM（Guest 的 Guest VM），ERET 被捕获到 EL2。
3. Host Hypervisor 检索 Guest 的 Guest 的配置，并将此配置加载到适当的寄存器中。然后 Host Hypervisor 清除 NV 位并进入 Guest 的 Guest。

这种方法的问题是，Guest Hypervisor 对 EL2 寄存器的每一次单独访问都必须被捕获。在两个 vCPU 或 VM 之间进行任务切换时会访问许多寄存器，导致许多陷阱。每个陷阱都有异常进入和返回的开销。

更好的解决方案是捕获 EL2 寄存器的配置，只在 ERET 时捕获到 Host Hypervisor。这种解决方案在 Armv8.4-A 中可行。当设置 NV2 时，EL1 对 _EL2 寄存器的访问被重定向到内存中的一个结构。Guest Hypervisor 可以根据需要多次读写寄存器，而无需一个陷阱。ERET 仍然会捕获到 EL2，此时 Host Hypervisor 可以从内存中重新检索配置。

下图说明了这个概念：

![图 9-3：ERET 仍然捕获到 EL2](../../asset/figure9-3-eret-traps-to-el2.png)

*图 9-3：ERET 仍然捕获到 EL2*

1. 来自 EL1 的 Guest Hypervisor 对 _EL2 寄存器的访问被重定向到内存中的一个结构。该结构的位置由 Host Hypervisor 使用 VNCR_EL2 指定。
2. Guest Hypervisor 尝试进入它的 Guest VM（即 Guest 的 Guest VM），ERET 被捕获到 EL2。
3. Host Hypervisor 检索 Guest 的 Guest 的配置，并将它们加载到适当的寄存器中。然后 Host Hypervisor 清除 NV 位并进入 Guest 的 Guest。

这种方法的优点是陷阱更少，因此进入 Host Hypervisor 的次数更少。

## 10. 安全虚拟化

虚拟化是在 Armv7-A 中引入的。当时，Hyp 模式（相当于 AArch32 中的 EL2）仅在非安全状态下可用。当引入 Armv8.4-A 时，安全状态下的 EL2 支持作为一个可选特性被加入。

当处理器支持安全 EL2 时，需要从 EL3 使用 SCR_EL3.EEL2 位来启用处理器。设置此位可以进入 EL2，并启用安全状态下的虚拟化特性。

在安全虚拟化可用之前，EL3 通常用于托管安全状态切换软件和平台固件的混合。这是因为我们喜欢尽量减少 EL3 中的软件数量，以便 EL3 更容易加固安全。安全虚拟化允许我们将平台固件移入 EL1。虚拟化为平台固件和受信任内核提供了单独的安全分区。下图说明了这一点：

![图 10-1：安全虚拟化框图](../../asset/figure10-1-secure-virtualization.png)

*图 10-1：安全虚拟化框图*

**安全 EL2 和两个中间物理地址空间**

Arm 架构定义了两个物理地址空间：安全和非安全。在非安全状态下，虚拟机（VM）的 stage 1 翻译的输出总是非安全的。因此，stage 2 只有一个单一的中间物理地址（IPA）空间需要处理。

在安全状态下，VM 的 stage 1 翻译可以输出安全和非安全地址。翻译表描述符中的 NS 位控制输出安全还是非安全地址空间。如下图所示，这意味着 stage 2 有两个 IPA 空间，安全和非安全：

![图 10-2：安全状态下的 IPA 空间](../../asset/figure10-2-ipa-spaces-secure.png)

*图 10-2：安全状态下的 IPA 空间*

与 stage 1 表不同，stage 2 表项中没有 NS 位。对于特定的 IPA 空间，所有翻译结果要么是安全物理地址，要么是非安全物理地址。这种翻译由一个寄存器位控制。通常，非安全 IPA 翻译为非安全 PA，安全 IPA 翻译为安全 PA。

## 11. 虚拟化的代价

虚拟化的代价由 hypervisor 需要服务 VM 时、在虚拟机（VM）和 hypervisor 之间切换所需的时间量决定。在 Arm 系统上，这种代价的下限是：

- 31 个 64 位通用寄存器（X0..X30）
- 32 个 128 位浮点/SIMD 寄存器（V0..V31）
- 两个栈指针（SP_EL0、SP_EL1）

使用 LDP 和 STP 指令，hypervisor 需要 33 条指令来保存或恢复这些寄存器。

一个项目的虚拟化确切代价取决于平台和 hypervisor 的设计。

## 12. 知识自测

**问：Type 1 hypervisor 和 Type 2 hypervisor 有什么区别？**

答：Type 2 hypervisor 运行在宿主操作系统之上，Type 1 hypervisor 没有宿主操作系统。

**问：安全状态和非安全状态各有多少个 IPA 空间？**

答：安全状态有两个 IPA 空间：安全和非安全。非安全状态有一个 IPA 空间。

**问：虚拟中断可以在哪些异常级别被送入？**

答：虚拟中断只能在 EL0 或 EL1 执行时被送入，并且只有在通过设置 HCR_EL2 中相应的路由位来启用时才行。

**问：什么是 SMMU？你如何使用 SMMU 进行虚拟化？**

答：SMMU，即系统 MMU，为非处理器主机提供地址翻译服务。在虚拟化中，SMMU 可用于让主机（例如 DMA 控制器）看到与分配给它的 VM 相同的内存视图。

**问：HCR_EL2.E2H 位如何影响 EL2 处 `MSR TTBR0_EL1, x0` 的执行？**

答：当 E2H==0 时，指令写入 TTBR0_EL1。当 E2H==1 时，写入被重定向到 TTBR0_EL2。

**问：什么是 VMID，它用于什么？**

答：VMID 是虚拟机标识符。它用于标记 VM 的 TLB 表项，使来自多个 VM 的 TLB 表项能够同时在 TLB 中共存。

**问：什么是陷阱，它如何用于虚拟化？**

答：陷阱使原本合法的操作触发异常，将该操作捕获到具有更高特权的软件。在虚拟化中，陷阱允许 hypervisor 检测某些操作何时被执行，并模拟这些操作。

## 13. 相关资料

以下是与本指南内容相关的一些资源：

- Memory Management（内存管理）
- Exception Model（异常模型）
- Arm Virtualization: Performance and Architectural Implications（关于基于 Arm 系统的虚拟化代价的背景阅读）
- Arm community（提出开发问题，并查找 Arm 专家针对特定主题的文章和博客）

以下是与本指南主题相关的资源：

- **虚拟化简介**
  - The Xen project
  - General information on KVM hypervisors
- **异常虚拟化**
  - GICv3/v4 Software Overview（关于 GIC 如何虚拟化中断的详情）
  - https://www.linux-kvm.org/page/Virtio（关于 VirtIO 的背景阅读）
- **有用的培训链接**
  - Introduction to Armv8-A
  - Memory model overview
  - AArch64 privilege and security model

## 14. 下一步

本指南介绍了 Armv8-A AArch64 提供的虚拟化支持。这对于任何准备在其系统中实现虚拟化的人来说都很有用。

按照本指南学习后，你可以探索一个简单的裸机示例。该示例演示了一些虚拟化特性的使用。它在单独的虚拟环境中运行两个 EL1 镜像，并使用中断在它们之间进行上下文切换。虽然该示例不是一个完整的 hypervisor，但它允许你试验处理器特性。

另外，《Spawn a Linux virtual machine on Arm using QEMU (KVM)》带你完成在 Arm Foundation 模型上设置开源 XEN 和 KVM hypervisor 的过程。

要了解更多关于安全的信息，《Security Features》讨论了架构安全特性。
