# Arm SMMUv3 —— 概念、实现与结果印证

本文档按《26 - SMMU 软件指南（中文精读版）》章节组织，把**文段内容**、
**代码实现**、**执行结果**、**因果解释**串在一起。核心机制（寄存器识别、
流表、命令/事件队列、最小配置七步）已在 **EL1 裸机（QEMU
`-machine virt,iommu=smmuv3`）** 真实验证。

代码位置：`src/`。运行环境：QEMU `iommu=smmuv3`，SMMUv3 映射在 `0x09050000`。

> 本文档聚焦 SMMUv3 的**编程模型**：配置大部分在内存结构（流表 / 命令队列 /
> 事件队列）里，寄存器只存「结构在哪、多大」。反汇编里能看到对
> `0x09050000+offset` 的 MMIO 读写与 §4.11 要求的 `dsb sy`。

---

## 一、SMMU 是什么（§2）—— 概念

**文段**：SMMU 给 I/O 设备做 MMU 的活——把 DMA 请求的地址在进入互连前转换好，
并提供**转换 / 保护 / 隔离**。区分设备靠 StreamID。

> 纯概念：SMMU 是系统级设备，本目录只验证它的编程接口，不接真实 DMA 设备。

## 二、识别 SMMU（§4.1）—— 已验证

**文段**：`SMMU_IDR*` 报告已实现特性；寄存器占据两个连续 64K 页。

**代码**：

```c
log_reg("SMMU_IDR0", SMMU_IDR0);   // 0x09050000 + 0x0000
log_reg("SMMU_IDR1", SMMU_IDR1);   // 0x09050000 + 0x0004
log_reg("SMMU_IDR3", SMMU_IDR3);
log_reg("SMMU_IDR5", SMMU_IDR5);
```

**结果**：

```text
[REG ] SMMU_IDR0 = 0x0d44101a
[REG ] SMMU_IDR1 = 0x02730010
[REG ] SMMU_IDR3 = 0x00001404
[REG ] SMMU_IDR5 = 0x00000074
```

**为什么**：QEMU 的 SMMUv3 是对 Arm MMU-600 的建模（其 Primecell ID 寄存器即
MMU600 p1）。可确认的关键字段：`IDR1.SIDSIZE=16`（支持 16 位 StreamID，正好
对应 PCIe RequesterID 宽度，§5.3）；`IDR5.OAS=4`（44 位输出地址大小）。

## 三、流表（§4.2）—— 已验证

**文段**：流表由 `STRTAB_BASE` 指向，StreamID 索引出 STE；线性流表是连续 STE 数组。

**代码**：

```c
SMMU_WR32(0x0088, 0x0);            // STRTAB_BASE_CFG：FMT=0 线性, LOG2SIZE=0(2 个 STE)
SMMU_WR32(0x0080, (u32)(u64)strtab); // STRTAB_BASE 低 32 位（64 字节对齐）
SMMU_WR32(0x0084, 0);              // 高 32 位
dsb_sy();                          // §4.11：确保 SMMU 观察到写数据
```

**反汇编**（MMIO 写 + DSB 屏障）：

```text
400012cc:  mov  x2, #0x9050000     ; SMMU 基址
400012dc:  ldr  w1, [x5]           ; 读 IDR0
40001324:  str  wzr, [x5, #132]    ; 写 STRTAB_BASE_CFG(0x88)
40001338:  dsb  sy                 ; §4.11 步骤 1 的 DSB
```

**为什么**：`STRTAB_BASE` 是 64 位寄存器，分两次 32 位写（低字偏移 0x80、高字
0x84）。流表在 `.bss` 已清零，即所有 `STE.V=0`（无效），防止未初始化内存被
误读为有效配置——正是 §4.11 步骤 1 的要求。

## 四、命令队列 + 事件队列（§4.8 / §4.9）—— 已验证

**文段**：命令队列是内存里的循环队列，软件写命令、更新 `CMDQ_PROD`；SMMU 消费、
更新 `CMDQ_CONS`。事件队列反之（SMMU 是生产者）。`Q_BASE` 寄存器低 5 位是
LOG2SIZE（队列项数 = 2^(LOG2SIZE+1)）。

**代码**：

```c
SMMU_WR32(0x0090, (u32)(u64)cmdq | 4u);   // CMDQ_BASE 低32: ADDR | LOG2SIZE=4(32项)
SMMU_WR32(0x0094, 0);                     // 高32
SMMU_WR32(0x0098, 0);                     // CMDQ_PROD=0
SMMU_WR32(0x009C, 0);                     // CMDQ_CONS=0
/* 事件队列同理（0x00A0/0x00A4/0x00A8/0x00AC）*/
```

**结果**（提交 2 条命令后）：

```text
[INFO] 已提交 CMD_TLBI_NSNH_ALL + CMD_SYNC，等待消费 ...
[REG ] 消费后 CMDQ_CONS = 0x00000002（RD=02）
[ OK ] CMD_SYNC 已消费，此前命令效果可见
```

**为什么**：软件把 `CMDQ_PROD.WR` 从 0 写到 2（告知 SMMU 有 2 条新命令），SMMU
逐条消费并把 `CMDQ_CONS.RD` 推进到 2。这正是 §4.8 描述的 PROD/CONS 环机制。
`CMD_SYNC`（操作码 0x46）被消费意味着前面 `CMD_TLBI_NSNH_ALL`（0x30）的效果已
可见——§4.8 强调「CONS 前进 ≠ 效果可见，要靠 CMD_SYNC」。

## 五、最小配置七步（§4.11）—— 已验证

**文段**：初始化 SMMU 的最小序列（流表 → 队列 → 属性 → IRQ → 使能队列 → 失效
→ 使能转换）。本程序 `main()` 主体就是这段序列。

**关键寄存器位**：`SMMU_CR0` 的 SMMUEN=bit0、EVENTQEN=bit2、CMDQEN=bit3
（bit1 保留）。

**结果**：

```text
[ OK ] 命令队列已使能（CR0ACK.CMDQEN=1）
[ OK ] 事件队列已使能（CR0ACK.EVENTQEN=1）
[ OK ] SMMU 转换已使能（CR0ACK.SMMUEN=1）
[REG ] 最终 SMMU_CR0 = 0x0000000d
[REG ] SMMU_CR0ACK = 0x0000000d
[REG ] SMMU_GERROR = 0x00000000
[ OK ] 无全局错误（GERROR=0）
```

**为什么**：最终 `CR0 = 0x0D` = bit0 + bit2 + bit3 = SMMUEN + EVENTQEN + CMDQEN，
印证上述位布局。每步使能后轮询 `CR0ACK`（§4.11 步骤 5/7 的「轮询直到读到 1」），
是使能类寄存器「写 CR0 → 轮询 ACK」的握手协议。`GERROR=0` 说明队列访问、命令
解析全程无全局错误（§4.10）。

## 六、I/O 一致性（§5.1）—— 概念

**文段**：`SMMU_IDR0.COHACC` 指示 SMMU 是否支持一致访问（BSA 要求转换表/配置
结构访问 I/O 一致）。

> QEMU 的 SMMUv3 实现里 COHACC=1（I/O 一致），故本示例无需额外数据缓存维护，
> `dsb sy` 即可让 SMMU 观察到写入的数据结构（§4.11 步骤 1 的 DSB 分支）。

## 七、命令队列消费的坑（实现细节）

QEMU 的 SMMUv3 命令队列有几个与真实硬件/规范略有出入的地方，写代码时要注意：

- `CR0` 位布局：SMMUEN=0、EVENTQEN=2、CMDQEN=3（bit1 保留）——写错位会导致
  命令队列看似「已使能」实则没使能；
- 命令队列基址须 **64 字节对齐**（QEMU 对 `Q_BASE` 统一按 6 位掩码处理）；
- `CMD_CFGI_ALL`（0x07）在 QEMU 中未实现，会被判为非法命令；本例改用
  §4.11 推荐的 `CMD_TLBI_NSNH_ALL`（0x30）。

## 附：完整执行输出

```text
[====]AArch64 SMMUv3 裸机验证
[REG ] SMMU_IDR0 = 0x0d44101a
[REG ] SMMU_IDR1 = 0x02730010
[REG ] SMMU_IDR3 = 0x00001404
[REG ] SMMU_IDR5 = 0x00000074
[INFO] 流表已配置（线性, 2 个 STE, 均无效 V=0）
[INFO] 命令队列(32项)/事件队列(32项)已配置
[ OK ] 命令队列已使能（CR0ACK.CMDQEN=1）
[ OK ] 事件队列已使能（CR0ACK.EVENTQEN=1）
[INFO] 已提交 CMD_TLBI_NSNH_ALL + CMD_SYNC，等待消费 ...
[REG ] 消费后 CMDQ_CONS = 0x00000002（RD=02）
[ OK ] CMD_SYNC 已消费，此前命令效果可见
[ OK ] SMMU 转换已使能（CR0ACK.SMMUEN=1）
[REG ] 最终 SMMU_CR0 = 0x0000000d
[REG ] SMMU_CR0ACK = 0x0000000d
[REG ] SMMU_GERROR = 0x00000000
[ OK ] 无全局错误（GERROR=0）
[REG ] SMMU_EVENTQ_CONS = 0x00000000
[REG ] SMMU_EVENTQ_PROD = 0x00000000
[EXIT]semihosting 正常退出
```