// 17_function_pcs.cpp — 验证 §25/26 函数调用 BL/RET 与过程调用标准 PCS
//
// 概念：BL 带链接分支（返回地址写 LR=X30）；RET 执行到 LR 的间接分支。
//   PCS 规定：参数走 X0-X7，结果用 X0 返回；X19-X28 被调用者保存；
//   X29=FP，X30=LR；返回结构体时 X8 作为间接结果寄存器（XR）。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 17_function_pcs.cpp -o 17_function_pcs.s

#include <cstdint>
#include "test_util.hpp"

struct Big { std::uint64_t a, b, c; };

// ============================================================================
// 被测函数
// ============================================================================

// 参数走 X0/X1，结果走 X0
__attribute__((noinline))
std::uint64_t foo(std::uint64_t a, std::uint64_t b) {
    return (a << 1) + b;            // ADD X0, X1, X0, LSL #1
}

// 累加循环（-O2 下被常量折叠为 +0x2d）
__attribute__((noinline))
std::uint64_t use_callee_saved(std::uint64_t x) {
    std::uint64_t acc = x;
    for (int i = 0; i < 10; ++i) acc += i;
    return acc;
}

// 返回结构体：X8 指向调用者分配的内存
__attribute__((noinline))
Big return_struct(std::uint64_t v) {
    Big r{v, v + 1, v + 2};         // STP X0, X2, [X8] ; STR X1, [X8, #16]
    return r;
}

// ============================================================================
// 测试
// ============================================================================

static void test_function_pcs() {
    group("§25/26 函数调用与 PCS");

    CHECK_EQ(foo(2, 3), 7ull);             // (2<<1)+3 = 7
    CHECK_EQ(use_callee_saved(100), 145ull);  // 100 + (0+...+9=45) = 145

    Big r = return_struct(10);
    CHECK_EQ(r.a, 10ull);
    CHECK_EQ(r.b, 11ull);
    CHECK_EQ(r.c, 12ull);
}

int main() {
    test_function_pcs();
    return report();
}