# Arm GICv3 —— 概念、实现与结果印证

本文档按《22 - GICv3 和 v4 概述（中文精读版）》章节组织，把**文段内容**、
**代码实现**、**执行结果**、**因果解释**串在一起。核心机制（三大编程接口、
初始化流程、IAR/EOI、运行优先级、SGI）已在 **EL1 裸机（QEMU `gic-version=3`）**
真实验证。

代码位置：`src/bare-metal/`。运行环境：QEMU `-machine virt,gic-version=3`。

> 本文档聚焦 GICv3 与 GICv2 的最大差异：**CPU 接口从内存映射寄存器改为
> `ICC_*_ELn` 系统寄存器**，用 `MRS`/`MSR` 访问。反汇编里能看到一条条
> `msr icc_sre_el1` / `mrs icc_iar1_el1` 这样的指令。

---

## 一、三大编程接口（§7）—— 已验证

**文段**：GICv3 的寄存器分三块——分发器 `GICD_*`（配 SPI）、再分发器 `GICR_*`
（每核一份，配 SGI/PPI）、CPU 接口 `ICC_*_ELn`（系统寄存器，处理中断）。

**QEMU virt 内存映射**：

```text
GICD 分发器   : 0x08000000
GICR 再分发器 : 0x080A0000  （PE0 的 RD 帧 @0x080A0000，SGI 帧 @0x080B0000）
CPU 接口       : ICC_*_ELn 系统寄存器（不是内存映射）
```

**代码**（识别 GIC）：

```c
log_reg("GICD_TYPER", GICD_TYPER);   // 类型信息
log_reg("GICD_IIDR",  GICD_IIDR);    // 实现标识
```

**结果**：

```text
[REG ] GICD_TYPER = 0x037a0008
[REG ] GICD_IIDR = 0x0000043b
```

**为什么**：`GICD_IIDR` 的 Implementer 字段 = `0x43B`，正是 Arm 的 JEP-106
厂商编码。`GICD_TYPER` 中 SecurityExtn 位（bit 10）为 1，说明该 GIC 支持
两种安全状态（对应 §6 的 DS==0 场景）。

## 二、全局设置 GICD_CTLR（§8.1）—— 已验证

**文段**：`GICD_CTLR` 要开亲和性路由（ARE）+ 各分组使能。

**代码**：

```c
GICD_CTLR = (1u << 0)   /* EnableGrp0   */
          | (1u << 1)   /* EnableGrp1S  */
          | (1u << 2)   /* EnableGrp1NS */
          | (1u << 4)   /* ARE_NS：非安全态亲和性路由 */
          | (1u << 5);  /* ARE_S ：安全态亲和性路由 */
```

**为什么**：ARE 位是 GICv3 与「传统操作」的分水岭（§12）：ARE=1 才走 GICv3
亲和性路由模式。三个 Enable 位分别放行 Group 0 / Secure Group 1 / Non-secure
Group 1 的分发（§6 安全模型）。

## 三、唤醒再分发器 GICR_WAKER（§8.2）—— 已验证

**文段**：复位后所有 PE 都被当作离线；离线时中断只会变成 wake-request 信号。
要标记在线，须清 `ProcessorSleep` 并轮询 `ChildrenAsleep`。

**代码**：

```c
GICR_WAKER = GICR_WAKER & ~0x2u;   // ProcessorSleep=0
while (GICR_WAKER & 0x4u) { }      // 等 ChildrenAsleep==0
```

**为什么**：不先唤醒再分发器就配 CPU 接口，行为 UNPREDICTABLE（§8.2 原文强调）。

## 四、CPU 接口系统寄存器 ICC_*（§8.2）—— 已验证

**文段**：启用系统寄存器访问（`ICC_SRE_EL1.SRE`）、设优先级掩码 `ICC_PMR_EL1`、
二进制点 `ICC_BPR1_EL1`、分组使能 `ICC_IGRPEN1_EL1`、EOI 模式 `ICC_CTLR_EL1`。

**代码**：

```c
wr_icc_sre(0x1);      // SRE=1：启用系统寄存器访问
wr_icc_pmr(0xFF);     // 优先级掩码：允许所有优先级
wr_icc_bpr1(0x0);     // 二进制点：全部 8 位都是组优先级
wr_icc_igrpen1(0x1);  // 使能 Group 1 中断信号
wr_icc_ctlr(0x0);     // EOImode=0：EOI 一步完成
```

**反汇编**（GICv3 的关键特征——系统寄存器访问）：

```text
40001434:  msr  icc_sre_el1, x5
4000143c:  msr  icc_pmr_el1, x0
40001444:  msr  icc_bpr1_el1, x0
40001448:  msr  icc_igrpen1_el1, x5
4000144c:  msr  icc_ctlr_el1, x0
```

**为什么**：这是 GICv3 相对 GICv2 的核心变化——不再用 `GICC_*` 内存映射寄存器，
而用 `MRS`/`MSR` 直接访问系统寄存器，省去了 MMIO 开销。

## 五、每 INTID 配置（§8.3）—— 已验证

**文段**：每个 INTID 要配优先级、分组、触发方式、使能。PPI/SGI 用 `GICR_*`。

**代码**：

```c
unsigned int grp = (1u << TIMER_IRQ_ID) | (1u << SGI_ID);
GICR_IGROUPR0  |= grp;                 // INTID 30/1 -> Group 1
GICR_IGRPMODR0 |= grp;                 // Group 1 -> Non-secure
GICR_IPRIORITY_BYTE(TIMER_IRQ_ID) = 0xA0;  // 定时器优先级 0xA0
GICR_IPRIORITY_BYTE(SGI_ID)       = 0x20;  // SGI 优先级 0x20
GICR_ISENABLER0 = grp;                 // 使能 INTID 30 与 1
```

**为什么**：`IGROUP=1 + IGRPMOD=1` 编码成 Non-secure Group 1，这样在非安全
EL1 下两个中断都以 IRQ 到达（§6 路由表）；优先级 0x00 最高、0xFF 最低（§8.3）。
后文 RPR 观察到的 `0xA0`、`0x20` 正是这里写进去的优先级。

## 六、接受中断：读 ICC_IAR1_EL1（§9.2）—— 已验证

**文段**：进处理器时软件不知道来的是谁，读 IAR 拿 INTID；读 IAR 会推进状态机。

**代码**：

```c
iar = rd_icc_iar1();            // mrs x?, icc_iar1_el1
id  = iar & 0xFFFFFFu;
```

**结果**（定时器与 SGI 各一次）：

```text
[IRQ ] ICC_IAR1_EL1=0x0000001e INTID=0x1e  ICC_RPR_EL1=0xa0
[IRQ ] ICC_IAR1_EL1=0x00000001 INTID=0x01  ICC_RPR_EL1=0x20
```

**为什么**：IAR1 返回的 INTID 分别是 0x1e（30，EL1 物理定时器 PPI）和 0x01
（SGI）。INTID 30 正是 SBSA 定义的物理定时器 PPI（§4.1 的 PPI 例子）。

## 七、运行优先级 ICC_RPR_EL1（§9.4）—— 已验证

**文段**：确认中断后，运行优先级 = 该中断优先级；写 EOI 后恢复原值。空闲时
运行优先级是 0xFF。

**结果**：

```text
[REG ] ICC_RPR_EL1 = 0x000000ff        ; 空闲：idle 0xFF
[IRQ ] ... INTID=0x1e  ICC_RPR_EL1=0xa0 ; 确认定时器(优先级 0xA0)后 = 0xA0
[REG ] EOI 后 ICC_RPR_EL1 = 0x000000ff  ; 写 EOI 后回 idle 0xFF
[IRQ ] ... INTID=0x01  ICC_RPR_EL1=0x20 ; 确认 SGI(优先级 0x20)后 = 0x20
```

**为什么**：运行优先级随「确认中断」抬升到该中断的优先级、随「写 EOI」回落，
与 §9.4 的图 6-3 完全对应。RPR 是抢占判定的依据（§9.1 第 5 项检查）。

## 八、中断结束：写 ICC_EOIR1_EL1（§9.5）—— 已验证

**文段**：`EOImode=0` 时写 EOIR 一步完成「优先级下降 + 撤销激活」。

**代码**：

```c
wr_icc_eoir1(iar);                 // msr icc_eoir1_el1, x?
```

**反汇编**：

```text
40001364:  msr  icc_eoir1_el1, x4
```

**为什么**：本例设 `ICC_CTLR_EL1=0`（EOImode=0），故只写 EOIR 就把运行优先级
降回（见上一节 RPR 回 0xFF）同时把中断从 Active 撤销激活到 Inactive。EOImode=1
时还要再写 `ICC_DIR_EL1` 才撤销激活（§9.5，虚拟化常用）。

## 九、查询最高优先级挂起中断 ICC_HPPIR1_EL1（§9.6）—— 已验证

**文段**：`ICC_HPPIR1_EL1` 报告本 PE 最高优先级挂起中断的 INTID。

**结果**：

```text
[REG ] ICC_HPPIR1_EL1 = 0x000003ff
```

**为什么**：0x3FF = 1023，即 §6.2 表中的「伪中断（spurious）」——没有足够
优先级的挂起中断。空闲时刻读 HPPIR1 得到 1023，印证「无挂起中断」这一语义。

## 十、给自己发 SGI：ICC_SGI1R_EL1（§10）—— 已验证

**文段**：SGI 由软件写 SGI 寄存器生成；`IRM=0` 时按 `aff3.aff2.aff1.<TargetList>`
定向（可含自己）；SGI 恒为边沿触发。

**代码**：

```c
wr_icc_sgi1r((0ul << 48)                   /* Aff3 = 0        */
           | (0ul << 40)                   /* IRM  = 0        */
           | (0ul << 32)                   /* Aff2 = 0        */
           | ((unsigned long)SGI_ID << 24) /* INTID = 1       */
           | (0ul << 16)                   /* Aff1 = 0        */
           | (1ul << 0));                  /* TargetList aff0=0 → 自己 */
```

**结果**：

```text
[INFO] 写 ICC_SGI1R_EL1 给自己发 SGI（INTID 1）...
[IRQ ] ICC_IAR1_EL1=0x00000001 INTID=0x01  ICC_RPR_EL1=0x20
[ OK ] SGI 处理完成（读 IAR1 -> INTID 1 -> 写 EOIR1）
```

**为什么**：单核 QEMU 的 MPIDR 亲和性为 0.0.0.0，故 `Aff3=Aff2=Aff1=0` 且
`TargetList` 第 0 位置 1 就命中自己——验证了「PE 能给自己发 SGI」（§11 自测
Q6）。SGI 是边沿触发（§4.2），处理里无需像定时器那样清源，直接 EOI 即可。

## 十一、GICv2 vs GICv3 的差异（对照）—— 概念

| 项 | GICv2（对照 `code/05-generic-timer`） | GICv3（本目录） |
| --- | --- | --- |
| CPU 接口访问 | 内存映射 `GICC_*`（0x08010000） | 系统寄存器 `ICC_*_ELn`（MRS/MSR） |
| 使能系统寄存器 | 无 | 先写 `ICC_SRE_EL1.SRE=1` |
| 中断结束 | 写 `GICC_EOIR` | 写 `ICC_EOIR1_EL1`（EOImode 可拆分） |
| SGI 存放 | 按「源 + 目标」双重 banked | 只按目标 banked（同 INTID 只一份） |

## 附：完整执行输出

```text
[====]AArch64 GICv3 裸机验证
[REG ] GICD_TYPER = 0x037a0008
[REG ] GICD_IIDR = 0x0000043b
[INFO] GICv3 初始化完成（GICD_CTLR/GICR_WAKER/ICC_SRE·PMR·BPR1·IGRPEN1·CTLR）
[REG ] ICC_HPPIR1_EL1 = 0x000003ff
[REG ] ICC_RPR_EL1 = 0x000000ff
[INFO] 使能 EL1 物理定时器（INTID 30, 优先级 0xA0）...
[IRQ ] ICC_IAR1_EL1=0x0000001e INTID=0x1e  ICC_RPR_EL1=0xa0
[ OK ] 定时器 PPI 处理完成（读 IAR1 -> INTID 0x1e -> 写 EOIR1）
[REG ] EOI 后 ICC_RPR_EL1 = 0x000000ff
[ OK ] 运行优先级已回 idle 0xFF
[INFO] 写 ICC_SGI1R_EL1 给自己发 SGI（INTID 1）...
[IRQ ] ICC_IAR1_EL1=0x00000001 INTID=0x01  ICC_RPR_EL1=0x20
[ OK ] SGI 处理完成（读 IAR1 -> INTID 1 -> 写 EOIR1）
[EXIT]semihosting 正常退出
```