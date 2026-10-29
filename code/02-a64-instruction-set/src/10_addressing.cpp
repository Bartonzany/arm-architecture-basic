// 10_addressing.cpp — 验证 §17 加载与存储（三）：寻址方式
//
// 概念：加载/存储的地址在方括号内，有几种寻址模式：
//   基址寄存器    LDR W0, [X1]
//   偏移寻址      LDR W0, [X1, #12]（适合结构体成员）
//   前索引        LDR W0, [X1, #12]!（先更新指针再访问）
//   后索引        LDR W0, [X1], #12（先访问再更新指针，栈弹出常用）
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 10_addressing.cpp -o 10_addressing.s

#include <cstdint>

struct Rec { std::uint32_t a; std::uint32_t b; std::uint64_t c; };

// 偏移寻址：成员 b 在偏移 4，c 在偏移 8 -> LDR W0, [X0, #4] / LDR X0, [X0, #8]
__attribute__((noinline))
std::uint64_t read_member_c(const Rec* r) { return r->c; }

// 基址 + 寄存器偏移：a[i] -> LDR W0, [X0, X1, LSL #2]
__attribute__((noinline))
std::uint32_t read_index(const std::uint32_t* a, std::uint32_t i) {
    return a[i];
}

// 后索引：数组求和循环，编译器生成 LDR W1, [X2], #4（读后指针自增）
__attribute__((noinline))
std::uint64_t sum_post_index(const std::uint32_t* a, int n) {
    std::uint64_t s = 0;
    for (int i = 0; i < n; ++i) s += a[i];
    return s;
}

#include <cstdio>

int main() {
    Rec r{1, 2, 0xDEADBEEFull};
    std::uint32_t arr[4] = {1, 2, 3, 4};
    std::printf("member=%llx indexed=%u sum=%llu\n",
                (unsigned long long)read_member_c(&r),
                (unsigned)read_index(arr, 2),
                (unsigned long long)sum_post_index(arr, 4));
    return (read_member_c(&r) == 0xDEADBEEFull && read_index(arr, 2) == 3
            && sum_post_index(arr, 4) == 10) ? 0 : 1;
}