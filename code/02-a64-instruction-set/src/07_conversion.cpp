// 07_conversion.cpp — 验证 §13 数据处理（五）：格式转换 FMOV vs SCVTF/FCVT
//
// 概念：FMOV 复制"字面上的位模式"，不做数值解释；SCVTF/FCVT 做真正的
//   数值转换。例如整数 2：
//     FMOV  D0, X0  -> D0 = 0x...0002（按 double 解释是极小的数）
//     SCVTF D1, X0  -> D1 = 2.0
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 07_conversion.cpp -o 07_conversion.s

#include <cstdint>
#include <cstring>

// 位的原样搬运（等价 FMOV）：double 位模式 = 整数 2 的位模式
// 结果按 double 解释是极小的正常值，而不是 2.0
__attribute__((noinline))
double bit_copy_int_to_double(std::uint64_t bits) {
    double d;
    std::memcpy(&d, &bits, sizeof(d));   // 只搬位，不转换数值
    return d;
}

// 真正的数值转换（等价 SCVTF）：有符号 int -> double，得到 2.0
__attribute__((noinline))
double convert_int_to_double(std::int64_t x) {
    return static_cast<double>(x);       // 预期：SCVTF D0, X0
}

// 反向：double -> 无符号 int（等价 FCVTZU，向零舍入）
__attribute__((noinline))
std::uint64_t convert_double_to_int(double d) {
    return static_cast<std::uint64_t>(d); // 预期：FCVTZU X0, D0
}

#include <cstdio>

int main() {
    double raw  = bit_copy_int_to_double(2ull);   // 位模式 0x...0002
    double conv = convert_int_to_double(2ll);     // 2.0
    std::uint64_t back = convert_double_to_int(3.7); // 3
    std::printf("raw_bits_as_double=%g convert=%g back=%llu\n",
                raw, conv, (unsigned long long)back);
    // raw != 2.0 且 conv == 2.0，证明 FMOV(位拷贝) 与 SCVTF(数值转换) 不同
    return (raw != 2.0 && conv == 2.0 && back == 3) ? 0 : 1;
}