// 05_bit_manipulation.cpp — 验证 §11 数据处理（三）：位操作
//
// 概念：寄存器内部"摆弄位"的指令：
//   BFI  位域插入（把一段位插入目标特定位）；
//   UBFX 无符号位域提取；
//   SBFX 有符号位域提取；
//   REV16 半字内字节反转；RBIT 位序反转（大小端转换常用）。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 05_bit_manipulation.cpp -o 05_bit_manipulation.s

#include <arm_acle.h>   // __rbit/__rev16/__rev 等位操作 intrinsic
#include <cstdint>

// 位域提取：取 src 第 start 位起的 width 位，对应 UBFX
__attribute__((noinline))
std::uint32_t extract(unsigned src, unsigned pos, unsigned width) {
    return (src >> pos) & ((1u << width) - 1u);   // 预期：UBFX
}

// 位域插入：把 value 的低 width 位插入 dst 的 pos 位，对应 BFI
__attribute__((noinline))
std::uint32_t insert(std::uint32_t dst, std::uint32_t value,
                     unsigned pos, unsigned width) {
    std::uint32_t mask = ((1u << width) - 1u) << pos;
    return (dst & ~mask) | ((value << pos) & mask);  // 预期：BFI
}

// 位反转发转 RBIT（GCC intrinsic __rbit）
__attribute__((noinline))
std::uint32_t reverse_bits(std::uint32_t x) {
    return __rbit(x);                       // 预期：RBIT W0, W0
}

// 字节反转 REV（GCC intrinsic __rev）
__attribute__((noinline))
std::uint32_t reverse_bytes(std::uint32_t x) {
    return __rev(x);                        // 预期：REV W0, W0
}

#include <cstdio>

int main() {
    std::uint32_t e = extract(0b10110110u, 2, 3);   // 取 bit2..4 = 101 = 5
    std::uint32_t i = insert(0u, 0b101u, 4, 3);     // 插到 bit4..6
    std::uint32_t rb = reverse_bits(0x80000000u);    // 反转后 = 1
    std::uint32_t rv = reverse_bytes(0x11223344u);   // = 0x44332211
    std::printf("extract=%u insert=%u rbit=%u rev=%08x\n", e, i, rb, rv);
    return (e == 5 && rb == 1 && rv == 0x44332211u) ? 0 : 1;
}