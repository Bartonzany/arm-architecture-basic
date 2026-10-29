// 16_function_pcs.cpp — 验证 §25/26 函数调用 BL/RET 与过程调用标准 PCS
//
// 概念：BL 带链接分支（返回地址写 LR=X30）；RET 执行到 LR 的间接分支。
//   PCS 规定：参数走 X0-X7（32 位类型用 W0-W7），结果用 X0/W0 返回；
//   X9-X15 可破坏；X19-X28 被调用者保存；X29=FP，X30=LR。
//   浮点参数走 D0-D7。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 16_function_pcs.cpp -o 16_function_pcs.s

#include <cstdint>

// 入参 4 个走 W0-W3，结果走 W0；对应文档 §28 自测问题 1 的变体
__attribute__((noinline))
std::uint64_t foo(std::uint64_t a, std::uint64_t b) {
    return (a << 1) + b;            // a 在 X0，b 在 X1，结果 W0 返回
}

// 多参数+被调用者保存：使用 X19 这类 callee-saved 寄存器，
// 编译器会 STP 保存、LDP 恢复
__attribute__((noinline))
std::uint64_t use_callee_saved(std::uint64_t x) {
    std::uint64_t acc = x;
    for (int i = 0; i < 10; ++i) acc += i;   // 累加循环可能占 X19
    return acc;
}

// 返回结构体：由调用者分配内存，XR(X8) 指向返回内存
struct Big { std::uint64_t a, b, c; };

__attribute__((noinline))
Big return_struct(std::uint64_t v) {
    Big r{v, v + 1, v + 2};         // 通过 X8 指向的缓冲写回
    return r;
}

#include <cstdio>

int main() {
    std::uint64_t r1 = foo(2, 3);                   // (2<<1)+3 = 7
    std::uint64_t r2 = use_callee_saved(100);        // 100 + 45 = 145
    Big r3 = return_struct(10);
    std::printf("foo=%llu cs=%llu struct=%llu,%llu,%llu\n",
                (unsigned long long)r1,
                (unsigned long long)r2,
                (unsigned long long)r3.a,
                (unsigned long long)r3.b,
                (unsigned long long)r3.c);
    return (r1 == 7 && r2 == 145 && r3.a == 10 && r3.c == 12) ? 0 : 1;
}