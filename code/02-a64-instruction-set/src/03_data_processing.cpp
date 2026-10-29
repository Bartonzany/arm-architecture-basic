// 03_data_processing.cpp — 验证 §9 数据处理（一）：算术与逻辑指令格式
//
// 概念：逻辑/整数算术指令通用格式为 <Op><S> <Rd>, <Rn>, <Operand2>。
//   Operand2 可以是寄存器（可附移位），也可以是编码在指令里的立即数。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 03_data_processing.cpp -o 03_data_processing.s

#include <cstdint>
#include "test_util.hpp"

// ============================================================================
// 被测函数
// ============================================================================

// 立即数算术：ADD 带 #imm
__attribute__((noinline))
std::uint64_t add_imm(std::uint64_t x) { return x + 5; }   // ADD X0, X0, #5

// 寄存器 + 移位 Operand2：x + (y << 3)
__attribute__((noinline))
std::uint64_t add_shifted(std::uint64_t x, std::uint64_t y) {
    return x + (y << 3);            // ADD X0, X0, X1, LSL #3
}

// MOV 立即数：把常量搬进寄存器
__attribute__((noinline))
std::uint64_t mov_imm() { return 0x1234; }  // MOV X0, #0x1234

// 逻辑运算：AND / ORR / EOR（本式被编译器化简为 ORR）
__attribute__((noinline))
std::uint64_t logic_ops(std::uint64_t a, std::uint64_t b) {
    return (a & b) | (a ^ b);       // 等价 a | b，实际编译为 ORR X0, X0, X1
}

// MVN：按位取反
__attribute__((noinline))
std::uint64_t bit_not(std::uint64_t x) { return ~x; }   // MVN X0, X0

// ============================================================================
// 测试
// ============================================================================

static void test_data_processing() {
    group("§9 算术/逻辑指令格式");

    CHECK_EQ(add_imm(1), 6ull);                    // 1 + 5
    CHECK_EQ(add_shifted(1, 2), 17ull);            // 1 + (2 << 3) = 1 + 16
    CHECK_EQ(mov_imm(), 0x1234ull);                // MOV 立即数
    CHECK_EQ(logic_ops(0xFF00ull, 0x0F0Full), 0xFF0Full);   // a | b
    CHECK_EQ(bit_not(0ull), ~0ull);                // MVN
}

int main() {
    test_data_processing();
    return report();
}