# 读懂 AArch64 内存管理示例（中文精读版）

> 原文：*Learn the architecture - AArch64 memory management examples*
>
> 版本：Version 1.0　|　文档 ID：102416_0100_01_en　|　Issue 01（2019-06-27）
>
> 版权 © 2019 Arm Limited（或其关联公司）。保留所有权利。非保密。
>
> 说明：本文在原中文翻译的基础上重新组织了叙述逻辑与表达方式，代码、表格与图表保持原意；个别明显笔误已按 Arm 架构规范订正。

---

## 导读：这组示例做什么

这组示例演示如何在**裸机环境**下设置内存管理单元（MMU）。它们逐段讲解代码，建立在《内存管理指南》对 MMU 与翻译过程的整体解释之上。

如果你需要在低层与 MMU 打交道（通常是裸机环境，如 bring-up 测试代码），这些示例很有用。它们不涉及操作系统中的 MMU 用法。

这里构建的虚拟地址空间并不追求“真实”，而是演示配置 MMU 的不同方式；最后一个示例才给出一个更贴近真实裸机系统的配置。学完之后，你将能编写或修改一段设置简单虚拟地址空间的代码序列。

### 开始之前

要求你熟悉内存翻译原理和 MMU 控制，这些见《内存管理指南》。示例使用 A64 汇编，基础可参考《Armv8-A Instruction Set Architecture guide》。

### 示例平台

这组示例作为单独下载提供，面向 Base Platform 模型开发。构建和运行需要 Arm Development Studio（可下载评估版）。示例包含 ReadMe.txt，说明文件清单与构建/运行方法；不同示例的模拟器启动参数不同，请以 ReadMe.txt 为准。

## 1. 示例一：EL3 单级表

第一个示例最简单：在 EL3 翻译机制里做**单级翻译**，并**平坦映射**虚拟地址——所有翻译的输入虚拟地址与输出物理地址相同，MMU 只用来控制属性和权限。

文件位于：`<example dir>\el3_stage1_l1only\`

### 1.1 指定翻译表的位置

代码在 `startup.S` 里，MMU 部分从第 159 行开始。第一段关键代码：

```asm
// Set the Base address
// ---------------------
LDR   x0, =tt_l1_base  // 获取 TTBR0_EL3 的第 1 级地址
MSR   TTBR0_EL3, x0    // 设置 TTBR0_EL3（注意：EL3 没有 TTBR1）
```

这段代码加载为翻译表分配的内存地址，写入翻译表基址寄存器（`TTBR0_ELx`）。该寄存器告诉处理器：表遍历时第一级表在哪里。

符号名表明它指向第 1 级表；翻译起始级别如何配置，见后文“配置翻译机制”。

表的分配在文件末尾：

```asm
.section TT,"ax"
.align 12
.global tt_l1_base
tt_l1_base:
.fill 4096 , 1 , 0
```

代码定义一个标签（`tt_l1_base`）来引用这块内存，再用 `fill` 分配一个预填充为零的 4KB 块。预填零很有用，因为翻译表项值为 0 就表示 Fault。

翻译表必须按大小对齐。本例是一个完整的第 1 级表：4KB 颗粒下，完整 L1 表有 512 个表项，每项 8 字节，总共 4KB，必须从 4KB 边界开始。`align` 指令把对齐设为 2 的幂，这里 2^12=4096。

### 1.2 初始化 MAIR

下一步是设置内存属性：

```asm
// Set up memory attributes
 // -------------------------
 // 这等同于：
 // 0 = b01000100 = Normal, Inner/Outer Non-Cacheable
 // 1 = b11111111 = Normal, Inner/Outer WB/WA/RA
 // 2 = b00000000 = Device-nGnRnE
 MOV   x0, #0x000000000000FF44
 MSR   MAIR_EL3, x0
```

《AArch64 Memory model guide》讲过：类型（Normal 或 Device）不直接编进 stage 1 表项，而是由表项给出一个指向内存属性间接寄存器（`MAIR_ELx`）的索引。每个 8 位表项由软件设成不同内存类型。本例只填 MAIR 前三项：

- [0] = Normal，内部/外部不可缓存
- [1] = Normal，内部/外部可缓存，写回 + 读/写分配
- [2] = Device_nGnRnE

这三种类型对这个简单示例已足够，其他索引值不用。

哪个类型放在哪个 MAIR 索引，会在稍后创建翻译表项时用到。

### 1.3 配置翻译机制

```asm
 // Set up TCR_EL3
 // ---------------
 MOV   x0, #0x19            // T0SZ=0b011001 将 VA 空间限制为 39 位，
                            // 翻译从 l1 开始
 ORR   x0, x0, #(0x1 << 8)  // IGRN0=0b01  对 TTBR0 的遍历是 Inner WB/WA
 ORR   x0, x0, #(0x1 << 10) // OGRN0=0b01  对 TTBR0 的遍历是 Outer WB/WA
 ORR   x0, x0, #(0x3 << 12) // SH0=0b11   Inner Shareable
                            // TBI0=0b0   不忽略顶部字节
                            // TG0=0b00   4KB 颗粒
                            // IPS=0      32 位 PA 空间
 MSR   TCR_EL3, x0
```

翻译控制寄存器（`TCR_ELx`）配置翻译机制的多个方面：

- **TnSZ**：控制正在描述的虚拟地址空间大小；
- **TGn**：设置翻译颗粒，即可描述的最小块；
- **IGRNn/ORGNn/SH**：指定 MMU 表遍历用的可缓存性与可共享性；
- **TBIn**：顶部字节忽略。置位后，处理器做虚拟到物理翻译时忽略虚拟地址顶部 8 位，软件可用这 8 位存别的信息。本例不用，保持禁用。

> TBI 的应用示例见《Providing protection for complex software guide》中对内存标记的描述。

所有示例的颗粒（TG0）都是 4KB。颗粒决定可用的页/块大小。4KB 颗粒下：

- L0 表：每项 512GB
- L1 表：每张覆盖 512GB，每项 1GB
- L2 表：每张覆盖 1GB，每项 2MB
- L3 表：每张覆盖 2MB，每项 4KB

虚拟地址空间大小 = 64 – TnSZ。本例 64–0x19 = 39 位，即 512GB（2^39），正好由单个 L1 表覆盖，所以翻译起始级别是第 1 级。

接着是：

```asm
 // Invalidate TLBs
 // ----------------
 TLBI  ALLE3
 DSB   SY
 ISB
```

TLB 在复位时的状态不可保证，所以示例在启用 MMU 前先使 TLB 失效。`TLBI ALLE3` 使 EL3 翻译机制的全部缓存翻译失效——这正是示例正在配置的机制。

### 1.4 生成翻译表

在内存里生成表，创建一组最小表项：

```asm
LDR   x1, =tt_l1_base              // L1 表的地址
 // [0]: 0x0000,0000 - 0x3FFF,FFFF
 LDR   x0, =TT_S1_DEVICE_nGnRnE     // 表项模板
                                    // AP=0, RW
                                    // 不需要 OR 入地址，因为它为 0
 STR   x0, [x1]
 
 // [1]: 0x4000,0000 - 0x7FFF,FFFF
 LDR   x0, =TT_S1_DEVICE_nGnRnE     // 表项模板
                                    // AP=0, RW
 ORR   x0, x0, #0x40000000          // 用基物理地址 'OR' 模板
 STR   x0, [x1, #8]
 // [2]: 0x8000,0000 - 0xBFFF,FFFF (VE 和 Base Platform 上的 DRAM)
 LDR   x0, =TT_S1_NORMAL_WBWA       // 表项模板
 ORR   x0, x0, #TT_S1_INNER_SHARED  // 用内部可共享属性 'OR'
                                    // AP=0, RW
 ORR   x0, x0, #0x80000000          // 用基物理地址 'OR' 模板
```

L1 是本例的第一级翻译，4KB 颗粒下每个表项覆盖 1GB。示例只填前三个表项，覆盖前 3GB 虚拟地址空间。

前面分配翻译表时用了 `fill` 预填零，零即 Fault，所以所有没写过的表项都是故障表项。

> 真实系统中，软件通常在运行时用零填充表，而不是依赖源代码里预分配。但预填零能加快部分模拟/仿真。

### 1.5 表项是如何形成的

代码使用文件开头定义的模板符号。例如 `TT_S1_NORMAL_WBWA` 是一个 Normal、写回、读/写分配表项的模板：

```asm
.equ TT_S1_NORMAL_WBWA,   0x00000000000000405
```

stage 1 第 1 级表项的格式如下：

![图 2-1：理解表项是如何形成的图](../../asset/figure2-1-entry-formed.png)

*图 2-1：理解表项是如何形成的图*

解码 `TT_S1_NORMAL_WBWA`：

- Indx= b01：从 MAIR 表项 [1] 取类型信息
- NS= b0：输出物理地址是安全的
- AP= b00：地址可读可写
- SH= b00：不可共享
- AF= b1：访问标志已预置，访问时不产生访问标志故障
- nG= 在 EL3 不使用
- Contig= b0：表项不是连续块的一部分
- PXN= b0：块可执行（此属性在 EL3 叫 XN）
- UXN= 在 EL3 不使用

这里也解释了为什么了解 MAIR 配置很重要：模板依赖 MAIR 表项 [1] 预置为 Normal/Cacheable。

示例想让该区域“内部可共享”，而模板里是“不可共享”。于是把 `TT_S1_NORMAL_WBWA` 与另一个模板 `TT_S1_INNER_SHAREABLE` 组合，后者在 SH 字段写入正确的值。

### 1.6 知识自测

**问题**：查看示例里的其他模板。你会怎么修改上面的示例，把地址映射到**非安全**物理地址？

**答案**：把 NS 位置 1。示例有现成模板 `TT_S1_NS`，可以像可共享属性那样 OR 进去。

### 1.7 配置后的虚拟地址空间

有了这组表项，虚拟地址空间看起来如下：

![图 2-2：MMU 虚拟地址空间](../../asset/figure2-2-mmu-va-space.png)

*图 2-2：MMU 虚拟地址空间*

### 1.8 启用 MMU

MMU 已配置，翻译表已就绪，接下来启用：

```asm
 // Enable MMU
 // -----------
 MOV   x0, #(1 << 0)      // M=1  启用 stage 1 MMU
 ORR   x0, x0, #(1 << 2)  // C=1  启用数据和统一缓存
 ORR   x0, x0, #(1 << 12) // I=1  启用指令取指分配到统一缓存
                        // A=0  禁用严格对齐检查
                        // SA=0 禁用栈对齐检查
                        // WXN=0 写权限不隐含 XN
                        // EE=0 EL3 数据访问是小端
 MSR   SCTLR_EL3, x0
 ISB
```

示例设置 `SCTLR_ELx` 的 M、C、I 位，启用 MMU 和缓存。写 SCTLR 后的 ISB 确保 MMU 启用的效果对下一条指令可见。

### 1.9 表遍历

示例跑在 Base Platform Fixed Virtual Platform（FVP，Arm 提供的一个模型）上。FVP 能输出 TARMAC 格式的跟踪，记录处理器执行的细节。跟踪整个示例会产生数百行，我们只从 MMU 启用的位置看起：

```text
75 clk IT (75) 8000012c d51e1000 O EL3h_s : MSR   SCTLR_EL3,x0
75 clk R SCTLR_EL3 00000000:00001005
75 clk CACHE FVP_Base_AEMv8A_AEMv8A.cluster0.cpu0.l1dcache LINE 0100
 ALLOC 0x000080002000
75 clk CACHE FVP_Base_AEMv8A_AEMv8A.cluster0.l2_cache LINE 0800
 ALLOC 0x000080002000
75 clk TTW ITLB LPAE 1:1 000080002010 0000000080000705 : BLOCK ATTRIDX=1 NS=0 AP=0
 SH=3 AF=1 nG=0 16E=0 PXN=0 XN=0 ADDR=0x0000000080000000
75 clk TLB FILL FVP_Base_AEMv8A_AEMv8A.cluster0.cpu0.UTLB 1G 0x80000000 EL3_s,
 nG asid=0:0x0080000000 Normal InnerShareable Inner=WriteBackWriteAllocate
 Outer=WriteBackWriteAllocate xn=0 pxn=0 ContiguousHint=0
75 clk CACHE FVP_Base_AEMv8A_AEMv8A.cluster0.cpu0.l1icache LINE 0008
 ALLOC 0x000080000100
75 clk CACHE FVP_Base_AEMv8A_AEMv8A.cluster0.l2_cache LINE 0040
 ALLOC 0x000080000100
76 clk IT (76) 80000130 d5033fdf O EL3h_s : ISB
```

逐行看。先看第一部分：

```text
75 clk IT (75) 8000012c d51e1000 O EL3h_s : MSR   SCTLR_EL3,x0
75 clk R SCTLR_EL3 00000000:00001005
```

这显示 MSR 的执行，它启用了 MMU。`0x8000_012C` 是指令地址，`0xD51E_1000` 是操作码。第二行显示指令写入寄存器的值。

> 默认情况下，跟踪显示“写入的值”，不是寄存器的新值。多数情况新值等于写入值，但寄存器含只读字段时例外。

因为这条指令启用了 MMU，处理器需要为下一条指令所在的页做一次表遍历。先看到：

```text
75 clk CACHE FVP_Base_AEMv8A_AEMv8A.cluster0.cpu0.l1dcache LINE 0100
 ALLOC 0x000080002000
75 clk CACHE FVP_Base_AEMv8A_AEMv8A.cluster0.l2_cache LINE 0800
 ALLOC 0x000080002000
```

配置 TCR_EL3 时，我们让 MMU 对表遍历使用可缓存访问。这两行显示“包含所需表项的缓存行”被取入缓存。行返回后，MMU 解释描述符：

```text
75 clk TTW ITLB LPAE 1:1 000080002010 0000000080000705 : BLOCK ATTRIDX=1 NS=0 AP=0
 SH=3 AF=1 nG=0 16E=0 PXN=0 XN=0 ADDR=0x0000000080000000
```

各项含义：

- TTW= 表遍历
- ITLB= 指令接口的表遍历（I 代表指令）
- 1:1= Stage 1、第 1 级表项
- 0x80002010= 表项被取出的地址
- 0x0000000080000705= 从内存系统返回的表项
- BLOCK= 该表项是块表项
- ATTRIDX=1= 使用 MAIR 表项 1
- NS= 输出物理地址安全
- AP= 访问权限位
- SH= 可共享性位

最后是生成的 TLB 记录：

```text
75 clk TLB FILL FVP_Base_AEMv8A_AEMv8A.cluster0.cpu0.UTLB 1G 0x80000000 EL3_s,
 nG asid=0:0x0080000000 Normal InnerShareable Inner=WriteBackWriteAllocate
 Outer=WriteBackWriteAllocate xn=0 pxn=0 ContiguousHint=0
```

TLB 项创建如下：

- 1GB 块
- PA: 0x8000_0000
- VA: 0x8000_0000，ASID 0（EL3 不使用 ASID）
- 翻译机制：EL3
- Normal、内部可共享、写回、写分配
- 可执行

**知识自测**：回看前面的代码，指出跟踪里的各项设置分别来自哪里。

## 2. 示例二：EL3 多级表

这个示例使用**两级翻译**。单级表示例只用单个 L1 表，所有映射都是 1GB 块——对简单系统够用，但很多系统需要更细粒度，这就靠多级表。

文件位于：`<example dir>\el3_stage1_l1andl2\`

### 2.1 生成 Level 1 表

前几步与单级表示例相同：MAIR 填三种类型，TCR 选 4KB 颗粒、起始级别 L1。区别在表的生成：

```asm
//
 // 生成 L1 表
 //
 
 LDR   x1, =tt_l1_base           // L1 表的地址
 // [0]: 0x0000,0000 - 0x3FFF,FFFF
 LDR   x0, =TT_S1_DEVICE_nGnRnE  // 表项模板
                                 // AP=0, RW
                                 // 不需要 OR 入地址，因为它为 0
 STR   x0, [x1]
 
 // [1]: 0x4000,0000 - 0x7FFF,FFFF
 LDR   x0, =TT_S1_DEVICE_nGnRnE  // 表项模板
                                 // AP=0, RW
 ORR   x0, x0, #0x40000000       // 用基物理地址 'OR' 模板
 STR   x0, [x1, #8]
 // [2]: 0x8000,0000 - 0xBFFF,FFFF (VE 和 Base Platform 上的 DRAM)
 LDR   x2, =tt_l2_base           // 获取 L2 表的地址
 LDR   x0, =TT_S1_TABLE          // 指向下一级表的表项模板
 ORR   x0, x0, x2                // 用 L2 表基址组合模板
 STR   x0, [x1, #16]             // 将模板写入表项 table[2]
```

前两个表项与单级表示例相同，创建两个 Device 类型的 1GB 块映射。第三个不同：它不映射 1GB 块，而是**指向第 2 级表**，把 1GB 块划分成 512 个 2MB 块。这里用了模板 `TT_S1_TABLE`。

第 2 级表地址 `tt_l2_base` 从哪来？同样用 `fill` 分配 4KB 内存：

```asm
 .align 12 
.global tt_l1_base
tt_l1_base:
 .fill 4096 , 1 , 0
 .global tt_l2_base
tt_l2_base:
 .fill 4096 , 1 , 0
```

值为 0 表示故障；真实系统在运行时写零，测试代码里预填零可减少模拟时间。第 2 级表同样需要 4KB 对齐。

> 本例中表是对齐的，因为它紧跟在另一个 4KB 对齐结构之后。

### 2.2 生成 Level 2 表

```asm
 //
 // 生成 L2 表
 //
 …
 LDR   x0, =tt_l2_base             // 第一个 L2 表的地址
 // L2 表覆盖的地址范围：
 // 0x8000_0000 - 0xBFFF_FFFF
 //
 // 此示例只填充表项 0，它覆盖：
 // 0x8000_0000 - 0x801F_FFFF
 LDR   x1, =tt_l2_base             // L1 表的地址
 LDR   x0, =TT_S1_NORMAL_WBWA      // 表项模板
 ORR   x0, x0, #TT_S1_INNER_SHARED // 用内部可共享属性 'OR'
                                   // AP=0, RW
 ORR   x0, x0, #0x80000000         // 用基物理地址 'OR' 模板
 STR   x0, [x1]
 DSB   SY
```

这个 L2 表覆盖 0x8000_0000 到 0xBFFF_FFFF（第三个 GB），共 512 个表项、每项 2MB。示例只填第一个表项（Normal/Cacheable 块），对应 0x8000_0000 到 0x801F_FFFF。

### 2.3 配置后的虚拟地址空间

![图 3-1：配置后的虚拟地址空间概览](../../asset/figure3-1-configured-va.png)

*图 3-1：配置后的虚拟地址空间概览*

### 2.4 表遍历

再看 MMU 启用后的第一次表遍历跟踪：

```text
81 clk TTW ITLB LPAE 1:1 000080002010 0000000080003003 : TABLE PXN=0 XN=0 AP=0 NS=0
 ADDR=0x0000000080003000
81 clk TTW ITLB LPAE 1:2 000080003000 0000000080000705 : BLOCK ATTRIDX=1 NS=0 AP=0
 SH=3 AF=1 nG=0 16E=0 PXN=0 XN=0 ADDR=0x0000000080000000
81 clk TLB FILL FVP_Base_AEMv8A_AEMv8A.cluster0.cpu0.UTLB 2M 0x80000000 EL3_s,
 nG asid=0:0x0080000000 Normal InnerShareable Inner=WriteBackWriteAllocate
 Outer=WriteBackWriteAllocate xn=0 pxn=0 ContiguousHint=0
```

这次有两条 ITLB 行。第一条报告 `1:1`（stage 1 第 1 级），它取出的 L1 表项指向 L2 表。第二条报告 `1:2`（stage 1 第 2 级），是个块描述符，和单级示例看到的一样。最后一行记录 TLB 项，因为翻译来自第 2 级块，大小记为 2MB。

### 2.5 知识自测

**问题**：此示例展示了两级表的 stage 1 翻译。对 stage 2 第 3 级表项，你期望看到什么跟踪？

**答案**：`2:3`

## 3. 示例三：EL1 单级表

这个示例把“EL3 单级表”搬到**非安全 EL1**。前两个示例都跑在 EL3。

文件位于：`<example dir>\el1_stage1\`

### 3.1 进入 NS.EL1

处理单元（PE）复位后总是进入最高已实现异常级别，本例是 EL3。因此示例需要包含从 EL3 切到 EL1 的代码，切换前要先在 EL3 做些配置。

先配置安全配置寄存器（SCR_EL3）：

```asm
 // 配置 SCR_EL3
 // ------------------
 MOV   x0, #1              // NS=1
 ORR   x0, x0, #(1 << 1)   // IRQ=1 IRQ 路由到 EL3
 ORR   x0, x0, #(1 << 2)   // FIQ=1 FIQ 路由到 EL3
 ORR   x0, x0, #(1 << 3)   // EA=1  SError 路由到 EL3
 ORR   x0, x0, #(1 << 8)   // HCE=1 HVC 指令被启用
 ORR   x0, x0, #(1 << 10)  // RW=1  下一个更低的 EL 使用 AArch64
 ORR   x0, x0, #(1 << 11)  // ST=1  安全 EL1 可以访问定时器
 MSR   SCR_EL3, x0 
```

对本文最关键的两个设置：

- **NS**：控制较低 EL 是安全还是非安全；
- **RW**：控制下一 EL 用 AArch64 还是 AArch32。

示例把两位都置 1，于是较低 EL 非安全、且 EL2 用 AArch64。

还要配置虚拟机监控器配置寄存器（HCR_EL2）。真实软件栈里这是 EL2 代码做的，示例为简单起见从 EL3 编程：

```asm
 // 配置 HCR_EL2
 // ------------------
 ORR   w0, wzr, #(1 << 3)       // FMO=1
 ORR   x0, x0, #(1 << 4)        // IMO=1
 ORR   x0, x0, #(1 << 31)       // RW=1     NS.EL1 是 AArch64
                                // TGE=0    可以进入 NS.EL1
                                // VM=0     Stage 2 MMU 禁用
 MSR   HCR_EL2, x0
```

这里最关心的两个设置：

- **RW**：控制 EL1 用 AArch64 还是 AArch32；
- **VM**：在 EL1/EL0 启用/禁用 stage 2 翻译。

示例禁用 stage 2，并让 EL1 用 AArch64。

切到 EL2 前，还要配置 EL2 的若干寄存器：

```asm
 // 设置 VMPIDR_EL2/VPIDR_EL2
 // ---------------------------
 MRS   x0, MIDR_EL1
 MSR   VPIDR_EL2, x0
 MRS   x0, MPIDR_EL1
 MSR   VMPIDR_EL2, x0
 
 
 // 设置 VMID
 // ---------
 // 虽然我们不使用 stage 2 翻译，NS.EL1 仍然关心 VMID
 MSR   VTTBR_EL2, xzr
 // 将 EL1/2 的 SCTLR 设置为安全值
 // ------------------------------------
 MSR   SCTLR_EL2, xzr
 MSR   SCTLR_EL1, xzr
```

在 NS.EL1 读 `MIDR_EL1` 和 `MPIDR_EL1` 返回虚拟值。保存这些虚拟值的 `VPIDR_EL2`、`VMPIDR_EL2` 没有定义复位值，软件应在首次进入 EL1 前初始化。本例不用虚拟化，所以直接复制物理值。

即使 stage 2 被禁用，EL1 仍使用 VMID，进入 EL1 前设成已知值是良好习惯，多核环境尤其重要——运行在同一 NS.EL0/1 翻译机制的所有 PE 要用相同 VMID。

最后，EL3/EL2/EL1 各有独立的 `SCTLR_ELx`，只有最高已实现 EL 的 SCTLR 有已知复位值。软件必须在进入较低 EL 前，把低 EL 的 SCTLR 设为已知/安全值。本例置 0，确保该 EL 的 MMU 被禁用。

最小配置完成，把控制交到 NS.EL1：

```asm
 ADR   x0, el1_entry
 MSR   ELR_EL3, x0
 LDR   x0, =AArch64_EL1_SP1
 MSR   spsr_el3, x0
 
 ERET
// ------------------------------------------------------------
// 进入 EL1
// ------------------------------------------------------------
el1_entry:
```

移动到更低 EL 的唯一办法是异常返回。通常返回信息是进入异常时生成的；但这是启动过程，示例手动填好：设置 `SPSR_ELx` 让 EL1 用 AArch64，设置 `ELR_ELx` 指向 EL1 代码入口，然后 ERET 执行异常返回。

### 3.2 在 EL1 配置 MMU

进入 EL1 后配置 MMU，步骤与第一个示例相同，只是把 `_EL3` 寄存器换成 `_EL1`：

```asm
 // 设置基址
 // ---------------------
 LDR   x0, =tt_l1_base
 MSR   TTBR0_EL1, x0          
 // 设置内存属性
 // -------------------------
 // 这等同于：
 // 0 = b01000100 = Normal, Inner/Outer Non-Cacheable
 // 1 = b11111111 = Normal, Inner/Outer WB/WA/RA
 // 2 = b00000000 = Device-nGnRnE
 MOV   x0, #0x000000000000FF44
 MSR   MAIR_EL1, x0
```

与 EL3 不同，EL1/0 有两个虚拟地址区域：一个在地址空间底部，一个在顶部：

![图 4-1：EL1 配置图](../../asset/figure4-1-el1-config.png)

*图 4-1：EL1 配置图*

按惯例，低地址区叫用户空间，高地址区叫内核空间——这只是一种约定，架构参考手册里没有这些名字。

本例只配置低区域，并用 TCR_ELx 的控制位禁用高区域：

```asm
 // 设置 TCR_EL1
 // ---------------
 MOV   x0, #0x19             // T0SZ=0b011001 将 VA 空间限制为 39 位
 ORR   x0, x0, #(0x1 << 8)   // IGRN0=0b01  对 TTBR0 的遍历是 Inner WB/WA
 ORR   x0, x0, #(0x1 << 10)  // OGRN0=0b01  对 TTBR0 的遍历是 Outer WB/WA
 ORR   x0, x0, #(0x3 << 12)  // SH0=0b11   Inner Shareable
 ORR   x0, x0, #(0x1 << 23)  // EPD1=0b1   禁用来自 TTBR1 的表遍历
                             // TBI0=0b0
                             // TG0=0b00   TTBR0 的 4KB 颗粒
                             // A1=0     TTBR0 包含 ASID
                             // AS=0     8 位 ASID
                             // IPS=0     32 位 IPA 空间
 MSR   TCR_EL1, x0
```

`EPDn` 位控制低区域（EPD0）和高区域（EPD1）的遍历。示例只配置低区域，禁用高区域（EPD1==1），因此无需在 `TTBR1_EL1` 提供表指针。翻译表的生成代码与“EL3 单级表”相同。

### 3.3 非安全翻译机制

这个示例跑在 NS.EL1，翻译表与“EL3 单级表”相同。那产生的映射也一样吗？

不一样。安全与非安全翻译机制有个重要区别：**安全翻译机制**把虚拟地址映射到由表项 NS 位控制的安全或非安全物理地址；**非安全翻译机制**只映射到非安全物理地址，表项的 NS 位被忽略。

“EL3 单级表”和本例中，表项 NS 位都是 b0（安全）。在 EL3，输出地址安全；在 NS.EL1，NS 位被忽略，输出地址非安全。

> 真实系统上你不太可能同时跑这两个示例——物理地址 0x8000_0000 处的内存要么安全、要么非安全，内存系统不会允许两种访问混用。这里用的 FVP 模型可以用模型参数控制对 DRAM 允许哪种访问。

### 3.4 表遍历

本示例的跟踪与“EL3 单级表”很相似：

```text
93 clk IT (93) 80000174 d5181000 O EL1h_n : MSR   SCTLR_EL1,x0
93 clk R SCTLR_EL1 00000000:00001005
93 clk CACHE FVP_Base_AEMv8A_AEMv8A.cluster0.cpu0.l1dcache LINE 0100
 ALLOC 0x000080002000_NS
93 clk CACHE FVP_Base_AEMv8A_AEMv8A.cluster0.l2_cache LINE 0800
 ALLOC 0x000080002000_NS
93 clk TTW ITLB LPAE 1:1 000080002010 0000000080000705 : BLOCK ATTRIDX=1 NS=0 AP=0
 SH=3 AF=1 nG=0 16E=0 PXN=0 XN=0 ADDR=0x0000000080000000
93 clk TLB FILL FVP_Base_AEMv8A_AEMv8A.cluster0.cpu0.UTLB 1G 0x80000000_NS EL1_n
 vmid=0:0x0080000000_NS Normal InnerShareable Inner=WriteBackWriteAllocate
 Outer=WriteBackWriteAllocate xn=0 pxn=0 ContiguousHint=0
93 clk CACHE FVP_Base_AEMv8A_AEMv8A.cluster0.cpu0.l1icache LINE 000a
 ALLOC 0x000080000140_NS
93 clk CACHE FVP_Base_AEMv8A_AEMv8A.cluster0.l2_cache LINE 0050
 ALLOC 0x000080000140_NS
```

但有几个重要区别。先看 MSR：

```text
93 clk IT (93) 80000174 d5181000 O EL1h_n : MSR   SCTLR_EL1,x0
```

TARMAC 记录指令执行的 EL 与安全状态。前面是 `EL3h_s`，本例是 `EL1h_n`——EL1、非安全。

TLB 项也不同：

```text
93 clk TLB FILL FVP_Base_AEMv8A_AEMv8A.cluster0.cpu0.UTLB 1G 0x80000000_NS EL1_n
 vmid=0:0x0080000000_NS Normal InnerShareable Inner=WriteBackWriteAllocate
 Outer=WriteBackWriteAllocate xn=0 pxn=0 ContiguousHint=0
```

跟踪记录了翻译机制（EL1_n），还有正在存储的 VMID（vmid=0）。如“进入 NS.EL1”所述，即使 stage 2 禁用，非安全 EL1 翻译机制仍会记录 VMID。

## 4. 示例四：更复杂的虚拟地址空间

这个示例展示一组更复杂的映射，同样在 EL3 运行。前面的示例只映射少量块，对简单测试镜像够了；但更大的系统要映射更多资源、且粒度更细。

文件位于：`<example dir>\el3_stage1_full_mem_map\`

### 4.1 系统物理地址映射

示例面向 Arm 的 Base Platform Model，其地址映射是现代 Arm SoC 的典型代表：

| 物理地址 | 组件 | 安全或非安全 | 示例分配的属性 |
| --- | --- | --- | --- |
| 0x0000_0000 到 0x03FF_FFFF | Trusted ROM | Secure | Normal、Cacheable、Shareable、Read-only、Executable |
| 0x0400_0000 到 0x05FF_FFFF | – | – | Fault |
| 0x0600_0000 到 0x07FF_FFFF | Trusted DRAM | Secure | Normal、Cacheable、Shareable、Read/Write、Executable |
| 0x0800_0000 到 0x0FFF_FFFF | Flash | Non-secure | Normal、Cacheable、Shareable、Read-only、XN\* |
| 0x1000_0000 到 0x19FF_FFFF | – | – | Fault |
| 0x1A00_0000 到 0x7FFF_FFFF | Peripherals | Secure | Device-nGnRnE、Read/Write、XN |
| 0x8000_0000 到 0xFFFF_FFFF | DRAM | Non-secure\*\* | Normal、Cacheable、Shareable、Read/Write、XN\* |

> \* 这些是非安全内存。本示例在 EL3（安全状态的一部分）运行，通常想在安全状态阻止从非安全位置执行；也可用 `SCR_EL3.CIF` 阻止。
>
> \*\* 用的 FVP 模型可配置 DRAM 为非安全、或既安全又非安全。前面的示例配成两者都允许，本示例只允许非安全访问，更贴近真实。

这个内存映射映射成 MMU 映射后大致如下：

![图 5-1：内存映射图](../../asset/figure5-1-memory-map.png)

*图 5-1：内存映射图*

### 4.2 设置第一级翻译

上面这个地址映射总共 4GB（0x0..0xFFFF_FFFF），即 32 位。虚拟地址空间大小 = 64–T0SZ，示例把 `TCR_EL3.T0SZ` 设为 32：

```asm
  // 设置 TCR_EL3
  // ---------------
  MOV      x0, #32                // T0SZ=32 将 VA 空间限制为 32 位
  ORR      x0, x0, #(0x1 << 8)    // IGRN0=0b01    对 TTBR0 的遍历是 Inner WB/WA
  ORR      x0, x0, #(0x1 << 10)   // OGRN0=0b01    对 TTBR0 的遍历是 Outer WB/WA
  ORR      x0, x0, #(0x3 << 12)   // SH0=0b11      Inner Shareable
                                  // TBI0=0b0      不忽略顶部字节
                                  // TG0=0b00      4KB 颗粒
                                  // IPS=0         32 位 PA 空间
  MSR      TCR_EL3, x0
```

4KB 颗粒下，4GB 对单个 L2 表来说太大（每个 L2 表只覆盖 1GB），所以起始级别是第 1 级。L1 表每项覆盖 1GB，4GB 地址空间只需要 4 个表项，不需要为其他表项提供内存或值。

之前的示例总是分配完整（512 项）的 L1 表。翻译表必须大小对齐：512 项 × 8 字节 = 4KB 对齐。本例只需 4 项 × 8 字节 = 32 字节对齐；但为简单起见仍分配 4KB 对齐的表：

```asm
.align 12
  .global tt_l1_base
tt_l1_base:
  .fill 32 , 1 , 0
  .align 12
  .global tt_l2_base
tt_l2_base:
  .fill 4096 , 1 , 0
```

### 4.3 Level 1 表

```asm
  //
  // 生成 L1 表
  //
  
  LDR      x1, =tt_l1_base             // L1 表的地址
  // [0]: 0x0000,0000 - 0x3FFF,FFFF
  LDR      x2, =tt_l2_base             // 获取 L2 表的地址
  LDR      x0, =TT_S1_TABLE            // 指向下一级表的表项模板
  ORR      x0, x0, x2                  // 用 L2 表基址组合模板
  STR      x0, [x1]
  
  // [1]: 0x4000,0000 - 0x7FFF,FFFF
  LDR      x0, =TT_S1_DEVICE_nGnRnE    // 表项模板
                                       // AP=0, RW
  ORR      x0, x0, #0x40000000         // 用基物理地址 'OR' 模板
  STR      x0, [x1, #8]
  // [2]: 0x8000,0000 - 0xBFFF,FFFF (VE 和 Base Platform 上的 DRAM)
  LDR      x0, =TT_S1_NORMAL_WBWA      // 表项模板
  ORR      x0, x0, #TT_S1_INNER_SHARED // 用内部可共享属性 'OR'
  ORR      x0, x0, #TT_S1_NS           // 用 NS==1 'OR'
  ORR      x0, x0, #TT_S1_PXN          // 用 XN==1 'OR'
                                       // AP=0, RW
  ORR      x0, x0, #0x80000000         // 用基物理地址 'OR' 模板
  STR      x0, [x1, #16]
  
  // [3]: 0xC000,0000 - 0xFFFF,FFFF (VE 和 Base Platform 上的 DRAM)
  LDR      x0, =TT_S1_NORMAL_WBWA      // 表项模板
  ORR      x0, x0, #TT_S1_INNER_SHARED // 用内部可共享属性 'OR'
  ORR      x0, x0, #TT_S1_NS           // 用 NS==1 'OR'
  ORR      x0, x0, #TT_S1_PXN          // 用 XN==1 'OR'
                                       // AP=0, RW
  ORR      x0, x0, #0xC0000000         // 用基物理地址 'OR' 模板
  STR      x0, [x1, #24]
```

L1 表只有 4 个表项，因为 4GB 空间、每项 1GB。

第一个 1GB（0x0000_0000 到 0x3FFF_FFFF）需要描述多个区域，所以该项指向 L2 表做更细粒度映射。后三个 1GB 用块映射即可——块越大越高效，翻译表占用更少、每个 TLB 项覆盖更多地址。

### 4.4 Level 2 表

第一个 1GB 的 L2 表：

```asm
//
  // 生成 L2 表
  //
  …
  LDR      x1, =tt_l2_base              // L1 表的地址
  // [0..31]: 0x0000,0000 - 0x03FF,FFFF (Trusted Boot ROM)
  LDR      x0, =TT_S1_NORMAL_WBWA       // 表项模板
  ORR      x0, x0, #TT_S1_INNER_SHARED  // 用内部可共享属性 'OR'
  ORR      x0, x0, #TT_S1_PRIV_RO       // 'OR' 入 Read-only
  ORR      x0, x0, xzr                  // 用基物理地址 'OR' 模板
  MOV      x2, #32
1:
  STR      x0, [x1], #8
  ADD      x0, x0, #0x200000            // 递增物理地址字段
  SUB      x2, x2, #1
  CBNZ     x2, 1b
  
  // [32..47]: 0x0400,0000 - 0x05FF,FFFF (Fault)
  LDR      x0, =TT_S1_FAULT             // 表项模板
  ORR      x0, x0, #0x04000000          // 用基物理地址 'OR' 模板
  MOV      x2, #16
1:
  STR      x0, [x1], #8
  ADD      x0, x0, #0x200000            // 递增物理地址字段
  SUB      x2, x2, #1
  CBNZ     x2, 1b
  
  // [48..63]: 0x0600,0000 - 0x07FF,FFFF (Trusted DRAM)
  LDR      x0, =TT_S1_NORMAL_WBWA       // 表项模板
  ORR      x0, x0, #TT_S1_INNER_SHARED  // 用内部可共享属性 'OR'
                                        // RW
  ORR      x0, x0, #0x06000000          // 用基物理地址 'OR' 模板
  MOV      x2, #16
1:
  STR      x0, [x1], #8
  ADD      x0, x0, #0x200000            // 递增物理地址字段
  SUB      x2, x2, #1
  CBNZ     x2, 1b
  
  // [64..127]: 0x0800,0000 - 0x0FFF,FFFF (Flash)
  LDR      x0, =TT_S1_NORMAL_WBWA       // 表项模板
  ORR      x0, x0, #TT_S1_INNER_SHARED  // 用内部可共享属性 'OR'
  ORR      x0, x0, #TT_S1_PRIV_RO       // 'OR' 入 Read-only
  ORR      x0, x0, #TT_S1_NS            // 用 NS==1 'OR'
  ORR      x0, x0, #TT_S1_PXN           // 用 XN==1 'OR'
  ORR      x0, x0, #0x08000000          // 用基物理地址 'OR' 模板
  MOV      x2, #64
1:
  STR      x0, [x1], #8
  ADD      x0, x0, #0x200000            // 递增物理地址字段
  SUB      x2, x2, #1
  CBNZ     x2, 1b
 
 // [128..511]: 0x1000,0000 - 0x3FFF,FFFF (Fault)
```

这个 L2 表的前 128 个表项覆盖第一个 1GB 的前 256MB。其余表项（[128..511]）保持为 Fault，对应 0x1000_0000 到 0x3FFF_FFFF 的未映射区域。

## 5. 小结

四个示例层层递进：从 EL3 单级平坦映射，到两级表实现更细粒度，再到 EL1 非安全机制（含 EL3→EL1 切换与 VMID 等细节），最后是一个接近真实的完整内存映射。掌握它们，你就能编写或修改自己的裸机 MMU 初始化序列。