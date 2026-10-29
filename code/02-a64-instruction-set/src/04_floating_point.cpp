// 04_floating_point.cpp — 验证 §10 浮点运算 + §10.1 8/16 位浮点（FP16/BF16/FP8）
//
// §10 标量浮点：操作名以 F 开头，宽度由寄存器名决定（S=32 位，D=64 位）。
// §10.1 小浮点格式：
//   FP16（FEAT_FP16, Armv8.2-A）：16 位半精度，有真实半精度指令 FADD H / FDIV H；
//   BF16（FEAT_BF16, Armv8.2-A）：指数同 FP32、尾数更短，可看作 FP32 的低 16 位；
//   FP8（FEAT_FP8, Armv9.2-A）：E5M2 / E4M3 两种，由 FPMR 寄存器选择。
//
// 编译（本文件用 pragma 局部启用 FP16，无需改全局 -march）：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 04_floating_point.cpp -o 04_floating_point.s

// 局部启用 FP16 扩展（pragma 只影响本文件，与工程的 armv8-a 基线不冲突）
#pragma GCC target ("arch=armv8.2-a+fp16")

#include <cstdint>
#include <cstring>
#include "test_util.hpp"

// ============================================================================
// §10 标量浮点（FP32 / FP64）
// ============================================================================

// 单精度加法：FADD S0, S0, S1
__attribute__((noinline))
float fadd32(float a, float b) { return a + b; }

// 双精度减法：FSUB D0, D0, D1
__attribute__((noinline))
double fsub64(double a, double b) { return a - b; }

// 单精度除法：FDIV S0, S0, S1
__attribute__((noinline))
float fdiv32(float a, float b) { return a / b; }

// 乘加融合：a*b+c 编译为 FMADD（单条指令）
__attribute__((noinline))
float fmadd32(float a, float b, float c) { return a * b + c; }

// ============================================================================
// §10.1 FP16：16 位半精度（生成真实半精度指令 FADD H / FDIV H）
// ============================================================================

// 半精度加法：FADD H0, H0, H1
__attribute__((noinline))
_Float16 fadd16(_Float16 a, _Float16 b) { return a + b; }

// 半精度除法：FDIV H0, H0, H1（对应文档 §10 的 FDIV H0, H1, H2）
__attribute__((noinline))
_Float16 fdiv16(_Float16 a, _Float16 b) { return a / b; }

// ============================================================================
// §10.1 BF16：bfloat16 = FP32 的高 16 位（1 符号 + 8 指数 + 高 7 尾数）
// ============================================================================

// 取 float 的高 16 位即得 BF16 位模式，印证"BF16 是 FP32 的低精度版本"
__attribute__((noinline))
std::uint16_t fp32_to_bf16_bits(float f) {
    std::uint32_t bits;
    std::memcpy(&bits, &f, sizeof(bits));   // 把 float 位模式搬到整数
    return static_cast<std::uint16_t>(bits >> 16);   // 右移 16 位取高 16 位
}

// ============================================================================
// §10.1 FP8：E5M2 / E4M3（FEAT_FP8, Armv9.2-A）
// GCC 13 尚未提供 fp8 类型与 intrinsic（需 GCC 14+ 与 -march=armv9.2-a），
// 故此处仅作说明，无法生成代码验证：
//   E5M2：5 位指数 + 2 位尾数，范围大、精度低；
//   E4M3：4 位指数 + 3 位尾数，精度更高；
//   具体用哪种由 FPMR 寄存器字段选择。
// ============================================================================

// ============================================================================
// 测试
// ============================================================================

static void test_scalar_fp() {
    group("§10 标量浮点 FP32/FP64");

    CHECK_EQ(fadd32(1.5f, 2.5f), 4.0f);             // FADD S
    CHECK_EQ(fsub64(10.0, 3.0), 7.0);               // FSUB D
    CHECK_EQ(fdiv32(1.0f, 4.0f), 0.25f);            // FDIV S
    CHECK_EQ(fmadd32(2.0f, 3.0f, 4.0f), 10.0f);     // FMADD
}

static void test_small_fp() {
    group("§10.1 8/16 位浮点 FP16 / BF16");

    // FP16：半精度加减乘除（结果转回 float 便于打印比较）
    CHECK_EQ(static_cast<float>(fadd16(static_cast<_Float16>(1.5f),
                                       static_cast<_Float16>(2.5f))), 4.0f);
    CHECK_EQ(static_cast<float>(fdiv16(static_cast<_Float16>(1.0f),
                                       static_cast<_Float16>(4.0f))), 0.25f);

    // BF16：1.0f 的位模式 0x3F800000，高 16 位 = 0x3F80
    CHECK_EQ(fp32_to_bf16_bits(1.0f), 0x3F80u);
}

int main() {
    test_scalar_fp();
    test_small_fp();
    return report();
}