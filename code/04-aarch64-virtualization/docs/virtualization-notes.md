# AArch64 虚拟化 —— 概念、实现与结果印证

本文档按《AArch64 虚拟化指南（中文精读版）》章节组织，把**文段内容**、
**代码实现**、**执行结果**、**因果解释**串在一起。虚拟化全部是 EL2/EL3
特权级内容，可验证的仅 **§7 陷阱机制**（已用 EL2 裸机示例验证），其余为
概念说明。

代码位置：`src/bare-metal/`（EL2 裸机示例）。

---

## 一、虚拟化基础（§1–§4）

- **§1 为什么重要**：隔离、高可用、负载均衡、沙箱。
- **§2 两类 hypervisor**：Type 1（独立式，Xen，直接跑硬件）、Type 2（托管式，
  KVM，跑在 Host OS 上）。
- **§3 全/半虚拟化**：全虚拟化模拟真实机器；半虚拟化让 Guest 感知虚拟化
  （Virtio 设备）。
- **§4 VM vs vCPU**：一个 VM 含多个 vCPU；虚拟中断面向特定 vCPU。

> 均纯概念，无代码。

## 二、虚拟化三抓手（§5）

运行在 EL2 或更高层的软件，有三类虚拟化控制：

1. **第二阶段（stage 2）翻译**（§6）；
2. **对 EL1/0 指令和寄存器访问的捕获（trap）**（§7）；
3. **虚拟异常生成**（§8）。

其中第 2 点（trap）在裸机 EL2 可验证，见下节。

---

## 三、陷阱与模拟（§7）—— 已验证

### 文段

架构允许 hypervisor 设置**陷阱（trap）**：一旦设置，执行某个原本合法的动作
就会产生异常、进入更高 EL。以 `WFI` 为例：若断言 `HCR_EL2.TWI=1`，则在 EL0/EL1
执行 `WFI` 会改为产生进入 EL2 的异常。VM 的 Guest OS 常在空闲循环执行 WFI，
hypervisor 捕获它转而调度另一个 vCPU。

### 代码实现

**步 1：EL2 启动，配置陷阱**（`boot.S`）：

```asm
_start:
    msr     spsel, #1          ; 选 SP_EL2
    mov     sp, x0             ; 设 hypervisor 栈
    ldr     x0, =vector_table_el2
    msr     vbar_el2, x0       ; EL2 向量表
    ldr     x0, =0x80002000    ; HCR_EL2: bit31 RW=1, bit13 TWI=1
    msr     hcr_el2, x0        ; ← TWI=1：EL0/1 执行 WFI 将陷阱到 EL2
    ldr     x1, =_stack_top
    msr     sp_el1, x1         ; guest 栈
    mov     x2, #0x3c5
    msr     spsr_el2, x2       ; 目标 EL1h
    ldr     x3, =guest_entry
    msr     elr_el2, x3        ; guest 入口
    eret                       ; 切入 EL1
```

**步 2：guest 执行 WFI**（`guest.c`）：

```c
__asm__ volatile("wfi");   // 因 TWI=1，被陷阱到 EL2
```

**步 3：EL2 陷阱处理器**（`vector_table.S` +0x400 低 EL 同步）：

```asm
wfi_trap_handler:
        save_all
        mrs     x0, esr_el2     ; 异常原因
        mrs     x1, elr_el2     ; guest 的 WFI 地址
        bl      el2_handler_c   ; 打印 + 模拟 + 跳过 WFI
        restore_all
        eret
```

```c
void el2_handler_c(unsigned long esr, unsigned long elr) {
    unsigned int ec = (esr >> 26) & 0x3F;
    /* 打印 EC、ELR_EL2 */
    __asm__ volatile("msr elr_el2, %0" : : "r"(elr + 4));  // 模拟：跳过 WFI
}
```

### 执行结果

```text
[====]AArch64 虚拟化裸机验证（陷阱机制）
[REG ] HCR_EL2 = 0x0000000080002000
[INFO] hypervisor(EL2) 已配置 TWI，ERET 切入 EL1
[GST ] CurrentEL=1（1=EL1，guest）
[GST ]guest 执行 WFI（将被 TWI 陷阱到 EL2）...
[EL2 ] EC=0x01 (Trapped WFI/WFE)
[REG ] ELR_EL2 = 0x00000000400012f0
[ OK ] WFI 被 trap 到 EL2 后返回，guest 继续执行
[ OK ] 陷阱机制验证完成
```

### 为什么结果是这样

1. `HCR_EL2=0x80002000`：bit13(TWI) 就是陷阱开关，bit31(RW) 声明 EL1 用 AArch64；
2. `CurrentEL=1`：`eret` 把 PE 从 EL2 降到 EL1，guest 正式运行；
3. `WFI` 触发后，EL2 陷阱处理器的 `EC=0x01`（"Trapped WFI/WFE"）正是 TWI 陷阱
   的异常类别，写入 `ESR_EL2`——印证"原本合法的 WFI 变为异常进入 EL2"；
4. `ELR_EL2` 指向 guest 的 WFI 地址，处理器把它 +4 跳过 WFI 再 `eret`，
   于是 guest 打印"WFI 被 trap 后返回"——完整走通"陷阱→模拟→返回"闭环。

---

## 四、第二阶段翻译（§6）—— 概念

- **stage 2**：hypervisor 控制 VM 看得到的内存视图（VA → IPA → PA 两段翻译）。
- **VMID**：给 TLB 表项打标签区分 VM（存 `VTTBR_EL2`）。
- **ASID**：给应用打标签；真正归属由 "ASID + VMID" 组合决定。
- **属性组合**：stage1/stage2 属性取更严格者；`HCR_EL2.CD/DC/FWB` 可覆盖。
- **MMIO 模拟**：虚拟外设 stage2 表项标故障，访问触发 stage2 故障，hypervisor
  据 `ESR_EL2` + `HPFAR_EL2`（IPA）模拟。
- **SMMU/IOMMU**：为 DMA 等主机提供 stage2 翻译，隔离 + 统一地址视图。

> 纯概念/需 EL2 编程 stage2 页表，未实现代码（复杂）。

## 五、异常虚拟化（§8）—— 概念

- 虚拟中断 vIRQ/vFIQ/vSError，**只能在 EL0/EL1 发信号**；
- two 生成方式：`HCR_EL2.VI/VF/VSE` 位（内核生成）或 GIC 虚拟 CPU 接口（硬件）。

## 六、通用定时器虚拟化（§9）—— 概念

- EL1 物理定时器 = 墙上时钟；EL1 虚拟定时器 = 虚拟计数（物理计数减偏移），
  hypervisor 用偏移隐藏"vCPU 未被调度的时间"。

## 七、VHE（§10）—— 概念

- `HCR_EL2.E2H` 让 Host OS 跑在 EL2 时拥有 EL0/2 两个地址空间 + ASID；
- `HCR_EL2.TGE` 决定 EL0 属于 Host 还是 Guest；
- E2H 使 `_EL1` 寄存器访问重定向到 `_EL2` 等价寄存器。

## 八、嵌套虚拟化与安全虚拟化（§11–§12）—— 概念

- **嵌套**：Guest hypervisor 在 VM 里，`HCR_EL2.NV/NV1/NV2` + `VNCR_EL2`
  捕获/重定向其对 EL2 寄存器的访问。
- **安全虚拟化**：`SCR_EL3.EEL2` 启用安全 EL2；安全状态有安全/非安全两个 IPA 空间。

## 九、虚拟化代价（§13）—— 概念

- 上下文切换保存/恢复 31 个 GPR + 32 个 128 位向量寄存器 + 2 个 SP，
  用 LDP/STP 约 33 条指令（见 02-a64-instruction-set 的 LDP/STP demo）。