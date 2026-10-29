// 15_conditional_select.cpp — 验证 §23 程序流（三）：条件选择指令
//
// 概念：CSEL 按条件在两值间选择，可替代分支；CSINC 在"选择"外还加一；
//   CINC 是"按条件自增"的别名。对比分支实现，条件选择序列更短且无分支，
//   对分支预测更友好。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 15_conditional_select.cpp -o 15_conditional_select.s

#include <cstdint>

// if (a==0) b=6 else b=7 -> 编译器用条件指令替代分支（CSET/CSEL 类）
__attribute__((noinline))
std::uint32_t select_val(std::uint32_t a) {
    return (a == 0) ? 6u : 7u;
}

// if (a != 0) b = b + 1 -> CINC W1, W1, NE
__attribute__((noinline))
std::uint32_t cond_inc(std::uint32_t a, std::uint32_t b) {
    if (a != 0) b = b + 1;
    return b;
}

// 绝对值：编译器用 CSEL/CSET 类指令实现（尽量无分支）
__attribute__((noinline))
std::int64_t abs_val(std::int64_t x) {
    return x < 0 ? -x : x;
}

#include <cstdio>

int main() {
    std::printf("sel=%u cond_inc=%u abs=%lld\n",
                (unsigned)select_val(0),
                (unsigned)cond_inc(5, 10),
                (long long)abs_val(-42));
    return (select_val(0) == 6 && cond_inc(5, 10) == 11 && abs_val(-42) == 42) ? 0 : 1;
}