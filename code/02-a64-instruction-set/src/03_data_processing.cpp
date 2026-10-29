// 03_data_processing.cpp — 验证 §9 数据处理（一）：算术与逻辑指令格式
//
// 概念：逻辑/整数算术指令通用格式为 <Op><S> <Rd>, <Rn>, <Operand2>。
//   Operand2 可以是寄存器（可附移位），也可以是编码在指令里的立即数。
//   操作名加 S（如 ADDS）会更新 ALU 标志。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 03_data_processing.cpp -o 03_data_processing.s

#include <cstdint>

// 立即数算术：ADD/SUB 带 #imm
__attribute__((noinline))
std::uint64_t add_imm(std::uint64_t x) { return x + 5; }   // ADD X0, X0, #5

// 寄存器 + 移位 Operand2：x + (y << 3)
__attribute__((noinline))
std::uint64_t add_shifted(std::uint64_t x, std::uint64_t y) {
    return x + (y << 3);            // 预期：ADD X0, X0, X1, LSL #3
}

// MOV 立即数：把常量搬进寄存器
__attribute__((noinline))
std::uint64_t mov_imm() { return 0x1234; }  // MOV X0, #0x1234

// 逻辑运算：AND / ORR / EOR
__attribute__((noinline))
std::uint64_t logic_ops(std::uint64_t a, std::uint64_t b) {
    return (a & b) | (a ^ b);       // AND X?, EOR X?, ORR X?
}

// MVN：按位取反（~x 编译为 MVN 或 EOR）
__attribute__((noinline))
std::uint64_t bit_not(std::uint64_t x) { return ~x; }   // MVN X0, X0

#include <cstdio>

int main() {
    std::uint64_t r = add_imm(1) + add_shifted(1, 2) + mov_imm() + bit_not(0);
    std::uint64_t l = logic_ops(0xFF00ull, 0x0F0Full);
    std::printf("sum=%llu logic=%llx\n", (unsigned long long)r, (unsigned long long)l);
    return (r == 1 + 5 + (1 + (2 << 3)) + 0x1234 + (~0ull)) ? 0 : 1;
}
