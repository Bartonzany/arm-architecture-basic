// 06_extend_narrow.cpp — 验证 §12 数据处理（四）：扩展与窄整数运算
//
// 概念：把数据从窄宽度转换到更宽宽度用 SXTx（符号扩展）/ UXTx（零扩展）。
//   x = B(8bit)/H(16bit)/W(32bit)。窄整数（8/16 位）加法会先扩展再相加：
//   无符号 16 位加法：AND 清高 + UXTH 扩展；有符号 16 位加法：SXTH 扩展。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 06_extend_narrow.cpp -o 06_extend_narrow.s

#include <cstdint>

// 字节符号扩展到 64 位：对应 SXTB X0, W0
__attribute__((noinline))
std::int64_t sxtb(std::int8_t x) { return x; }

// 半字符号扩展：对应 SXTH
__attribute__((noinline))
std::int64_t sxth(std::int16_t x) { return x; }

// 半字零扩展：对应 UXTH（编译器常把 AND #0xffff 与 UXTH 结合）
__attribute__((noinline))
std::uint64_t uxth(std::uint16_t x) { return x; }

// 无符号 16 位加法（文档示例 2）：
//   uint16_t 参数相加 -> 先清高 16 位（AND #0xffff），再 ADD 带 UXTH 扩展
__attribute__((noinline))
std::uint16_t uadd16(std::uint16_t a, std::uint16_t b) {
    return static_cast<std::uint16_t>(a + b);
}

// 有符号 16 位加法（文档示例 3）：
//   int16_t 参数相加 -> SXTH 扩展后 ADD，可带 , SXTH 后缀
__attribute__((noinline))
std::int16_t sadd16(std::int16_t a, std::int16_t b) {
    return static_cast<std::int16_t>(a + b);
}

#include <cstdio>

int main() {
    std::printf("sxtb=%lld sxth=%lld uxth=%llu uadd=%u sadd=%d\n",
                (long long)sxtb(-1),
                (long long)sxth(-2),
                (unsigned long long)uxth(0xFFFFu),
                (unsigned)uadd16(0xFFFEu, 3u),
                (int)sadd16(-20000, -20000));
    // sadd16: -20000 + -20000 = -40000，截断到 16 位后回绕
    return (sxtb(-1) == -1 && sxth(-2) == -2 && uxth(0xFFFF) == 0xFFFF) ? 0 : 1;
}