# AArch64 内存属性与特性 —— 概念、实现与结果印证

本文档按《AArch64 内存属性与特性（中文精读版）》章节组织，把**文段内容**、
**代码实现**、**执行结果**、**因果解释**串在一起。该文档几乎全是特权级/
MMU 内容，故用 **EL1 裸机（QEMU virt）** 示例真实验证；仅有无法在 QEMU 建模
的部分（Device 非对齐故障）标注为"说明"。

代码位置：`src/`（boot.S + vector_table.S + main.c + linker.ld）。
运行方式：

```bash
./scripts/build_all.sh --run
```

验证输出（摘要）：

```text
[ B ]建立页表：Normal(WB/WA) vs Device(nGnRnE)
[REG ] MAIR_EL1 = 0x000000000044ff00
[REG ] TCR_EL1  = 0x0000000000803519
[ C ]权限：EL1 写只读页 → 权限故障
[SYNC] EC=0x25  FSC=0x0f  WnR=1   ← 权限故障（0x0F=level3）
[ D ]执行权限：跳转 PXN=1 页 → 指令中止
[SYNC] EC=0x21  FSC=0x0f        ← 指令中止（取指 PXN 页）
[ E ]对齐：SCTLR.A 控制普通内存非对齐访问
[SYNC] EC=0x25  FSC=0x21        ← 对齐故障
[ F ]访问标志：AF=0 页首次访问 → 访问标志故障
[SYNC] EC=0x25  FSC=0x0b        ← 访问标志故障（0x0B=level3）
```

---

## 一、内存类型与属性来源（§2、§3、§5、§6）

### 文段

处理器对待内存和外设用两套规则：普通内存（Normal）可缓存、可乱序、可合并；
设备内存（Device）有副作用、不可缓存、访问次数必须精确。内存类型不直接写进
转换表条目，而是用一个 3 位 **AttrIndx** 去 **MAIR_ELx（内存属性间接寄存器）**
里选一个 8 位属性值（省转换表条目空间）。

### 代码实现（`main.c` 的 `build_page_tables` + `enable_mmu`）

```c
#define MAIR_VAL  ((ATTR_NORMAL_NC << 16) | (ATTR_NORMAL_WB << 8) | ATTR_DEVICE)
// = (0x44 << 16) | (0xFF << 8) | 0x00 = 0x44FF00
//   Attr2 = Normal 非缓存(0x44)，Attr1 = Normal WB/WA(0xFF)，Attr0 = Device-nGnRnE(0x00)

#define ATTRINDX(i) ((unsigned long)(i) << 2)   // 描述符 [4:2] 选 MAIR 索引
```

页表把地址空间分成两种类型：

- **L1[0]：1GB Device 块**，覆盖 `0x0000_0000..0x3FFF_FFFF`（UART/GIC 等 MMIO，
  标了 PXN+UXN），选中 MAIR `Attr0 = Device-nGnRnE`；
- **L1[1]→L2：2MB Normal 块**，覆盖 RAM（`0x4000_0000` 起），选中 MAIR
  `Attr1 = Normal WB/WA`。

### 结果

```text
[REG ] MAIR_EL1 = 0x000000000044ff00
[REG ] SCTLR_EL1 = 0x0000000000c5183d   (M=1 开 MMU, C=1, I=1)
```

### 为什么

`MAIR_EL1=0x44FF00`：Attr0（`[7:0]`）=0x00 是 Device-nGnRnE、Attr1（`[15:8]`）
=0xFF 是 Normal WB/WA。转换表条目的 AttrIndx 只需 3 位，就能复用这 8 位的类型值。

---

## 二、权限：读写与执行（§7、§7.2）

### 文段

`AP` 字段决定谁能读/写：`AP=0b00`（EL1 读写）、`AP=0b10`（EL1 只读）。违反权限
（写只读区）触发**权限故障**。执行权限 `PXN`（特权不可执行）/`UXN`（用户不可
执行）区分"应用代码在 EL0 可执行、在内核权限下不可执行"。

### 代码实现

L3 页表逐页设属性（page 描述符用 `0b11`，与 block 的 `0b01` 不同）：

```c
l3[0] = (unsigned long)read_only_page  | S1_AF | ATTRINDX(1) | S1_AP_RO_EL1 | S1_PAGE;
l3[1] = (unsigned long)exec_never_page | S1_AF | ATTRINDX(1) | S1_AP_RW_EL1 | S1_PXN | S1_PAGE;
```

测试 C 写只读页、测试 D 跳 PXN 页：

```c
g_test = T_PERM;
__asm__ volatile("str %0, [%1]" : : "r"(0xDEADBEEFUL), "r"((unsigned long)read_only_page));
// 预期：数据中止

void (*fn)(void) = (void (*)(void))(unsigned long)exec_never_page;
g_test = T_PXN;
fn();   // 预期：取指非执行页 → 指令中止，处理器把 ELR 指回 pxn_resume
```

### 结果

```text
[REG ]   读只读页 = 0x0000000000000000      ← 读允许
[SYNC] EC=0x25  FSC=0x0f  WnR=1             ← 写 → 权限故障
[REG ]   故障后页内容 = 0x0000000000000000   ← 数据未被写入
[SYNC] EC=0x21  FSC=0x0f                   ← 跳 PXN → 指令中止
```

### 为什么

- `FSC=0x0F` 是"权限故障 level 3"（我们的只读页在 L3），`WnR=1` 表示是写操作；
  故障后重读页内容仍为 0，说明 `str` 没落地——权限真正拦住了写。
- `EC=0x21` 是"当前 EL 指令中止"，`ELR=0x4100_1000` 正是 `exec_never_page` 地址：
  取指本身就被 PXN 挡住，故把 ELR 指到 `pxn_resume` 恢复主流程。

---

## 三、对齐（§10、§12 自测）

### 文段

非对齐访问对**普通内存允许**（`LDP X0,X1,[X2]` 的 X2 只需 64 位对齐）；对**设备
区域不允许**（对齐故障）。想让普通内存的非对齐也报错，可置 `SCTLR_ELx.A`。

### 代码实现（`test_e_alignment`）

```c
write_sctlr(read_sctlr() | SCTLR_A);       // A=1：普通内存非对齐也故障
g_test = T_ALIGN;
__asm__ volatile("ldr %0, [%1]" : "=r"(v) : "r"((unsigned long)normal_rw_page + 1));

write_sctlr(read_sctlr() & ~SCTLR_A);      // A=0：普通内存非对齐允许
__asm__ volatile("ldr %0, [%1]" : "=r"(v) : "r"((unsigned long)normal_rw_page + 1));
```

### 结果

```text
[SYNC] EC=0x25  FSC=0x21   ← A=1 时未对齐访问 → 对齐故障
[ OK ] A=0：普通内存非对齐访问正常完成
```

### 为什么

`FSC=0x21` 就是**对齐故障**；`FAR=0x4100_3001` 指向未对齐地址。A=0 时同样一条
`ldr` 顺利读完，说明"普通内存默认允许非对齐、A 位才是开关"。

> §10/§12 的"设备区域非对齐必故障"是一条架构规则，但 QEMU TCG 没有按内存类型
> 建模这个对齐故障，故本示例只在 [ A ] 里说明、不验证运行时故障；真正能验证的
> 对齐行为是上面的 `SCTLR.A` 开关。

---

## 四、访问标志（§9）

### 文段

`AF` 位跟踪页面是否被访问过（AF=0 未访问、AF=1 已访问）。更新方式两种：**软件
更新**（访问触发访问标志故障，异常处理程序手动置位）和**硬件更新**
（Armv8.1-A 的 `TCR_ELx.HA`）。

### 代码实现（`test_f_access_flag` + `sync_handler_c` 的 `T_AF` 分支）

页表里 `af_page` 故意 **AF=0**：

```c
l3[2] = (unsigned long)af_page | ATTRINDX(1) | S1_AP_RW_EL1 | S1_PAGE;  // 无 S1_AF
```

访问它触发故障，异常处理程序软件置位后返回：

```c
case T_AF:
    l3[2] |= S1_AF;                     // 软件置 AF=1
    __asm__ volatile("dsb ishst");
    __asm__ volatile("tlbi vaae1, %0" : : "r"((unsigned long)af_page));
    __asm__ volatile("isb");
    ...
```

### 结果

```text
[SYNC] EC=0x25  FSC=0x0b   ← 首次访问 AF=0 页 → 访问标志故障
[ OK ] 首次访问触发访问标志故障（FSC=0x0B），处理器已置 AF=1
[ OK ] 再次访问（AF=1）不再故障
```

### 为什么

`FSC=0x0B` 是"访问标志故障 level 3"。第一次访问时 `AF=0`，硬件停下让软件处理；
软件把 `AF` 置 1、`tlbi` 清掉该页的 TLB 表项后返回，第二次访问命中 AF=1 的表项，
不再故障——这就是"软件更新 AF"的完整闭环。

---

## 五、本示例未覆盖（需更高版本 / 更复杂环境）

- **PAN + LDTR/STTR**（§7.1，Armv8.1-A）：本示例用 `cortex-a53`（Armv8.0），无 PAN；
  换成 `-cpu max` 可验证"PAN=1 后 EL1 访问 EL0 数据故障、LDTR 不拦"。
- **权限间接/覆盖**（§8，Armv8.9-A）：需要 `PIRE*/PIR*`、`POR*` 寄存器，QEMU 尚不支持。
- **脏状态 DBM**（§9，Armv8.1-A）：`TCR_EL1.HD` + DBM 位，硬件自动清只读为读写。
- **端序**（§10）：指令取指永远小端；数据端序按异常级别配置，需在 EL3 切换 EE 才有可观
  察效果，本示例未涉及。