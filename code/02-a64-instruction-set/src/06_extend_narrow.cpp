// 06_extend_narrow.cpp — 验证 §12 数据处理（四）：扩展与窄整数运算
//
// 概念：把数据从窄宽度转换到更宽宽度用 SXTx（符号扩展）/ UXTx（零扩展）。
//   x = B(8bit)/H(16bit)/W(32bit)。窄整数加法会先扩展再相加：
//   无符号 16 位加法：AND 清高 + UXTH 扩展；有符号 16 位加法：SXTH 扩展。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 06_extend_narrow.cpp -o 06_extend_narrow.s

#include <cstdint>
#include "test_util.hpp"

// ============================================================================
// 被测函数
// ============================================================================

// 字节符号扩展到 64 位：SXTB X0, W0
__attribute__((noinline))
std::int64_t sxtb(std::int8_t x) { return x; }

// 半字符号扩展：SXTH
__attribute__((noinline))
std::int64_t sxth(std::int16_t x) { return x; }

// 半字零扩展：UXTH（实际编译为 AND X0, X0, #0xffff）
__attribute__((noinline))
std::uint64_t uxth(std::uint16_t x) { return x; }

// 无符号 16 位加法（文档示例 2）：AND 清高 + ADD 带 UXTH
__attribute__((noinline))
std::uint16_t uadd16(std::uint16_t a, std::uint16_t b) {
    return static_cast<std::uint16_t>(a + b);
}

// 有符号 16 位加法（文档示例 3）：SXTH + ADD 带 SXTH
__attribute__((noinline))
std::int16_t sadd16(std::int16_t a, std::int16_t b) {
    return static_cast<std::int16_t>(a + b);
}

// ============================================================================
// 测试
// ============================================================================

static void test_extend_narrow() {
    group("§12 扩展与窄整数运算");

    CHECK_EQ(sxtb(static_cast<std::int8_t>(-1)), -1ll);   // SXTB
    CHECK_EQ(sxth(static_cast<std::int16_t>(-2)), -2ll);  // SXTH
    CHECK_EQ(uxth(0xFFFFu), 0xFFFFull);                   // UXTH（零扩展）

    // 无符号 16 位：0xFFFE + 3 = 0x10001，截断 16 位 = 1
    CHECK_EQ(uadd16(0xFFFEu, 3u), 1u);
    // 有符号 16 位：-20000 + -20000 = -40000，溢出回绕为 +25536
    CHECK_EQ(sadd16(-20000, -20000), 25536);
}

int main() {
    test_extend_narrow();
    return report();
}