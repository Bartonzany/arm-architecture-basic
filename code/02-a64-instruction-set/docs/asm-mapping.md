# C++ 源码与汇编对应关系说明

本文档解释 `src/` 下每个 demo 的 C++ 代码与**本机编译器实际输出**的 A64 汇编
之间的对应关系：哪句 C++ 对应哪条指令、为什么。

## 阅读前提

1. **以下汇编均为本机实际结果**，由以下命令在 `/mnt/workspace` 这台服务器上
   实际交叉编译 + 反汇编得到（非臆测、非教科书示例）：

   ```bash
   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -std=c++14 \
       -fno-stack-protector -fno-stack-clash-protection -c src/XX.cpp
   aarch64-linux-gnu-objdump -d --no-show-raw-insn XX.o
   ```

   工具链版本：`aarch64-linux-gnu-g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`。
   换用其他编译器版本、优化级别或 `-march`（如带 LSE 的 `armv8.1-a`），
   指令序列可能不同，但 C++ 操作 → A64 指令族的**语义对应关系**是稳定的。

2. **-O2 会做激进优化**：常量折叠、代数化简、循环优化会改变汇编形态。
   文档中凡出现"朴素预期"与实际不符处，都用 **⚙ 编译器优化点** 标注并解释。

3. 函数体加了 `__attribute__((noinline))`，否则会内联进 main 看不到独立函数。

---

## 01_registers — §6 寄存器：X/W 视图 与 浮点/向量寄存器

§6 讲了两类寄存器：通用寄存器（X/W 视图）与浮点/向量寄存器（B/H/S/D/Q 视图）。

### 6.1 通用寄存器：X 与 W

**C++**

```cpp
std::uint32_t add32(std::uint32_t a, std::uint32_t b) { return a + b; }
std::uint64_t add64(std::uint64_t a, std::uint64_t b) { return a + b; }
std::uint64_t write_w_clears_upper(std::uint64_t x) {
    std::uint32_t low = static_cast<std::uint32_t>(x);  // 写 W 寄存器
    return low;
}
```

**本机实际汇编**

```asm
add32(unsigned int, unsigned int):
        add     w0, w0, w1
        ret
add64(unsigned long, unsigned long):
        add     x0, x0, x1
        ret
write_w_clears_upper(unsigned long):
        mov     w0, w0
        ret
```

**对应关系**

| C++ | 汇编 | 说明 |
|-----|------|------|
| `uint32_t` 参数/返回 | `w0` | 32 位类型用 W 视图 |
| `uint64_t` 参数/返回 | `x0` | 64 位类型用 X 视图 |
| 截断到 `uint32_t` | `mov w0, w0` | §6 关键副作用：写 W 即清零对应 X 高 32 位 |

> ⚙ 置零写法：本机对 `return 0`（见 02）生成 `mov x0, #0x0`（立即数 0），
> 而非 `mov x0, xzr`——两条都合法，取决于编译器选择。

### 6.2 浮点/向量寄存器：B/H/S/D/Q 视图

同类寄存器还有独立的 32 个 128 位寄存器，按宽度有多种视图：
`B`=8 位、`H`=16 位、`S`=32 位、`D`=64 位、`Q`=128 位。
用 `V` 形式时被当作"装着多个独立元素的向量"。

**C++**

```cpp
float  fadd_s(float a, float b) { return a + b; }   // 标量 S（32 位）
double fadd_d(double a, double b) { return a + b; } // 标量 D（64 位）
void vec_fadd_2d(const double* a, const double* b, double* o) {
    vst1q_f64(o, vaddq_f64(vld1q_f64(a), vld1q_f64(b)));   // 2 个 double 并行
}
void vec_add_2d(const uint64_t* a, const uint64_t* b, uint64_t* o) {
    vst1q_u64(o, vaddq_u64(vld1q_u64(a), vld1q_u64(b)));   // 2 个 int64 并行
}
```

**本机实际汇编**

```asm
fadd_s(float, float):
        fadd    s0, s0, s1          ; S=32 位单精度
        ret
fadd_d(double, double):
        fadd    d0, d0, d1          ; D=64 位双精度
        ret
vec_fadd_2d(double const*, double const*, double*):
        ldr     q0, [x0]
        ldr     q1, [x1]
        fadd    v0.2d, v0.2d, v1.2d ; FADD V0.2D：2 个 64 位浮点并行加
        str     q0, [x2]
        ret
vec_add_2d(unsigned long const*, unsigned long const*, unsigned long*):
        ldr     q0, [x0]
        ldr     q1, [x1]
        add     v0.2d, v0.2d, v1.2d ; ADD V0.2D：2 个 64 位整数并行加
        str     q0, [x2]
        ret
```

**对应关系**

| C++ | 汇编 | 说明 |
|-----|------|------|
| `float` 加 | `fadd s0, s0, s1` | 标量 S 视图（32 位）|
| `double` 加 | `fadd d0, d0, d1` | 标量 D 视图（64 位）|
| `vaddq_f64` | `fadd v0.2d` | **向量 `.2d`**：2 个 64 位元素并行，正是文档的 `FADD V0.2D` |
| `vaddq_u64` | `add v0.2d` | **向量 `.2d`**：2 个 64 位整数并行，正是文档的 `ADD V0.2D` |

> `.2d` 后缀 = 128 位寄存器按 64 位通道分成 2 个元素；同理 `.16b`（16×8bit）、
> `.8h`（8×16bit）、`.4s`（4×32bit），见 demo 08 的 `add v0.16b`/`fadd v0.4s`。

---

## 02_special_regs — §7 XZR / SP / ADR

**C++**

```cpp
std::uint64_t zero_via_xzr() { return 0; }
std::uintptr_t read_pc_like() { __asm__ volatile("adr %0, ." : "=r"(addr)); return addr; }
std::uint64_t sp_based_access(std::uint64_t x) { std::uint64_t local = x + 1; return local * 2; }
```

**本机实际汇编**

```asm
zero_via_xzr():
        mov     x0, #0x0
        ret
read_pc_like():
        adr     x0, .            ; 计算自身地址
        ret
sp_based_access(unsigned long):
        add     x0, x0, #0x1
        lsl     x0, x0, #1
        ret
```

**对应关系**

| C++ | 汇编 | 说明 |
|-----|------|------|
| `return 0` | `mov x0, #0x0` | 立即数 0 直接编码进 MOV（XZR 是另一种选择）|
| `__asm__("adr %0, .")` | `adr x0, .` | PC 不是通用寄存器，只能用 ADR 间接取"当前地址" |
| 局部变量 `local` | 寄存器化 | ⚙ -O2 下简单局部变量不落栈；SP 作基址的真实访问见 demo 11 的 `stp ...[sp]` |

---

## 03_data_processing — §9 算术/逻辑指令格式

**C++**

```cpp
std::uint64_t add_imm(uint64_t x) { return x + 5; }
std::uint64_t add_shifted(uint64_t x, uint64_t y) { return x + (y << 3); }
std::uint64_t mov_imm() { return 0x1234; }
std::uint64_t logic_ops(uint64_t a, uint64_t b) { return (a & b) | (a ^ b); }
std::uint64_t bit_not(uint64_t x) { return ~x; }
```

**本机实际汇编**

```asm
add_imm(unsigned long):
        add     x0, x0, #0x5
        ret
add_shifted(unsigned long, unsigned long):
        add     x0, x0, x1, lsl #3
        ret
mov_imm():
        mov     x0, #0x1234
        ret
logic_ops(unsigned long, unsigned long):
        orr     x0, x0, x1
        ret
bit_not(unsigned long):
        mvn     x0, x0
        ret
```

**对应关系**

| C++ | 汇编 | 说明 |
|-----|------|------|
| `x + 5` | `add x0, x0, #5` | 立即数作 Operand2 |
| `x + (y<<3)` | `add x0, x0, x1, lsl #3` | 寄存器 Operand2 附 `lsl #3`，一条指令完成移位+加 |
| `return 0x1234` | `mov x0, #0x1234` | 小常量直接编码进 MOV |
| `~x` | `mvn x0, x0` | 按位取反 → MVN |
| `(a&b)|(a^b)` | `orr x0, x0, x1` | ⚙ 代数化简 `(a&b)∨(a⊕b)=a∨b`，直接合并成一条 ORR |

---

## 04_floating_point — §10 浮点

**C++**

```cpp
float  fadd32(float a, float b) { return a + b; }
double fsub64(double a, double b) { return a - b; }
float  fdiv32(float a, float b) { return a / b; }
float  fmadd32(float a, float b, float c) { return a * b + c; }
```

**本机实际汇编**

```asm
fadd32(float, float):
        fadd    s0, s0, s1
        ret
fsub64(double, double):
        fsub    d0, d0, d1
        ret
fdiv32(float, float):
        fdiv    s0, s0, s1
        ret
fmadd32(float, float, float):
        fmadd   s0, s0, s1, s2
        ret
```

**对应关系**：浮点指令名以 F 开头，宽度由寄存器名决定——`s`=32 位、`d`=64 位；
`a*b+c` 被融合为一条 `fmadd`（乘加融合）。

---

## 05_bit_manipulation — §11 位操作

**C++**

```cpp
uint32_t extract(unsigned src, unsigned pos, unsigned width) {
    return (src >> pos) & ((1u << width) - 1u);
}
uint32_t insert(uint32_t dst, uint32_t value, unsigned pos, unsigned width) {
    uint32_t mask = ((1u << width) - 1u) << pos;
    return (dst & ~mask) | ((value << pos) & mask);
}
uint32_t reverse_bits(uint32_t x) { return __rbit(x); }
uint32_t reverse_bytes(uint32_t x) { return __rev(x); }
```

**本机实际汇编**

```asm
extract(unsigned int, unsigned int, unsigned int):
        mov     w3, #0xffffffff
        lsr     w0, w0, w1
        lsl     w3, w3, w2
        bic     w0, w0, w3
        ret
insert(unsigned int, unsigned int, unsigned int, unsigned int):
        mov     w4, #0x1
        lsl     w1, w1, w2
        lsl     w4, w4, w3
        sub     w4, w4, #0x1
        eor     w1, w1, w0
        lsl     w4, w4, w2
        and     w4, w4, w1
        eor     w0, w4, w0
        ret
reverse_bits(unsigned int):
        rbit    w0, w0
        ret
reverse_bytes(unsigned int):
        rev     w0, w0
        ret
```

**对应关系**

| C++ | 汇编 | 说明 |
|-----|------|------|
| `__rbit(x)` | `rbit w0, w0` | 直接对应 RBIT |
| `__rev(x)` | `rev w0, w0` | 直接对应 REV |
| `(src>>pos) & mask` | `lsr`+`lsl`+`bic` | ⚙ 文档 §11 用 **UBFX/BFI**，但 GCC 对**变量**宽度/位置选择了移位+BIC 组合，未合并成 UBFX/BFI；语义等价 |

---

## 06_extend_narrow — §12 扩展与窄整数加法

**C++**

```cpp
std::int64_t  sxtb(std::int8_t x)  { return x; }
std::int64_t  sxth(std::int16_t x) { return x; }
std::uint64_t uxth(std::uint16_t x){ return x; }
std::uint16_t uadd16(std::uint16_t a, std::uint16_t b) { return a + b; }
std::int16_t  sadd16(std::int16_t a, std::int16_t b)  { return a + b; }
```

**本机实际汇编**

```asm
sxtb(signed char):
        sxtb    x0, w0
        ret
sxth(short):
        sxth    x0, w0
        ret
uxth(unsigned short):
        and     x0, x0, #0xffff
        ret
uadd16(unsigned short, unsigned short):
        and     w1, w1, #0xffff
        add     w0, w1, w0, uxth
        ret
sadd16(short, short):
        sxth    w1, w1
        add     w0, w1, w0, sxth
        ret
```

**对应关系**：与文档 §12.2 示例 2/3 完全一致——无符号 16 位加法先 `and #0xffff`
清高位再 `add ..., uxth`；有符号 16 位加法先 `sxth` 再 `add ..., sxth`。
`add` 末尾的 `, uxth/sxth` 表示"先把该操作数扩展再参与运算"。

---

## 07_conversion — §13 FMOV vs SCVTF

**C++**

```cpp
double bit_copy_int_to_double(uint64_t bits) {
    double d; std::memcpy(&d, &bits, sizeof d); return d;
}
double convert_int_to_double(int64_t x) { return (double)x; }
uint64_t convert_double_to_int(double d) { return (uint64_t)d; }
```

**本机实际汇编**

```asm
bit_copy_int_to_double(unsigned long):
        fmov    d0, x0
        ret
convert_int_to_double(long):
        scvtf   d0, x0
        ret
convert_double_to_int(double):
        fcvtzu  x0, d0
        ret
```

**对应关系**：`memcpy`（整块位拷贝）→ `fmov`（只搬 64 位进 D 寄存器）；
`(double)x` → `scvtf`（真正数值转换）。这是 §13 核心：**FMOV 搬位，SCVTF/FCVT 换数值**。

---

## 08_neon_simd — §14 NEON 向量

**C++**

```cpp
vst1q_s8(out, vaddq_s8(vld1q_s8(a), vld1q_s8(b)));   // 16 个 int8 并行加
vst1q_f32(out, vaddq_f32(vld1q_f32(a), vld1q_f32(b))); // 4 个 float 并行加
```

**本机实际汇编**

```asm
vec_add_int8(signed char const*, signed char const*, signed char*):
        ldr     q0, [x0]
        ldr     q1, [x1]
        add     v0.16b, v0.16b, v1.16b
        str     q0, [x2]
        ret
vec_add_f32(float const*, float const*, float*):
        ldr     q0, [x0]
        ldr     q1, [x1]
        fadd    v0.4s, v0.4s, v1.4s
        str     q0, [x2]
        ret
```

**对应关系**：`vld1q`→`ldr q`、`vaddq_s8`→`add v0.16b`（16 个 byte 通道）、
`vaddq_f32`→`fadd v0.4s`（4 个 single 通道）、`vst1q`→`str q`。
四/五条指令完成 16 次（或 4 次）运算，印证 SIMD"单指令多数据"。

---

## 09_load_store — §15/16 访问宽度与符号扩展

**C++**

```cpp
uint64_t load_byte_zero_ext(const uint8_t* p) { return *p; }
int64_t  load_byte_sign_ext(const int8_t* p)  { return *p; }
int64_t  load_half_sign_ext(const int16_t* p) { return *p; }
void store_byte_and_half(uint8_t* b, uint16_t* h, uint32_t v) { *b = v; *h = v; }
```

**本机实际汇编**

```asm
load_byte_zero_ext(unsigned char const*):
        ldrb    w0, [x0]
        ret
load_byte_sign_ext(signed char const*):
        ldrsb   x0, [x0]
        ret
load_half_sign_ext(short const*):
        ldrsh   x0, [x0]
        ret
store_byte_and_half(unsigned char*, unsigned short*, unsigned int):
        strb    w2, [x0]
        strh    w2, [x1]
        ret
```

**对应关系**：`ldrb` 加载字节并清零高 48 位（零扩展）；`ldrsb/ldrsh` 带 S 后缀
= 符号扩展；`strb/strh` 只存低 8/16 位。这是 §15/16 的访问宽度与扩展规则。

---

## 10_addressing — §17 寻址方式

**C++**

```cpp
uint64_t read_member_c(const Rec* r) { return r->c; }        // 成员偏移 8
uint32_t read_index(const uint32_t* a, uint32_t i) { return a[i]; }
uint64_t sum_post_index(const uint32_t* a, int n) { /* for(i) s+=a[i]; */ }
```

**本机实际汇编**

```asm
read_member_c(Rec const*):
        ldr     x0, [x0, #8]
        ret
read_index(unsigned int const*, unsigned int):
        ldr     w0, [x0, w1, uxtw #2]
        ret
sum_post_index(unsigned int const*, int):
        cmp     w1, #0x0
        b.le    .Lret0
        mov     x2, x0
        mov     x0, #0x0
        add     x3, x2, w1, sxtw #2
.Lloop:
        ldr     w1, [x2], #4
        add     x0, x0, x1
        cmp     x2, x3
        b.ne    .Lloop
.Lret0:
        ret
```

**对应关系**：成员 `r->c`（偏移 8）→ `[x0, #8]` 立即数偏移；
`a[i]` → `[x0, w1, uxtw #2]`（i 扩展后 ×4 字节）；循环累加 → `ldr w1, [x2], #4`
后索引，指针自增与访存合并为同一条指令。

---

## 11_ldp_stp — §18 成对访问

**C++**

```cpp
Pair make_pair(uint64_t lo, uint64_t hi) { Pair p{lo,hi}; return p; }
uint64_t read_two(const uint64_t* p) { return p[0] + p[1]; }
uint64_t frame_demo(uint64_t x) { uint64_t local = x+1; return local + leaf(x); }
```

**本机实际汇编**

```asm
make_pair(unsigned long, unsigned long):
        ret                     ; ⚙ 见下
read_two(unsigned long const*):
        ldp     x1, x0, [x0]    ; 一次加载两个 64 位
        add     x0, x1, x0
        ret
leaf(unsigned long):
        lsl     x0, x0, #1
        ret
frame_demo(unsigned long):
        stp     x29, x30, [sp, #-16]!   ; 压栈（先减 SP 再存 fp/lr）
        mov     x1, x0
        add     x1, x1, #0x1
        mov     x29, sp
        bl      leaf(unsigned long)
        add     x0, x1, x0
        ldp     x29, x30, [sp], #16     ; 出栈（先读再 SP 加回）
        ret
```

**对应关系**：`ldp/stp` 一次搬运两个寄存器；压栈 `stp ...[sp,#-16]!`
（`!` 写回 SP）、出栈 `ldp ...[sp],#16`；`stp x29,x30` 保存 fp 与 lr。

> ⚙ `make_pair` 被 -O2 优化成空函数（只 `ret`）：返回结构体通过 sret（X8 指针）
> 写回，编译器把写回延迟/消除掉了。真正演示"成对访问"的是 `read_two`（`ldp`）
> 与 `frame_demo`（`stp/ldp`）；"X8 间接结果寄存器"的实际示例在 demo 16
> 的 `return_struct`。

---

## 12_fp_vector_load — §19 浮点/向量加载 + memcpy

**C++**

```cpp
void copy_16_bytes(const uint8_t* s, uint8_t* d) { std::memcpy(d, s, 16); }
void copy_32_bytes(const uint8_t* s, uint8_t* d) { std::memcpy(d, s, 32); }
```

**本机实际汇编**

```asm
copy_16_bytes(unsigned char const*, unsigned char*):
        ldp     x2, x3, [x0]    ; 16 字节 = 两个 X
        stp     x2, x3, [x1]
        ret
copy_32_bytes(unsigned char const*, unsigned char*):
        ldp     q0, q1, [x0]    ; 32 字节 = 两个 Q（128 位向量）
        stp     q0, q1, [x1]
        ret
```

**对应关系**：印证 §19——`memcpy` 用宽寄存器减少迭代次数，代码里没有浮点值
也用 `q0/q1` 向量寄存器一次搬 16 字节。

---

## 13_load_acquire — §20.1 Load-Acquire / Release

**C++**

```cpp
data.store(v, std::memory_order_relaxed);   // 普通写
flag.store(1, std::memory_order_release);   // release 写
while (flag.load(std::memory_order_acquire) == 0) {}  // acquire 读
```

**本机实际汇编**

```asm
producer(unsigned int):
        adrp    x1, data        ; 计算全局变量地址（省略符号重定位细节）
        add     x1, x1, ...
        str     w0, [x1]        ; 普通写 data
        add     x1, x1, #0x8
        mov     w0, #0x1
        stlr    w0, [x1]        ; release 写 flag（隐含屏障）
        ret
consumer():
        adrp    x2, data
        add     x2, x2, ...
        add     x1, x2, #0x8
.Lspin:
        ldar    w0, [x1]        ; acquire 读 flag（隐含屏障）
        cbz     w0, .Lspin
        ldr     w0, [x2]        ; 普通读 data
        ret
```

**对应关系**：`release` 存储 → `stlr`，`acquire` 加载 → `ldar`，单条指令即
"访问 + 内存屏障"，无需额外 `dmb`。这正是 §20.1 的"自带隐式内存屏障"。

---

## 14_branch_flags — §21/22 分支与条件码

**C++**

```cpp
uint32_t if_eq(uint32_t a) { uint32_t b = 0; if (a == 5) b = 5; return b; }
uint32_t while_nonzero(uint32_t a, uint32_t c) { uint32_t b=0; while(a){b+=c;--a;} return b; }
uint64_t sum_n(uint32_t n) { uint64_t s=0; for(i=1..n) s+=i; return s; }
int classify(int a, int b) { if (a - b < 0) return -1; return 0; }
```

**本机实际汇编**

```asm
if_eq(unsigned int):
        mov     w1, #0x5
        cmp     w0, w1
        csel    w0, w1, wzr, eq
        ret
while_nonzero(unsigned int, unsigned int):
        mul     w0, w0, w1      ; ⚙ 循环被识别成乘法 b = a*c
        ret
sum_n(unsigned int):
        mov     w2, w0
        cbz     w0, .Lret0      ; 判零分支
        mov     w1, #0x1
        mov     x0, #0x0
.Lloop:
        add     x0, x0, w1, uxtw
        add     w1, w1, #0x1
        cmp     w2, w1
        b.cs    .Lloop          ; 无符号比较条件分支
.Lret0:
        ret
classify(int, int):
        cmp     w0, w1
        csetm   w0, lt          ; a-b<0 ? 0xFFFFFFFF(-1) : 0
        ret
```

**对应关系**：`if/while` 编译为 `cmp`+条件分支或 `cbz`；但 -O2 尽量用条件选择
（`csel/csetm`）替代分支；`while(a){b+=c;--a}` 甚至被强度削减成一条 `mul`。

---

## 15_conditional_select — §23 条件选择

**C++**

```cpp
uint32_t select_val(uint32_t a) { return (a == 0) ? 6u : 7u; }
uint32_t cond_inc(uint32_t a, uint32_t b) { if (a != 0) b = b + 1; return b; }
int64_t  abs_val(int64_t x) { return x < 0 ? -x : x; }
```

**本机实际汇编**

```asm
select_val(unsigned int):
        cmp     w0, #0x0
        cset    w0, ne          ; ne 时置 1，否则 0
        add     w0, w0, #0x6    ; 1+6=7 或 0+6=6
        ret
cond_inc(unsigned int, unsigned int):
        cmp     w0, #0x0
        cinc    w0, w1, ne      ; if(a!=0) b+1 → 一条 CINC
        ret
abs_val(long):
        cmp     x0, #0x0
        cneg    x0, x0, lt      ; x<0 则取负，无分支
        ret
```

**对应关系**：§23 的条件选择家族（CSEL/CSINC/CINC/CSET/CNEG）取代分支，序列更短、
无跳转。本机为常量 6/7 选了 `cset+add`，`b+1` 用 `cinc`，绝对值用 `cneg`。

---

## 16_function_pcs — §25/26 BL/RET 与 PCS

**C++**

```cpp
uint64_t foo(uint64_t a, uint64_t b) { return (a << 1) + b; }
uint64_t use_callee_saved(uint64_t x) { uint64_t acc=x; for(i=0..9) acc+=i; return acc; }
Big return_struct(uint64_t v) { Big r{v, v+1, v+2}; return r; }
```

**本机实际汇编**

```asm
foo(unsigned long, unsigned long):
        add     x0, x1, x0, lsl #1
        ret
use_callee_saved(unsigned long):
        add     x0, x0, #0x2d   ; ⚙ 0..9 求和被常量折叠为 45(0x2d)
        ret
return_struct(unsigned long):
        add     x2, x0, #0x1
        add     x1, x0, #0x2
        stp     x0, x2, [x8]    ; X8 = 返回结构体内存指针（§26 XR）
        str     x1, [x8, #16]
        ret
```

**对应关系**：`foo` 两个 64 位参数走 `x0/x1`，结果 `x0`（PCS 约定）；
返回结构体时 `x8` 作为间接结果寄存器（XR）指向调用者分配的内存；
`ret` 即"分支到 x30(LR) 的返回"。

> ⚙ `use_callee_saved` 本意演示"用 X19 需先保存"，但 -O2 把 `0+1+...+9`
> 常量折叠成 `+0x2d`，循环消失、也没用到 X19。要强制触发 callee-saved 保存，
> 需无法折叠的循环（如累加动态数组长度）。

---

## 17_syscall — §27 SVC

**C++**

```cpp
register long num __asm__("x8") = 172;   // __NR_getpid
__asm__ volatile("svc #0" : "=r"(ret) : "r"(num) : "memory", "cc");
```

**本机实际汇编**

```asm
sys_getpid_asm():
        mov     x8, #0xac       ; 系统调用号 172 放入 x8
        svc     #0x0            ; 陷入 EL1（触发异常进入内核）
        ret
```

**对应关系**：Linux AArch64 系统调用约定——`x8` 放调用号，`svc #0` 触发同步异常
切到 EL1，返回值由内核写回 `x0`。印证 §27：SVC 是"受控方式切到更高特权级"。

---

## 小结：如何读这些汇编

1. 先看**函数签名**确定寄存器宽度（W/X/D/Q）。
2. 再看**操作类型**对应指令族（算术→add/sub、逻辑→and/orr/eorn、
   浮点→fadd...、访存→ldr/str、控制→cmp/b/csel...）。
3. 最后留意 **-O2 优化**：常量折叠、代数化简、循环强度削减会让汇编与
   "逐句直译"不同，但语义不变。
4. 要复现本机结果，直接 `make disasm` 或
   `aarch64-linux-gnu-objdump -d --no-show-raw-insn <bin>` 对比即可。