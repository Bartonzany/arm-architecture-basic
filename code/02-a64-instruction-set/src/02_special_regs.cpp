// 02_special_regs.cpp — 验证 §7 特殊寄存器：XZR / SP / LR / ADR
//
// 概念：除了通用寄存器，还有几个特殊角色：
//   XZR/WZR  零寄存器：读恒为 0，写被忽略（编译器用 MOV X0, XZR 置零）
//   SP       栈指针：作为加载/存储基址
//   X30=LR   链接寄存器：保存函数返回地址
//   ADR Xd,. 计算标签地址，等效读取 PC（PC 不是通用寄存器）
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 02_special_regs.cpp -o 02_special_regs.s

#include <cstdint>

// 返回 0：编译器通常用 MOV W0, WZR / WZR 置零，而不是加载常量 0
__attribute__((noinline))
std::uint64_t zero_via_xzr() {
    return 0;                           // 预期：MOV X0, XZR
}

// 读取"当前地址"：用内联汇编 ADR 拿 PC（A64 中 PC 不是通用寄存器，
// 必须借 ADR 这类指令间接获取当前指令地址）
__attribute__((noinline))
std::uintptr_t read_pc_like() {
    std::uintptr_t addr;
    __asm__ volatile("adr %0, ." : "=r"(addr));  // ADR Xd, . -> 当前地址
    return addr;
}

// 验证 SP 作为基址：一个用到栈局部变量的函数，观察 ldr/str 以 SP 为基址
__attribute__((noinline))
std::uint64_t sp_based_access(std::uint64_t x) {
    std::uint64_t local = x + 1;        // 局部变量位于栈上，通过 SP 相对寻址
    return local * 2;
}

#include <cstdio>

int main() {
    std::uint64_t z = zero_via_xzr();
    std::uintptr_t pc = read_pc_like();
    std::uint64_t s = sp_based_access(10);
    std::printf("zero=%llu pc=%p sp_access=%llu\n",
                (unsigned long long)z, (void*)pc, (unsigned long long)s);
    return (z == 0 && pc != 0 && s == 22) ? 0 : 1;
}
