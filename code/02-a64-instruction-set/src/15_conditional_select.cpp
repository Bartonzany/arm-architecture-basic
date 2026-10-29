// 15_conditional_select.cpp — 验证 §23 程序流（三）：条件选择指令家族
//
// 概念：条件选择指令用"按条件在值间选择"替代分支，序列更短、无跳转，
//   对分支预测更友好。§23 的主要成员（本文件逐一验证）：
//     CSEL   if cond then Xd=Xn else Xd=Xm        基本二选一
//     CSINC  if cond then Xd=Xn else Xd=Xm+1      选择 + 加一
//     CINC   if cond then Xd=Xn+1 else Xd=Xn      CSINC 别名（Xn=Xm，条件自增）
//     CSET   Xd = (cond ? 1 : 0)                  CSINC 别名（Xn=Xm=XZR，条件置位）
//     CNEG   if cond then Xd=-Xn else Xd=Xn       条件取负
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 15_conditional_select.cpp -o 15_conditional_select.s

#include <cstdint>
#include "test_util.hpp"

// ============================================================================
// 被测函数
// ============================================================================

// CSEL：两个变量二选一（两个源都在寄存器，直接 CSEL）
__attribute__((noinline))
std::int32_t select_reg(std::int32_t a, std::int32_t b, int cond) {
    return cond ? a : b;                // CSEL W0, W1, W0, EQ
}

// CSINC：cond ? x : (y + 1)
__attribute__((noinline))
std::int32_t select_inc(std::int32_t x, std::int32_t y, int cond) {
    return cond ? x : (y + 1);          // CSINC W0, W0, W1, NE
}

// CINC：cond ? (x + 1) : x（条件自增）
__attribute__((noinline))
std::int32_t inc_cond(std::int32_t x, int cond) {
    return cond ? (x + 1) : x;          // CINC W0, W0, NE
}

// CSET：cond ? 1 : 0（条件置位）
__attribute__((noinline))
std::int32_t set_cond(int cond) {
    return cond ? 1 : 0;                // CSET W0, NE
}

// CNEG：cond ? -x : x（条件取负，绝对值即其特例）
__attribute__((noinline))
std::int64_t neg_cond(std::int64_t x, int cond) {
    return cond ? -x : x;               // CNEG X0, X0, NE
}

// ============================================================================
// 测试
// ============================================================================

static void test_conditional_select() {
    group("§23 条件选择 CSEL / CSINC / CINC / CSET / CNEG");

    CHECK_EQ(select_reg(10, 20, 1), 10);   // cond 真 -> a
    CHECK_EQ(select_reg(10, 20, 0), 20);   // cond 假 -> b

    CHECK_EQ(select_inc(100, 5, 1), 100);  // cond 真 -> x
    CHECK_EQ(select_inc(100, 5, 0), 6);    // cond 假 -> y+1

    CHECK_EQ(inc_cond(10, 1), 11);         // cond 真 -> x+1
    CHECK_EQ(inc_cond(10, 0), 10);         // cond 假 -> x

    CHECK_EQ(set_cond(1), 1);              // cond 真 -> 1
    CHECK_EQ(set_cond(0), 0);              // cond 假 -> 0

    CHECK_EQ(neg_cond(5, 1), -5ll);        // cond 真 -> -x
    CHECK_EQ(neg_cond(5, 0), 5ll);         // cond 假 -> x
}

int main() {
    test_conditional_select();
    return report();
}