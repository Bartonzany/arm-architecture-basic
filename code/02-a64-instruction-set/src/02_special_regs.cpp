// 02_special_regs.cpp — 验证 §7 特殊寄存器：XZR / SP / LR / ADR
//
// 概念：除了通用寄存器，还有几个特殊角色：
//   XZR/WZR  零寄存器：读恒为 0，写被忽略
//   SP       栈指针：作为加载/存储基址
//   X30=LR   链接寄存器：保存函数返回地址
//   ADR Xd,. 计算标签地址，等效读取 PC（PC 不是通用寄存器）
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 02_special_regs.cpp -o 02_special_regs.s

#include <cstdint>
#include "test_util.hpp"

// ============================================================================
// 被测函数
// ============================================================================

// 返回 0：编译器用立即数 0 或 XZR 置零
__attribute__((noinline))
std::uint64_t zero_via_xzr() {
    return 0;    // 实际：MOV X0, #0x0
}

// 读取"当前地址"：内联汇编 ADR 拿 PC（A64 中 PC 不是通用寄存器）
__attribute__((noinline))
std::uintptr_t read_pc_like() {
    std::uintptr_t addr;
    __asm__ volatile("adr %0, ." : "=r"(addr));    // ADR Xd, . -> 当前地址
    return addr;
}

// SP 作基址：带局部变量的函数（简单局部变量可能被寄存器化）
__attribute__((noinline))
std::uint64_t sp_based_access(std::uint64_t x) {
    std::uint64_t local = x + 1;    // 局部变量，-O2 下可能被寄存器化
    return local * 2;
}

// ============================================================================
// 测试
// ============================================================================

static void test_special_registers() {
    group("§7 特殊寄存器 XZR / SP / ADR");

    CHECK_EQ(zero_via_xzr(), 0ull);          // 零寄存器/立即数置零
    CHECK(read_pc_like() != 0);              // ADR 应返回非零的当前地址
    CHECK_EQ(sp_based_access(10), 22ull);    // (10+1)*2
}

int main() {
    test_special_registers();
    return report();
}