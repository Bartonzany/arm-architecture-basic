# MMU 初始化示例 —— 概念、代码与示例索引

本目录收录 Arm 官方博客《Basic MMU Initialization Sequences》提供的 10 个
AArch64 MMU 初始化示例（`startup.S`）。它们不是可直接运行的应用，而是一组
"最小 MMU 打开流程"骨架：从 EL3 复位 → 配好系统寄存器 → 生成页表 → 打开
`SCTLR_ELx.M`，用于对照理解不同特权级/不同页表级数/不同特性的配置差异。

源码统一在 `src/<示例名>/startup.S`；`docs/upstream-ReadMe.md` 是 Arm 原始
说明（含 FVP 运行参数）。

---

## 一、MMU 核心概念（本节知识对应各示例）

### 1. 地址翻译与页表级数

AArch64 的 stage 1 翻译把虚拟地址（VA）经多级页表翻译成物理地址（PA）。
4KB 粒度的地址划分如下（位号从 VA 最低位 0 起）：

| 级 | 索引位置 | 每表项数 | 覆盖粒度（block） |
|----|----------|----------|--------------------|
| L0 |[47:39] | 512 | 512 GB |
| L1 |[38:30] | 512 | 1 GB |
| L2 |[29:21] | 512 | 2 MB |
| L3 |[20:12] | 512 | 4 KB（page）|

> 每一级的表项**要么**是"指向下一级表的指针（table descriptor）"，
> **要么**是"直接映射一块地址的 block/page descriptor"。表项最低 2 位
> `b[1:0]=0b11`，设 `b[1]`（bit1）为 1 表示 table、`b[0]`（bit0）为 1
> 表示有效。翻译从哪一级开始由 TCR 里 `T0SZ` 决定（L0 vs L1）。

### 2. 关键系统寄存器

| 寄存器 | 作用 |
|--------|------|
| `TTBR0_ELx` | stage 1 页表基地址（示例只用 TTBR0，禁用 TTBR1 表遍历）|
| `TCR_ELx` | 配置 VA 宽度（`T0SZ`）、起始级、粒度（`TG0`）、cache/share 属性 |
| `MAIR_ELx` | 内存属性编码（`0xFF44`：Attr0=Normal-非cache、Attr1=Normal-WB/WA、Attr2=Device-nGnRnE）|
| `SCTLR_ELx` | `M=1` 打开 MMU、`C`/`I` 打开 data/instruction 缓存 |
| `VTTBR_EL2` / `VTCR_EL2` | EL2 stage 2 页表基地址 + 配置（IPA 宽度、起始级）|

### 3. 一级页表（仅 L1）

EL1/EL3 的 `T0SZ=0x19` 表示 VA 宽度 39-bit，从 **L1** 起译，一个 L1 表项
直接是 **1GB block**。示例用 3 个表项覆盖最低 3GB：

| L1 索引 | VA 范围 | 属性 |
|---------|---------|------|
| [0] | 0x0000,0000 – 0x3FFF,FFFF | Device-nGnRnE（外设）|
| [1] | 0x4000,0000 – 0x7FFF,FFFF | Device-nGnRnE |
| [2] | 0x8000,0000 – 0xBFFF,FFFF | Normal WB/WA（DRAM，VE/Base 平台）|

### 4. 二级页表（L1 + L2）

`T0SZ=0x20`（32-bit VA）仍从 L1 起译，但 L1 表项改为**指向 L2 表的指针**、
L2 表项是 **2MB block**。示例把 4 张 L2 表背靠背放置、当作"一张大表"，用
一个循环填满，映射 4GB 地址空间。

### 5. 三级页表（L0 → L2）

`T0SZ=0x10`（48-bit VA）从 **L0** 起译：L0 → L1 → L2（L2 仍是 2MB block）。
示例演示从 L0 一级级建立到 L2 的完整链路。

### 6. Stage 2（EL2 第二阶段翻译）

虚拟化的第二次翻译（IPA → PA）：`VTCR_EL2.T0SZ/SL0` 配 IPA 宽度与起始级、
`VTTBR_EL2` 给 stage 2 页表基地址。`HCR_EL2.VM=1` 打开 stage 2 翻译。

### 7. AF（Access Flag）与硬件更新

描述符 bit10 是 AF（访问标志），软件在 table walk 时读到 AF=0 会触发
Access Flag fault。ARMv8.1-A 引入 `TCR_EL3.HA=1`，让硬件在访问时**自动置位
AF**，省去软件再走一遍更新流程。

### 8. E2H / VHE

`HCR_EL2.E2H=1` 打开 VHE（虚拟化宿主机扩展），使 EL2 像"EL2 里的 EL1"一样
拥有 TTBR0/TTBR1 两套页表。`{E2H,TGE}={1,1}` 时，访问 `_EL1` 寄存器会被重定向
到 `_EL2` 等价寄存器（真实 EL1 寄存器改用 `_EL12` 别名访问）。

---

## 二、10 个示例索引

| 示例 | EL / 翻译级 | 关键配置 | 说明 |
|------|-------------|----------|------|
| `el1_stage1` | EL3→EL1，stage1 | `TCR_EL1.T0SZ=0x19`，L1 1GB | 最小 EL1 MMU：39-bit VA，3 个 1GB block |
| `el1_stage1_and_stage2` | EL1 stage1 + EL2 stage2 | `VTCR_EL2(T0SZ=32,SL0=1)`、`VTTBR_EL2` | 同时打开 stage1 + stage2 两段翻译 |
| `el2_stage1_l1andl2` | EL3→EL2，stage1 | `TCR_EL2.T0SZ=32`，L1+L2 | EL2 无 TTBR1；L1 指针 + 4 张 L2 表（2MB）|
| `el2_stage1_l1only` | EL3→EL2，stage1 | `TCR_EL2.T0SZ=0x19`，L1 1GB | EL2 只用一级页表（3 个 1GB block）|
| `el2_stage1_l1only_e2h` | EL2 + E2H | `HCR_EL2.E2H=1,TGE=1`，armv8.1-a | VHE 下 `_EL1` 寄存器重定向到 `_EL2` |
| `el3_AF_update` | EL3，stage1 | `TCR_EL3.HA=1`，AF=0 模板，armv8.1-a | 硬件自动更新 AF 位 |
| `el3_stage1_full_mem_map` | EL3，stage1 | `TCR_EL3.T0SZ=32`，L1+L2 | 全 4GB 映射（2MB 粒度）|
| `el3_stage1_l0_to_l2` | EL3，stage1 | `TCR_EL3.T0SZ=16`（48-bit），L0→L1→L2 | 从 L0 起译的三级表 |
| `el3_stage1_l1andl2` | EL3，stage1 | `TCR_EL3.T0SZ=32`，L1+L2 | L1 指针 + 4 张 L2 表（2MB）|
| `el3_stage1_l1only` | EL3，stage1 | `TCR_EL3.T0SZ=0x19`，L1 1GB | EL3 无 TTBR1；一级页表（1GB block）|

> `_e2h` 与 `_AF_update` 两个示例需要的 ARMv8.1-A 特性在 `scripts/build_all.sh`
> 里用 `-march=armv8.1-a` 显式声明。

---

## 三、这些代码是如何工作的（以 `el1_stage1` 为最小样例）

`startup64` 入口的完整流程（其他示例只是 EL 与页表级数不同，骨架相同）：

1. **复位态到 EL3**：清寄存器、按 `MPIDR_EL1` 区分主/从核；
2. **配 EL3 出口**：`CPTR_EL3/EL2` 允许访问、`SCR_EL3`（NS/RW）、
   `HCR_EL2.RW`、`SCTLR_EL1` 清零，`ERET` 切入 EL1；
3. **装页表基址**：`LDR x0,=tt_l1_base; MSR TTBR0_EL1,x0`；
4. **配内存属性**：`MAIR_EL1 = 0xFF44`（Normal/Device 三档属性）；
5. **配翻译控制**：`TCR_EL1`（T0SZ=39-bit、L1 起始、Inner/Outer WB/WA、
   Inner Shareable、EPD1=1 关 TTBR1）；
6. **清 TLB**：`TLBI VMALLE1` + `DSB/ISB` 保证表遍历从新页表开始；
7. **填页表**：直接把 3 个 block 描述符写进 `tt_l1_base` 前三项——
   `TT_S1_DEVICE_nGnRnE`（0 与 0x4000_0000 两段）与 `TT_S1_NORMAL_WBWA`
   （0x8000_0000 DRAM 段）；
8. **开 MMU**：`SCTLR_EL1 = M|C|I`，`ISB` 后 MMU 生效。

### 页表项模板（关键位）

```asm
.equ TT_S1_NORMAL_WBWA,   0x00000000000000405
                                                  // AttrIndx=1(WB/WA), AF=1, 有效
.equ TT_S1_DEVICE_nGnRnE, 0x00600000000000409
                                                  // AttrIndx=2, PXN=1, UXN=1, AF=1
```

- bit[63:56] 无关 / bit[54]UXN / bit[53]PXN / AttrIndx 由 MAIR 解析
  / AF 是 bit10 / AP 是 bit[7:6]（0b00 = EL1 可写、EL0 无访问）/ bit[1:0]=0b01
  表示 block 有效。
- 代码里不写页表级数的"类型位"是因为 L1 直接 block 无需 table 指针。

### 代码位置与 Mark 点

每个 `startup.S` 内部都有清晰注释段：`// MMU setup`、`// Generate Translation
Table`、`// Enable MMU`。阅读时按以下顺序找：

```
start64 → primary → (EXIT EL3) → el1/el2/el3_entry
        → Set Base address(TTBR)
        → Set MAIR → Set TCR → ISB → TLBI/DSB/ISB
        → Generate Translation Table
        → Enable MMU(SCTLR.M)
        → semihosting exit(HLT #0xf000)
```

---

## 四、与 02/03/04/05 的关系

- 本目录是**纯特权级 MMU 初始化**，可视为 04（虚拟化）里 stage 2 翻译与
  03（异常模型）里特权级切换的姊妹篇：`ERET`（EL 切换）在 03/04 已分别
  验证，这里聚焦"页表 + 系统寄存器 + 打开 MMU"这一环。
- 表的"描述符模板 + 地址 OR 上去"写法与 02 里位操作/加载指令的语义一致，
  可对照 `02-a64-instruction-set` 的 `ADD/ORR/STR` 理解。

## 五、运行环境（重要提示）

原示例面向 **Arm FVP Base Platform AEM**（需 `armclang` + 特定 `-C` 参数，
见 `docs/upstream-ReadMe.md`）；本仓库提供的 `scripts/build_all.sh` 只做
**汇编 + 链接 + 反汇编**（用 `aarch64-linux-gnu-*` GNU 工具链），产物在
`build/<示例名>/image.elf` 与 `image.dis`，供静态阅读与对照，不能直接在
QEMU 上原样运行。