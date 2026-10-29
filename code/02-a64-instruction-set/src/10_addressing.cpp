// 10_addressing.cpp — 验证 §17 加载与存储（三）：寻址方式
//
// 概念：加载/存储的地址在方括号内，有四种寻址模式：
//   基址寄存器    LDR W0, [X1]          直接以 X1 为完整地址
//   偏移寻址      LDR W0, [X1, #12]      基址 + 立即数偏移（适合结构体成员）
//   前索引        LDR W0, [X1, #12]!     先更新指针再访问（循环遍历常用）
//   后索引        LDR W0, [X1], #12      先访问再更新指针（栈弹出常用）
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 10_addressing.cpp -o 10_addressing.s

#include <cstdint>
#include "test_util.hpp"

struct Rec { std::uint32_t a; std::uint32_t b; std::uint64_t c; };

// ============================================================================
// 被测函数
// ============================================================================

// 基址寻址：直接解引用，无偏移 -> LDR W0, [X0]
__attribute__((noinline))
std::uint32_t load_base(const std::uint32_t* p) {
    return *p;
}

// 偏移寻址：成员 c 在偏移 8 -> LDR X0, [X0, #8]
__attribute__((noinline))
std::uint64_t read_member_c(const Rec* r) {
    return r->c;
}

// 基址 + 寄存器偏移：a[i] -> LDR W0, [X0, X1, LSL #2]
__attribute__((noinline))
std::uint32_t read_index(const std::uint32_t* a, std::uint32_t i) {
    return a[i];
}

// 后索引（post-index）：先按旧地址访存，再指针自增。
//   汇编：LDR W1, [X2], #4   ->  w1 = mem[x2]; x2 += 4
//   C 等价：*p++   （本次读自增前的地址）
//   典型应用：出栈（先读旧 SP 指向的栈顶，再上移 SP）
__attribute__((noinline))
std::uint64_t sum_post_index(const std::uint32_t* a, int n) {
    std::uint64_t s = 0;
    for (int i = 0; i < n; ++i) s += a[i];   // 等价 s += *p++，累加 a[0],a[1],...
    return s;
}

// 前索引（pre-index）：先指针自增，再按新地址访存。
//   汇编：LDR W1, [X2, #4]!   ->  x2 += 4; w1 = mem[x2]
//   C 等价：*++p   （本次读自增后的地址）
//   典型应用：压栈（先下移 SP，再写入新栈顶）
//
// 两者净效果相同（指针最终都前移 4），唯一区别是"本次访存发生在
// 自增前还是自增后"：后索引读 p[0]，前索引读 p[1]。
__attribute__((noinline))
std::uint32_t sum_pre_index(std::uint32_t* p, int n) {
    std::uint32_t s = 0;
    for (int i = 0; i < n; ++i) s += *++p;   // 累加 p[1],p[2],...（跳过 p[0]）
    return s;
}

// ============================================================================
// 测试
// ============================================================================

static void test_addressing() {
    group("§17 寻址方式");

    Rec r{1, 2, 0xDEADBEEFull};
    std::uint32_t arr[5] = {1, 2, 3, 4, 5};

    CHECK_EQ(load_base(arr), 1u);                    // 基址寻址
    CHECK_EQ(read_member_c(&r), 0xDEADBEEFull);      // 偏移寻址
    CHECK_EQ(read_index(arr, 2), 3u);                // 寄存器偏移
    CHECK_EQ(sum_post_index(arr, 4), 10ull);         // 后索引：1+2+3+4
    CHECK_EQ(sum_pre_index(arr, 4), 14u);            // 前索引：2+3+4+5
}

int main() {
    test_addressing();
    return report();
}