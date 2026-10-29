// 11_ldp_stp.cpp — 验证 §18 加载与存储（四）：成对访问 LDP/STP
//
// 概念：LDP/STP 一次搬运两个寄存器与内存之间的数据，常用于压栈/出栈：
//   STP X0, X1, [SP, #-16]!  压栈（SP 先减 16 再存两个 64 位值）
//   LDP X0, X1, [SP], #16    出栈（先读两个值再 SP 加 16）
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 11_ldp_stp.cpp -o 11_ldp_stp.s

#include <cstdint>
#include "test_util.hpp"

struct Pair { std::uint64_t lo, hi; };

// ============================================================================
// 被测函数
// ============================================================================

// 构造结构体（返回结构体通过 X8 写回）
__attribute__((noinline))
Pair make_pair(std::uint64_t lo, std::uint64_t hi) {
    Pair p{lo, hi};
    return p;
}

// 读两个相邻 64 位：LDP X1, X0, [X0]
__attribute__((noinline))
std::uint64_t read_two(const std::uint64_t* p) {
    return p[0] + p[1];
}

// 叶子函数
__attribute__((noinline))
std::uint64_t leaf(std::uint64_t x) { return x * 2; }

// 带栈帧的非叶子函数：prologue/epilogue 用 STP/LDP 保存 fp/lr
__attribute__((noinline))
std::uint64_t frame_demo(std::uint64_t x) {
    std::uint64_t local = x + 1;    // 触发建栈帧
    return local + leaf(x);
}

// ============================================================================
// 测试
// ============================================================================

static void test_ldp_stp() {
    group("§18 成对访问 LDP/STP");

    Pair p = make_pair(0x11111111ull, 0x22222222ull);
    CHECK_EQ(p.lo, 0x11111111ull);
    CHECK_EQ(p.hi, 0x22222222ull);

    std::uint64_t arr[2] = {100, 200};
    CHECK_EQ(read_two(arr), 300ull);   // LDP 一次读两个

    CHECK_EQ(frame_demo(5), 16ull);    // (5+1) + 5*2 = 16
}

int main() {
    test_ldp_stp();
    return report();
}