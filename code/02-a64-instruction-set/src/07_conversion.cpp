// 07_conversion.cpp — 验证 §13 数据处理（五）：格式转换 FMOV vs SCVTF/FCVT
//
// 概念：FMOV 复制"字面上的位模式"，不做数值解释；SCVTF/FCVT 做真正的数值转换。
//   整数 2：
//     FMOV  D0, X0  -> D0 = 0x...0002（按 double 解释是极小的数）
//     SCVTF D1, X0  -> D1 = 2.0
//   FCVTxx 把浮点转整数，xx 决定舍入模式：
//     FCVTZU  向零舍入（无符号）
//     FCVTNS  向最近舍入（nearest, ties to even，有符号）
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 07_conversion.cpp -o 07_conversion.s

#include <cstdint>
#include <cstring>
#include "test_util.hpp"

// ============================================================================
// 被测函数
// ============================================================================

// 位的原样搬运（等价 FMOV）：只搬位，不转换数值
__attribute__((noinline))
double bit_copy_int_to_double(std::uint64_t bits) {
    double d;
    std::memcpy(&d, &bits, sizeof(d));   // 编译为 FMOV D0, X0
    return d;
}

// 真正的数值转换（等价 SCVTF）：有符号 int -> double
__attribute__((noinline))
double convert_int_to_double(std::int64_t x) {
    return static_cast<double>(x);       // SCVTF D0, X0
}

// double -> 无符号 int，向零舍入（等价 FCVTZU）
__attribute__((noinline))
std::uint64_t convert_double_to_int(double d) {
    return static_cast<std::uint64_t>(d); // FCVTZU X0, D0
}

// double -> long，向最近舍入（nearest, ties to even，等价 FCVTNS）
// 注：GCC 对 std::llrint/__builtin_llrint 生成 frintx + fcvtzs 两条，
//     不直接生成单条 fcvtns，故用内联汇编精确展示该指令。
__attribute__((noinline))
long convert_double_to_int_round(double d) {
    long r;
    __asm__ volatile("fcvtns %0, %d1" : "=r"(r) : "w"(d));  // FCVTNS X0, D0
    return r;
}

// ============================================================================
// 测试
// ============================================================================

static void test_conversion() {
    group("§13 FMOV 搬位 vs SCVTF 数值转换 vs FCVT 舍入");

    // 位拷贝（FMOV）：位模式 0x2 按 double 解释不是 2.0
    CHECK(bit_copy_int_to_double(2ull) != 2.0);
    // 数值转换（SCVTF）：得到真正的 2.0
    CHECK_EQ(convert_int_to_double(2ll), 2.0);

    // FCVTZU 向零舍入：3.7 -> 3
    CHECK_EQ(convert_double_to_int(3.7), 3ull);
    // FCVTNS 向最近舍入（ties to even）：3.7 -> 4
    CHECK_EQ(convert_double_to_int_round(3.7), 4l);
    // ties to even：2.5 -> 2（2 为偶数），3.5 -> 4（4 为偶数）
    CHECK_EQ(convert_double_to_int_round(2.5), 2l);
    CHECK_EQ(convert_double_to_int_round(3.5), 4l);
    CHECK_EQ(convert_double_to_int_round(-2.5), -2l);
}

int main() {
    test_conversion();
    return report();
}