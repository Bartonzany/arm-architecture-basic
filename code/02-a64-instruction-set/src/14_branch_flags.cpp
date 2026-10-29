// 14_branch_flags.cpp — 验证 §21/22 程序流：分支、循环与条件码
//
// 概念：B/BL 无条件分支；B.cond 条件分支；CBZ/CBNZ 判零分支；
//   TBZ/TBNZ 判位分支。条件码 N/Z/C/V 由带 S 后缀的指令（ADDS/SUBS）
//   或 CMP/TST 更新。CMP 是 SUBS XZR,Xn,Xm 的别名，TST 是 ANDS 的别名。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 14_branch_flags.cpp -o 14_branch_flags.s

#include <cstdint>

// if (a == 5) b = 5; -> CMP W0, #5 ; B.NE skip ; MOV ...
__attribute__((noinline))
std::uint32_t if_eq(std::uint32_t a) {
    std::uint32_t b = 0;
    if (a == 5) b = 5;
    return b;
}

// while (a != 0) { b+=c; a--; } -> CBZ 判零循环
__attribute__((noinline))
std::uint32_t while_nonzero(std::uint32_t a, std::uint32_t c) {
    std::uint32_t b = 0;
    while (a != 0) { b += c; --a; }
    return b;
}

// 求和：用循环 + 计数，观察 SUBS/CBNZ 组合（比较更新标志后分支）
__attribute__((noinline))
std::uint64_t sum_n(std::uint32_t n) {
    std::uint64_t s = 0;
    for (std::uint32_t i = 1; i <= n; ++i) s += i;
    return s;
}

// 条件码：利用带 S 的减法结果做符号判断（B.LT 依据 N/V 标志）
__attribute__((noinline))
int classify(int a, int b) {
    if (a - b < 0) return -1;   // SUBS 后 B.GE 跳过 / 依据 N
    return 0;
}

#include <cstdio>

int main() {
    std::printf("if_eq=%u while=%u sum=%llu classify=%d\n",
                (unsigned)if_eq(5),
                (unsigned)while_nonzero(4, 3),
                (unsigned long long)sum_n(100),
                (int)classify(3, 5));
    return (if_eq(5) == 5 && while_nonzero(4, 3) == 12
            && sum_n(100) == 5050 && classify(3, 5) == -1) ? 0 : 1;
}