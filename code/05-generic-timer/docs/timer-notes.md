# Arm 通用定时器 —— 概念、实现与结果印证

本文档按《Arm 通用定时器（中文精读版）》章节组织，把**文段内容**、
**代码实现**、**执行结果**、**因果解释**串在一起。核心机制（频率、计数、
CVAL/TVAL、中断）已在 EL1 裸机验证；系统计数器缩放、事件流等为概念说明。

代码位置：`src/bare-metal/`（EL1 裸机，QEMU）。

---

## 一、通用定时器组成（§1）

**文段**：一个**系统计数器**（常开、固定频率递增、广播给所有核）+
每个核各自的**比较器定时器**。所有核共享同一份时间视图。

> 纯概念，无代码（系统计数器是 SoC 级设备，处理器通过寄存器读它）。

## 二、计数与频率（§2.1）—— 已验证

### 文段

`CNTPCT_EL0` 报告当前系统计数值；`CNTFRQ_EL0` 报告系统计数频率（硬件不自动
填，由固件在 EL3 写入，所有 EL 可读）。

### 代码

```c
__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
__asm__ volatile("mrs %0, cntpct_el0" : "=r"(cnt1));
__asm__ volatile("mrs %0, cntpct_el0" : "=r"(cnt2));
__asm__ volatile("mrs %0, cntvct_el0" : "=r"(vct));
```

### 结果

```text
[REG ] CNTFRQ_EL0 = 0x0000000003b9aca0   ; 62500000 = 62.5MHz
[REG ] CNTPCT_EL0 = 0x000000000000be43
[REG ] CNTVCT_EL0 = 0x000000000000bf30   ; 无 EL2 时 = 物理计数
[INFO] 两次 CNTPCT_EL0 差值 = 00000106（计数在递增）
```

### 为什么

`0x3b9aca0` = 62,500,000 正是 QEMU virt 的系统计数频率；两次读 `CNTPCT_EL0`
差值非零，说明系统计数器在持续递增；`CNTVCT_EL0`（虚拟计数）略大于
`CNTPCT_EL0`（物理计数）是读指令先后导致的，两者无 EL2 偏移时相等——
印证 §2.6 的"虚拟计数 = 物理计数 - 偏移，无 EL2 时偏移为 0"。

## 三、CVAL vs TVAL（§2.4）—— 已验证

### 文段

两种配置同一个定时器的方式：**TVAL**（32 位）写"X tick 后触发"，处理器内部
读取当前计数 + 写入值填入 CVAL；**CVAL**（64 位）写绝对比较值。读回 TVAL
会看到它递减到 0（有符号）。

### 代码

```c
__asm__ volatile("mrs %0, cntpct_el0" : "=r"(cnt1));
__asm__ volatile("msr cntp_tval_el0, %0" : : "r"(100000));  /* 写 TVAL */
__asm__ volatile("mrs %0, cntp_cval_el0" : "=r"(cval));      /* 读 CVAL */
__asm__ volatile("mrs %0, cntp_tval_el0" : "=r"(tval));      /* 读回 TVAL */
```

### 结果

```text
[INFO] 写 CNTP_TVAL_EL0 = 100000
[REG ] CNTP_CVAL_EL0 = 0x0000000000029abb
[REG ] CNTP_TVAL_EL0 = 0x0000000000018446
[INFO] CVAL - 原计数 = 00018878（≈ 100000，因执行期间计数仍在走）
```

### 为什么

写 TVAL=100000（0x186A0）后，硬件内部执行 `CVAL = 计数 + TVAL`。读回的
CVAL 与原计数之差 = 0x18878 = 100,472 ≈ 100,000（多了 472 tick 是"写 TVAL →
读 CVAL"之间计数又走了 472 步），印证 §2.4 说的"CVAL 大约等于系统计数 + 1000，
之所以是大约，是因为指令执行期间时间仍在流逝"。读回 TVAL = 0x18446（递减），
印证"TVAL 报告有符号值、递减到 0"。

## 四、生成中断（§2.5）—— 已验证

### 文段

定时器触发（CVAL <= System Count）时向 GIC 断言中断（EL1 物理定时器 INTID 30，
SBSA 定义）。中断**电平敏感**：清除须置 IMASK/清 ENABLE/更新 TVAL 或 CVAL，
否则会反复触发。

### 代码

```c
__asm__ volatile("msr cntp_tval_el0, %0" : : "r"(0x8000));   /* 较短倒计时 */
__asm__ volatile("msr cntp_ctl_el0, %0" : : "r"(1));         /* ENABLE=1, IMASK=0 */
__asm__ volatile("msr daifclr, #2");                         /* 解除 IRQ 屏蔽 */
while (!g_irq_ok) { /* 忙循环 */ }

void irq_handler_c(void) {
    unsigned int iar = *GICC_IAR;       /* 中断确认 */
    *GICC_EOIR = iar;                    /* 中断结束 */
    __asm__ volatile("msr cntp_ctl_el0, xzr");  /* 清 ENABLE，避免电平重触发 */
    g_irq_ok = 1;
}
```

### 结果

```text
[INFO] 定时器已使能（ENABLE=1, IMASK=0），等待 IRQ ...
[IRQ ] 定时器到期，GICC_IAR=0x01e INTID=1e（EL1 物理定时器）
[ OK ] 定时器中断验证完成（INTID 30）
```

### 为什么

`GICC_IAR=0x01e`（INTID=30）正是 SBSA 定义的 EL1 物理定时器 PPI；处理器里
读 IAR/写 EOI 是 GIC 中断确认/结束流程，写 `CNTP_CTL_EL0=0`（清 ENABLE）是
§2.5 说的"清 ENABLE 清除电平敏感中断"，否则会看到 IRQ 反复触发。

---

## 五、定时器虚拟化（§2.6）—— 概念

- 虚拟定时器与**虚拟计数**比较：`Virtual Count = Physical Count - CNTVOFF_EL2`；
- `CNTVOFF_EL2` 只能 EL2/EL3 访问，hypervisor 用它隐藏 VM 未被调度的时间。

> 已在虚拟化指南（04）的 EL2 示例覆盖陷阱；本目录 EL1 下偏移恒为 0，
> 故 `CNTVCT_EL0 == CNTPCT_EL0`（见 §二结果）。

## 六、系统计数器与事件流（§2.7/§3）—— 概念

- **缩放**（§3.2，Armv8.4-A+）：计数每 tick 递增 X（CNTSCR），内部 88 位定点数
  （64 整数 + 24 小数）；
- **事件流**（§2.7）：WFE 低功耗等待时，定时器按 EVNTI/EVNTDIR 指定的位变化
  生成事件，保证唤醒时间有界。

> 均需系统计数器（SoC 级）编程，处理器侧无寄存器，未实现代码。

## 附：完整执行输出

```text
[====]AArch64 通用定时器裸机验证
[REG ] CNTFRQ_EL0 = 0x0000000003b9aca0
[REG ] CNTPCT_EL0 = 0x000000000000be43
[REG ] CNTVCT_EL0 = 0x000000000000bf30
[INFO] 两次 CNTPCT_EL0 差值 = 00000106（计数在递增）
[INFO] 写 CNTP_TVAL_EL0 = 100000
[REG ] CNTP_CVAL_EL0 = 0x0000000000029abb
[REG ] CNTP_TVAL_EL0 = 0x0000000000018446
[INFO] CVAL - 原计数 = 00018878（≈ 100000，因执行期间计数仍在走）
[INFO] 定时器已使能（ENABLE=1, IMASK=0），等待 IRQ ...
[IRQ ] 定时器到期，GICC_IAR=0x01e INTID=1e（EL1 物理定时器）
[ OK ] 定时器中断验证完成（INTID 30）
[EXIT]semihosting 正常退出
```