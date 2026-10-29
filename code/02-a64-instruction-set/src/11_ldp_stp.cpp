// 11_ldp_stp.cpp — 验证 §18 加载与存储（四）：成对访问 LDP/STP
//
// 概念：LDP/STP 一次搬运两个寄存器与内存之间的数据，常用于压栈/出栈：
//   STP X0, X1, [SP, #-16]!  压栈（SP 先减 16 再存两个 64 位值）
//   LDP X0, X1, [SP], #16    出栈（先读两个值再 SP 加 16）
//   AArch64 栈指针必须 16 字节对齐；LDP/STP 按两次独立访问处理。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 11_ldp_stp.cpp -o 11_ldp_stp.s

#include <cstdint>

// 交换两个 64 位值：返回 pair，编译器可用 LDP 成对加载
struct Pair { std::uint64_t lo, hi; };

__attribute__((noinline))
Pair make_pair(std::uint64_t lo, std::uint64_t hi) {
    Pair p{lo, hi};                 // 两次相邻 64 位存储
    return p;
}

// 把两个相邻 64 位读出来（编译器可能用 LDP X?, X?, [X0]）
__attribute__((noinline))
std::uint64_t read_two(const std::uint64_t* p) {
    return p[0] + p[1];
}

// 演示带栈帧的非叶子函数：prologue/epilogue 用 STP/LDP 保存 fp/lr
__attribute__((noinline))
std::uint64_t leaf(std::uint64_t x) { return x * 2; }

__attribute__((noinline))
std::uint64_t frame_demo(std::uint64_t x) {
    std::uint64_t local = x + 1;    // 触发建栈帧
    return local + leaf(x);
}

#include <cstdio>

int main() {
    Pair p = make_pair(0x11111111ull, 0x22222222ull);
    std::uint64_t arr[2] = {100, 200};
    std::printf("lo=%llx hi=%llx read_two=%llu frame=%llu\n",
                (unsigned long long)p.lo, (unsigned long long)p.hi,
                (unsigned long long)read_two(arr),
                (unsigned long long)frame_demo(5));
    return (p.lo == 0x11111111ull && read_two(arr) == 300 && frame_demo(5) == 16) ? 0 : 1;
}