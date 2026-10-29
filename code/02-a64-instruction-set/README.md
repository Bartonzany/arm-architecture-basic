# A64 指令集架构 —— 知识点验证代码

本目录针对《03 - A64 指令集架构指南（中文精读版）》整理的可验证知识点，
编写了 17 个最小的 C++ 复现程序。每个 demo 都附有 `-O2` 优化下的 AArch64
交叉编译产物与反汇编，用于对照"一段 C++ 代码如何被编译器翻译成 A64 指令"。

> 文档原文：《Learn the architecture - A64 Instruction Set Architecture Guide》
> 版本：v1.4

> **📖 汇编对应关系**：每个 demo 的 C++ 代码、预期汇编与逐行解释见
> [docs/asm-mapping.md](docs/asm-mapping.md)（含 -O2 编译器优化点说明）。

## 一、文档框架分析

这份指南的目标不是逐条背指令，而是建立四张"地图"，本仓库代码恰好覆盖其中
三张（系统级指令需特权，见 [不覆盖部分](#未覆盖知识点)）：

| 文档脉络 | 章节 | 核心内容 | 对应代码 |
|----------|------|----------|----------|
| 指令长什么样 | §9–§14 | 数据处理（算术/逻辑/浮点/位/转换/向量）| demo 03–08 |
| 寄存器怎么用 | §6–§8 | X/W 视图、特殊寄存器、系统寄存器 | demo 01–02 |
| 加载与存储 | §15–§20 | 访问宽度、寻址、LDP/STP、向量加载、acquire/release | demo 09–13 |
| 程序流与控制 | §21–§27 | 分支、条件码、条件选择、函数调用、系统调用 | demo 14–17 |

## 二、知识点 → Demo 映射

| # | 源文件 | 文档章节 | 验证机制 |
|---|--------|----------|----------|
| 01 | `01_registers.cpp` | §6 | X 与 W 是同一寄存器的两种视图；写 W 清零 X 高 32 位 |
| 02 | `02_special_regs.cpp` | §7 | XZR 零寄存器、`ADR` 读"PC"、SP 作基址 |
| 03 | `03_data_processing.cpp` | §9 | 算术/逻辑指令格式：立即数、移位 Operand2、MOV/MVN、AND/ORR/EOR |
| 04 | `04_floating_point.cpp` | §10 | 浮点 FADD/FSUB/FDIV/FMA（S/D 宽度）|
| 05 | `05_bit_manipulation.cpp` | §11 | 位操作 UBFX/BFI/RBIT/REV |
| 06 | `06_extend_narrow.cpp` | §12 | 扩展 SXTB/SXTH/UXTH；16 位窄整数加法 |
| 07 | `07_conversion.cpp` | §13 | `FMOV`（搬运位）vs `SCVTF`（数值转换）|
| 08 | `08_neon_simd.cpp` | §14 | Advanced SIMD (NEON) 128 位向量加法 |
| 09 | `09_load_store.cpp` | §15/16 | 访问宽度（B/H/W/X）；零扩展 vs 符号扩展（LDRSB/LDRSH）|
| 10 | `10_addressing.cpp` | §17 | 寻址方式：偏移 / 寄存器偏移 / 后索引 |
| 11 | `11_ldp_stp.cpp` | §18 | 成对访问 LDP/STP、栈帧压栈出栈 |
| 12 | `12_fp_vector_load.cpp` | §19 | 浮点/向量寄存器加载；memcpy 用宽寄存器 |
| 13 | `13_load_acquire.cpp` | §20.1 | Load-Acquire/Release（STLR/LDAR 隐式屏障）|
| 14 | `14_branch_flags.cpp` | §21/22 | 分支 CBZ/B.cond；条件码 N/Z/C/V（CMP/TST）|
| 15 | `15_conditional_select.cpp` | §23 | 条件选择 CSEL/CSINC/CINC/CSET/CNEG |
| 16 | `16_function_pcs.cpp` | §25/26 | BL/RET；PCS 参数寄存器、callee-saved |
| 17 | `17_syscall.cpp` | §27 | SVC 系统调用（`svc #0`）|

## 三、目录结构

```
03-a64-instruction-set/
├── README.md                # 本文件（框架分析 + 知识点映射 + 使用说明）
├── docs/
│   └── asm-mapping.md       # C++ ↔ 汇编逐行对应解释（含优化点）
├── CMakeLists.txt           # CMake 构建
├── Makefile                 # make 构建
├── scripts/
│   └── build_all.sh         # 一键交叉编译 + 汇编 + 反汇编
├── src/                     # 17 个自包含 demo（含 main）
└── build/                   # 构建产物（编译后生成，已 gitignore）
    ├── bin/                 #   AArch64 可执行文件
    ├── asm/                 #   -O2 汇编 (.s)
    └── disasm/              #   objdump 反汇编 (.txt)
```

## 四、环境准备

```bash
# Ubuntu 安装交叉编译工具链
sudo apt install g++-aarch64-linux-gnu binutils-aarch64-linux-gnu

# （可选）在 x86 主机上运行 demo，需要模拟器
sudo apt install qemu-user
```

> 所有 demo 编译为 AArch64 可执行文件，可直接在 ARM64 真机运行；
> 无真机时用 `qemu-aarch64` 模拟运行，或在 x86 上只看交叉编译产出的汇编/反汇编。

## 五、构建方式（三选一）

### 方式 1：一键脚本（推荐）

```bash
./scripts/build_all.sh          # 编译 + 生成汇编 + 反汇编
./scripts/build_all.sh --run    # 额外用 qemu 运行全部 demo
./scripts/build_all.sh clean    # 清理
```

### 方式 2：Makefile

```bash
make            # 编译全部可执行 → build/bin/
make asm        # 生成 -O2 汇编 → build/asm/*.s
make disasm     # 反汇编 → build/disasm/*.txt
make run        # qemu 运行全部
make clean
```

### 方式 3：CMake

```bash
cmake -B build-cmake -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++
cmake --build build-cmake -j          # 编译全部 + 生成汇编到 build-cmake/asm/
```

## 六、查看单个 demo 的汇编

```bash
# 直接编译生成汇编
aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -std=c++14 \
    -fno-stack-protector -S src/06_extend_narrow.cpp -o /tmp/06.s

# 反汇编查看关键函数（去掉 C++ 符号修饰后更易读）
aarch64-linux-gnu-objdump -d --no-show-raw-insn build/bin/06_extend_narrow \
    | grep -A15 '<sadd16'
```

## 七、关键汇编对照（节选）

以下是几个能"一眼印证理论"的真实 `-O2` 输出：

**§6 写 W 清高 32 位**（`write_w_clears_upper`）

```asm
mov  w0, w0     ; 只需写 W0，X0 高 32 位自动清零
ret
```

**§12 窄整数加法**（与文档示例 2/3 完全一致）

```asm
uadd16:  and  w1, w1, #0xffff   ; 无符号：先清高 16 位
         add  w0, w1, w0, uxth  ; 再带 UXTH 扩展相加
sadd16:  sxth w1, w1            ; 有符号：SXTH 扩展
         add  w0, w1, w0, sxth  ; ADD 末尾带 , SXTH 后缀
```

**§13 FMOV 搬运位 vs SCVTF 数值转换**

```asm
bit_copy_int_to_double:  fmov   d0, x0   ; 只搬位模式
convert_int_to_double:   scvtf  d0, x0   ; 真正数值转换
```

**§20.1 Load-Acquire / Release**

```asm
producer:  str   w0, [x1]   ; 普通写 data
           stlr  w0, [x1]   ; release 写 flag（隐含屏障）
consumer:  ldar  w0, [x1]   ; acquire 读 flag（隐含屏障）
           ldr   w0, [x2]   ; 普通读 data
```

**§23 条件选择替代分支**

```asm
cond_inc:  cmp  w0, #0x0
           cinc w0, w1, ne  ; if(a!=0) b=b+1 浓缩为一条 CINC
abs_val:   cmp  x0, #0x0
           cneg x0, x0, lt  ; x<0 则取负，无分支
```

**§27 SVC 系统调用**

```asm
sys_getpid_asm:  mov  x8, #0xac   ; 系统调用号 getpid = 172
                 svc  #0x0        ; 陷入 EL1
                 ret
```

## 八、未覆盖知识点

受限于"用户态可验证"与工具链版本，以下章节仅做文字说明，未提供 demo：

- **§8 系统寄存器 MRS/MSR**：访问 `SCTLR_EL1` 等需 EL1 及以上特权，用户态进程
  直接执行会触发异常，需裸机/内核或特权容器才能演示。
- **§14 SVE/SVE2/SME**：需 `-march=armv8.2-a+sve` 及支持 SVE 的编译器/模拟器，
  超出本仓库的 armv8-a 基线范围。
- **§20.2/20.3 LD64B/CPYx/SETx**：Armv8.7-A+ 引入的专用加速器指令，普通编译器
  与模拟器不支持。
- **§24 条件故障 FEAT_CFLT**：Armv9.8-A 引入，当前工具链不可用。