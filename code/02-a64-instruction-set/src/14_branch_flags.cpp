// 14_branch_flags.cpp — 验证 §21/22 程序流：分支、循环与条件码
//
// 概念：B.cond 条件分支；CBZ/CBNZ 判零分支。条件码 N/Z/C/V 由带 S 后缀的
//   指令（ADDS/SUBS）或 CMP/TST 更新。CMP 是 SUBS XZR,Xn,Xm 的别名。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 14_branch_flags.cpp -o 14_branch_flags.s

#include <cstdint>
#include "test_util.hpp"

// ============================================================================
// 被测函数
// ============================================================================

// if (a == 5) b = 5; -> CMP + CSEL（-O2 用条件选择替代分支）
__attribute__((noinline))
std::uint32_t if_eq(std::uint32_t a) {
    std::uint32_t b = 0;
    if (a == 5) b = 5;
    return b;
}

// while (a != 0) { b+=c; --a; } -> 被强度削减为 MUL
__attribute__((noinline))
std::uint32_t while_nonzero(std::uint32_t a, std::uint32_t c) {
    std::uint32_t b = 0;
    while (a != 0) { b += c; --a; }
    return b;
}

// 求和循环：CBZ 判零 + CMP/B.CS 条件分支
__attribute__((noinline))
std::uint64_t sum_n(std::uint32_t n) {
    std::uint64_t s = 0;
    for (std::uint32_t i = 1; i <= n; ++i) s += i;
    return s;
}

// 条件码：a-b<0 判断（CMP + CSETM，依据 N 标志）
__attribute__((noinline))
int classify(int a, int b) {
    if (a - b < 0) return -1;
    return 0;
}

// ============================================================================
// 测试
// ============================================================================

static void test_branch_flags() {
    group("§21/22 分支、循环与条件码");

    CHECK_EQ(if_eq(5), 5u);              // a==5 -> 5
    CHECK_EQ(while_nonzero(4, 3), 12u);  // 4 次加 3
    CHECK_EQ(sum_n(100), 5050ull);       // 1+...+100
    CHECK_EQ(classify(3, 5), -1);        // 3-5 < 0
}

int main() {
    test_branch_flags();
    return report();
}