// 01_registers.cpp — 验证 §6 寄存器：通用寄存器 X/W 与浮点/向量寄存器
//
// §6 讲了两类寄存器：
//  (1) 通用寄存器：X0..X30 是 64 位视图，W0..W30 是低 32 位视图；
//      写 W 寄存器会清零对应 X 寄存器的高 32 位。
//  (2) 浮点/向量寄存器：独立的 32 个 128 位寄存器，多种访问宽度视图：
//      Bx=8 位、Hx=16 位、Sx=32 位、Dx=64 位、Qx=128 位。
//      用 V 形式时寄存器被看作"装着多个独立元素的向量"：
//      FADD V0.2D, V1.2D, V2.2D  向量双精度浮点加（2 个 64 位）
//      ADD  V0.2D, V1.2D, V2.2D  向量 64 位整数加（2 个 64 位）
//
// 编译（交叉编译 + 反汇编）：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 01_registers.cpp -o 01_registers.s

#include <cstdint>
#include <arm_neon.h>

// ================= 通用寄存器：X 与 W =================

// 32 位加法：参数与结果都用 W 寄存器（低 32 位）
__attribute__((noinline))
std::uint32_t add32(std::uint32_t a, std::uint32_t b) {
    return a + b;                       // 预期：ADD W0, W0, W1
}

// 64 位加法：参数与结果都用 X 寄存器
__attribute__((noinline))
std::uint64_t add64(std::uint64_t a, std::uint64_t b) {
    return a + b;                       // 预期：ADD X0, X0, X1
}

// 演示"写 W 清高 32 位"：把 64 位值的高 32 位砍掉
__attribute__((noinline))
std::uint64_t write_w_clears_upper(std::uint64_t x) {
    std::uint32_t low = static_cast<std::uint32_t>(x);  // 写 W -> 高 32 位清零
    return low;                                          // 零扩展返回
}

// ================= 浮点/向量寄存器：B/H/S/D/Q 视图 =================

// 标量 S 视图：32 位单精度浮点加法
__attribute__((noinline))
float fadd_s(float a, float b) { return a + b; }        // FADD S0, S0, S1

// 标量 D 视图：64 位双精度浮点加法
__attribute__((noinline))
double fadd_d(double a, double b) { return a + b; }     // FADD D0, D0, D1

// 向量 .2D 视图：2 个 64 位双精度浮点并行加（对应文档 FADD V0.2D, V1.2D, V2.2D）
__attribute__((noinline))
void vec_fadd_2d(const double* a, const double* b, double* out) {
    float64x2_t va = vld1q_f64(a);      // 加载 2 个 double 到 128 位寄存器
    float64x2_t vb = vld1q_f64(b);
    vst1q_f64(out, vaddq_f64(va, vb));  // FADD V0.2D, V0.2D, V1.2D
}

// 向量 .2D 视图：2 个 64 位整数并行加（对应文档 ADD V0.2D, V1.2D, V2.2D）
__attribute__((noinline))
void vec_add_2d(const std::uint64_t* a, const std::uint64_t* b, std::uint64_t* out) {
    uint64x2_t va = vld1q_u64(a);       // 加载 2 个 uint64
    uint64x2_t vb = vld1q_u64(b);
    vst1q_u64(out, vaddq_u64(va, vb));  // ADD V0.2D, V0.2D, V1.2D
}

// 向量 B 视图：16 个 8 位整数并行加（同一组寄存器，按 8 位通道解释）
// 其余宽度：H=8×16bit(.8h)、S=4×32bit(.4s)、D=2×64bit(.2d)、Q=128bit
__attribute__((noinline))
void vec_add_16b(const std::int8_t* a, const std::int8_t* b, std::int8_t* out) {
    int8x16_t va = vld1q_s8(a);         // 16 个 B（8 位）
    int8x16_t vb = vld1q_s8(b);
    vst1q_s8(out, vaddq_s8(va, vb));    // ADD V0.16B
}

#include <cstdio>

int main() {
    std::uint32_t r32 = add32(0xFFFFFFFFu, 1u);          // 32 位回绕 -> 0
    std::uint64_t r64 = add64(0xFFFFFFFFull, 1ull);      // 64 位 -> 0x100000000
    std::uint64_t rw  = write_w_clears_upper(0xDEADBEEFCAFEBABEull);

    float  fs = fadd_s(1.5f, 2.5f);
    double fd = fadd_d(1.5, 2.5);

    double da[2] = {1.0, 2.0}, db[2] = {10.0, 20.0}, do2[2];
    vec_fadd_2d(da, db, do2);

    std::uint64_t ua[2] = {100, 200}, ub[2] = {1, 2}, uo[2];
    vec_add_2d(ua, ub, uo);

    std::int8_t sa[16], sb[16], so[16];
    for (int i = 0; i < 16; ++i) { sa[i] = (std::int8_t)i; sb[i] = 1; }
    vec_add_16b(sa, sb, so);

    std::printf("add32=%08x add64=%016llx upper=%016llx\n",
                r32, (unsigned long long)r64, (unsigned long long)rw);
    std::printf("fadd_s=%f fadd_d=%f vec2d=%f,%f vecadd2d=%llu,%llu vec16b[0]=%d\n",
                (double)fs, fd, do2[0], do2[1],
                (unsigned long long)uo[0], (unsigned long long)uo[1], (int)so[0]);
    return (do2[0] == 11.0 && do2[1] == 22.0 && uo[0] == 101 && uo[1] == 202
            && so[0] == 1) ? 0 : 1;
}