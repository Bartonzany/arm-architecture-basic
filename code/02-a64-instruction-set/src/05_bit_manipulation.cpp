// 05_bit_manipulation.cpp — 验证 §11 数据处理（三）：位操作
//
// 概念：寄存器内部"摆弄位"的指令（文档 §11 示例）：
//   BFI   位域插入：把源的低 6 位插入目标第 9 位起的位置；
//   UBFX  位域提取：从源第 18 位起取 7 位放入目标；
//   REV   字节反转；RBIT 位序反转（大小端转换常用）。
//
// 关键：位域提取/插入只有在"位置与宽度为编译期常量"时才会生成
//   UBFX/BFI（它们把位置/宽度编码在指令立即数里）；若为运行时变量，
//   编译器只能降级为移位 + 逻辑组合（见 docs/asm-mapping.md §11 说明）。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 05_bit_manipulation.cpp -o 05_bit_manipulation.s

#include <arm_acle.h>   // __rbit/__rev 等位操作 intrinsic
#include <cstdint>
#include "test_util.hpp"

// ============================================================================
// 被测函数
// ============================================================================

// 位域提取：从第 18 位起取 7 位 -> UBFX W0, W0, #18, #7
__attribute__((noinline))
std::uint32_t extract_ubfx(std::uint32_t x) {
    return (x >> 18) & 0x7Fu;
}

// 位域插入：把 src 的低 6 位插入 dst 第 9 位起的位置 -> BFI W0, W1, #9, #6
__attribute__((noinline))
std::uint32_t insert_bfi(std::uint32_t dst, std::uint32_t src) {
    return (dst & ~(0x3Fu << 9)) | ((src & 0x3Fu) << 9);
}

// 位反转（GCC intrinsic __rbit）
__attribute__((noinline))
std::uint32_t reverse_bits(std::uint32_t x) {
    return __rbit(x);                       // RBIT W0, W0
}

// 字节反转（GCC intrinsic __rev）
__attribute__((noinline))
std::uint32_t reverse_bytes(std::uint32_t x) {
    return __rev(x);                        // REV W0, W0
}

// ============================================================================
// 测试
// ============================================================================

static void test_bit_manipulation() {
    group("§11 位操作 BFI / UBFX / RBIT / REV");

    CHECK_EQ(extract_ubfx(0x1FC0000u), 0x7Fu);   // bit18..24 全 1 -> 0x7F
    CHECK_EQ(insert_bfi(0u, 0x3Fu), 0x7E00u);    // 0x3F 插入 bit9 -> 0x3F<<9
    CHECK_EQ(reverse_bits(0x80000000u), 1u);     // RBIT
    CHECK_EQ(reverse_bytes(0x11223344u), 0x44332211u); // REV
}

int main() {
    test_bit_manipulation();
    return report();
}