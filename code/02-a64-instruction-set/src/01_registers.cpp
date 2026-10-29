// 01_registers.cpp — 验证 §6 通用寄存器：X 与 W 两种视图
//
// 概念：X0..X30 是 64 位视图，W0..W30 是同一物理寄存器的低 32 位视图。
//   选 X 做 64 位运算，选 W 做 32 位运算。
//   关键副作用：写 W 寄存器会清零对应 X 寄存器的高 32 位。
//
// 编译（交叉编译 + 反汇编）：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 01_registers.cpp -o 01_registers.s

#include <cstdint>

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

// 演示"写 W 清高 32 位"：把一个 64 位值的高 32 位砍掉
// 截断到 32 位后，高 32 位必须为 0
__attribute__((noinline))
std::uint64_t write_w_clears_upper(std::uint64_t x) {
    std::uint32_t low = static_cast<std::uint32_t>(x);  // 写 W 寄存器 -> 高 32 位清零
    return low;                                          // 零扩展到 64 位返回
}

#include <cstdio>

int main() {
    std::uint32_t r32 = add32(0xFFFFFFFFu, 1u);          // 32 位回绕 -> 0
    std::uint64_t r64 = add64(0xFFFFFFFFull, 1ull);      // 64 位 -> 0x100000000
    std::uint64_t rw  = write_w_clears_upper(0xDEADBEEFCAFEBABEull); // 只保留低 32 位
    std::printf("add32=%08x add64=%016llx upper_cleared=%016llx\n",
                r32, (unsigned long long)r64, (unsigned long long)rw);
    // 自检：写 W 后高 32 位应为 0
    return (rw >> 32) == 0 ? 0 : 1;
}
