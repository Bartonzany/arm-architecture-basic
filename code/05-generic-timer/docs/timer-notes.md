# Arm 通用定时器 —— 概念、实现与结果印证

本文档按《Arm 通用定时器（中文精读版）》章节组织，把**文段内容**、
**代码实现**、**执行结果**、**因果解释**串在一起。代码参考 ARM 官方
generic_timer 示例（`src/generic_timer.zip`），全程在 EL3（Secure 世界）运行，
用 CVAL 与 TVAL 两种方式各配一个定时器，走 FIQ 处理。

代码位置：`src/bare-metal/`（EL3 裸机，QEMU）。

运行方式：

```bash
make -C src/bare-metal run
# QEMU: -machine virt,secure=on,gic-version=2 -cpu cortex-a53 -kernel kernel.elf
```

---

## 一、通用定时器组成（§1）

**文段**：一个**系统计数器**（常开、固定频率递增、广播给所有核）+ 每个核
各自的**比较器定时器**。所有核共享同一份"时间视图"。

**代码**：本示例读系统计数器（`CNTPCT_EL0`/`CNTVCT_EL0`）并配两个定时器
（CNTPS、CNTP），印证"计数器广播 + 每核比较器"的模型。

---

## 二、计数与频率（§2.1）—— 已验证

### 文段

`CNTPCT_EL0` 报告当前系统计数值；`CNTFRQ_EL0` 报告系统计数频率——它**不是
硬件自动填**的，由固件在最高 EL（EL3）写入，所有 EL 可读。

### 代码

`boot.S` 在 EL3 写入频率；`main.c` 读回验证：

```asm
ldr x2, =62500000        ; QEMU virt 默认 62.5MHz
msr cntfrq_el0, x2       ; 固件在 EL3 填频率
```

```c
__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
__asm__ volatile("mrs %0, cntpct_el0" : "=r"(cnt2));
__asm__ volatile("mrs %0, cntvct_el0" : "=r"(vct));
```

### 结果

```text
[REG]   CNTFRQ_EL0 = 0x0000000003b9aca0    ; 62500000 = 62.5MHz
[REG]   CNTPCT_EL0 = 0x000000000001f56c
[REG]   CNTVCT_EL0 = 0x000000000001f6fa
```

### 为什么

`0x3b9aca0` = 62,500,000 正是 QEMU virt 的系统计数频率；`CNTVCT_EL0`（虚拟
计数）略大于 `CNTPCT_EL0`（物理计数）是两条读指令先后执行、计数仍在走的缘故，
二者无 EL2 偏移时相等（见 §六）。

---

## 三、定时器寄存器与访问（§2.2/§2.3）—— 已验证

### 文段

每个定时器三个寄存器：`<timer>_CTL` / `_CVAL` / `_TVAL`。前缀标识定时器：

| 定时器 | 寄存器前缀 | INTID |
| --- | --- | --- |
| EL1 物理定时器 | CNTP | 30 |
| 安全 EL1 物理定时器（EL3 物理定时器） | CNTPS | 29 |

安全物理定时器 `CNTPS` 的访问：从 S.EL1 的访问由 `SCR_EL3.ST` 控制；本示例
全程在 EL3，直接访问 `CNTPS_CVAL_EL1` / `CNTPS_CTL_EL1`。

### 代码（`boot.S`）

```asm
mov x2, #((1<<11)|(1<<10)|(1<<3)|(1<<2)|(1<<1))
msr scr_el3, x2           ; ST=1（S.EL1 可访问 CNTPS）、FIQ/IRQ 路由到 EL3
```

---

## 四、CVAL vs TVAL（§2.4）—— 已验证

### 文段

配置同一个定时器的两种方式：**TVAL**（32 位）写"X tick 后触发"，处理器内部
读当前计数 + 写入值填入 CVAL；**CVAL**（64 位）写绝对比较值。读回 TVAL 会
递减（有符号）。

### 代码

```c
__asm__ volatile("mrs %0, cntpct_el0" : "=r"(cnt1));
__asm__ volatile("msr cntp_tval_el0, %0" : : "r"(100000));  // 写 TVAL
__asm__ volatile("mrs %0, cntp_cval_el0" : "=r"(cval));      // 读 CVAL
__asm__ volatile("mrs %0, cntp_tval_el0" : "=r"(tval));      // 读回 TVAL
```

### 结果

```text
[INFO]  写 CNTP_TVAL_EL0 = 100000
[REG]   CNTP_CVAL_EL0 = 0x000000000003bd2f
[REG]   CNTP_TVAL_EL0 = 0x000000000001853f
```

### 为什么

写 TVAL=100000 后，硬件内部算 `CVAL = 计数 + TVAL`。读回的 CVAL 与写 TVAL 时
的计数之差略大于 100000（多出的部分是"写 TVAL → 读 CVAL"之间计数又走了几 tick），
印证 §2.4"CVAL 大约等于系统计数 + 1000，之所以是大约，是因为指令执行期间时间
仍在流逝"。读回 TVAL 递减，印证"TVAL 报告有符号值、递减到 0"。

---

## 五、生成中断（§2.5）—— 已验证

### 文段

定时器触发（`CVAL <= System Count`）时向 GIC 断言中断。中断**电平敏感**
（level-sensitive）：清除须置 IMASK / 清 ENABLE / 更新 TVAL 或 CVAL，否则反复
触发。参考的 generic_timer 示例里，安全物理定时器用 CVAL、非安全 EL1 物理
定时器用 TVAL，二者中断都配成 Group 0（FIQ）。

### 代码

```c
// 配置安全物理定时器 CNTPS：CVAL 设绝对触发时间
mrs  cntpct_el0, cnt1;
msr  cntps_cval_el1, cnt1 + 20000;
msr  cntps_ctl_el1, 1;                  // ENABLE=1

// 配置非安全物理定时器 CNTP：TVAL 设相对 X tick
msr  cntp_tval_el0, 40000;
msr  cntp_ctl_el0, 1;                   // ENABLE=1

void fiq_handler_c(void) {
    unsigned int iar = *GICC_IAR;       // 读 IAR 拿 INTID
    unsigned int id  = iar & 0x3FF;
    switch (id) {
        case 29: msr cntps_ctl_el1, xzr; break;   // 清 ENABLE 清电平中断
        case 30: msr cntp_ctl_el0,  xzr; break;
    }
    *GICC_EOIR = iar;                    // 写 EOIR 结束中断
}
```

> GIC 关键点：`GICC_CTLR.FIQEn(bit3)` 须置 1，否则 Group 0 中断会被当 IRQ 而非
> FIQ 发信号（默认 FIQEn=0）。

### 结果

```text
[INFO]  安全物理定时器 CNTPS 已使能（CVAL，INTID 29）
[INFO]  非安全物理定时器 CNTP 已使能（TVAL，INTID 30）
[FIQ]   Received INTID 29 (Secure Physical Timer)
[FIQ]   Received INTID 30 (Non-secure EL1 Physical Timer)
[OK]    两个定时器中断均验证完成（INTID 29 + 30）
```

### 为什么

`INTID 29/30` 正是 SBSA 定义的安全/非安全物理定时器 PPI；处理器读 IAR/写 EOIR
是 GIC 的中断确认/结束流程；写 `CTL=0`（清 ENABLE）是 §2.5 说的"清 ENABLE 清除
电平敏感中断"，否则会看到中断反复触发。

---

## 六、定时器虚拟化（§2.6）—— 概念

- 虚拟定时器与**虚拟计数**比较：`Virtual Count = Physical Count - CNTVOFF_EL2`；
- `CNTVOFF_EL2` 只能 EL2/EL3 访问，hypervisor 用它隐藏 VM 未被调度的时间。

> 本示例未设 `CNTVOFF_EL2`（偏移为 0），故 `CNTVCT_EL0 == CNTPCT_EL0`（见 §二）。

---

## 七、事件流与系统计数器（§2.7/§3）—— 概念

- **事件流**（§2.7）：WFE 低功耗等待时，定时器按 EVNTI/EVNTDIR 指定的位变化
  生成事件，保证唤醒时间有界；
- **缩放**（§3.2，Armv8.4-A+）：计数每 tick 递增 X（CNTSCR），内部 88 位定点数。

> 事件流需 `CNTKCTL_EL1`/`CNTKCTL_EL2` 配置，缩放需系统计数器（SoC 级）编程，
> 均未实现代码。

---

## 附：完整执行输出

```text
========== Arm 通用定时器验证 启动 ==========

CPU:              cortex-a53
Machine:          virt (secure=on, gic-version=2)
Timers:           CNTPS(CVAL) + CNTP(TVAL)

[STEP 1/3] 引导（EL3 设频率 + GIC 初始化）
[REG]   CNTFRQ_EL0 = 0x0000000003b9aca0
[INFO]  GIC 已配置（INTID 29/30 -> Group0/FIQ，路由到 EL3）

[STEP 2/3] 系统计数器与 CVAL/TVAL
[REG]   CNTPCT_EL0 = 0x000000000001f56c
[REG]   CNTVCT_EL0 = 0x000000000001f6fa
[INFO]  两次 CNTPCT_EL0 差值为非零（计数递增）
[INFO]  写 CNTP_TVAL_EL0 = 100000
[REG]   CNTP_CVAL_EL0 = 0x000000000003bd2f
[REG]   CNTP_TVAL_EL0 = 0x000000000001853f

[STEP 3/3] 定时器中断（CNTPS 用 CVAL + CNTP 用 TVAL）
[INFO]  安全物理定时器 CNTPS 已使能（CVAL，INTID 29）
[INFO]  非安全物理定时器 CNTP 已使能（TVAL，INTID 30）
[FIQ]   Received INTID 29 (Secure Physical Timer)
[FIQ]   Received INTID 30 (Non-secure EL1 Physical Timer)
[OK]    两个定时器中断均验证完成（INTID 29 + 30）

========== 全部验证完成 (CNTPS CVAL + CNTP TVAL) ==========
```

（寄存器地址/计数值因运行时刻不同会浮动，属正常。日志支持 `NO_COLOR=1` 关闭颜色。）