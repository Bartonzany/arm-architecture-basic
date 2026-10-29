// 18_syscall.cpp — 验证 §27 系统调用 SVC / HVC / SMC
//
// 概念：三类系统调用指令都触发异常、受控切到更高特权级：
//   SVC（Supervisor Call）      面向 EL1，应用程序调用操作系统
//   HVC（Hypervisor Call）      面向 EL2，OS 调用虚拟机监控器；EL0 不可用
//   SMC（Secure Monitor Call）  面向 EL3，调用安全固件；EL0 不可用
//   Linux AArch64 系统调用约定：x8 = 系统调用号，svc #0 陷入内核，返回值在 x0。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 18_syscall.cpp -o 18_syscall.s
//
// 注意：HVC/SMC 在 EL0（用户态）不可用，运行会触发异常导致进程崩溃，
//       本文件仅通过交叉编译 + 反汇编展示其指令形式；运行测试只调用 SVC。

#include <cstdint>
#include <unistd.h>
#include "test_util.hpp"

// ============================================================================
// 被测函数
// ============================================================================

// SVC：内联汇编发起 getpid 系统调用（__NR_getpid == 172 on AArch64 Linux）
//   预期汇编：MOV X8, #172 ; SVC #0
__attribute__((noinline))
long sys_getpid_asm() {
    register long num __asm__("x8") = 172;   // 系统调用号（getpid）
    register long ret __asm__("x0");         // 返回值寄存器
    __asm__ volatile("svc #0"
                     : "=r"(ret)
                     : "r"(num)
                     : "memory", "cc");
    return ret;                              // 返回值在 x0
}

// HVC：虚拟机监控器调用，面向 EL2（EL0 不可用，仅展示汇编）
__attribute__((noinline))
void hvc_call() {
    __asm__ volatile("hvc #0");              // 预期汇编：HVC #0x0
}

// SMC：安全监控器调用，面向 EL3（EL0 不可用，仅展示汇编）
__attribute__((noinline))
void smc_call() {
    __asm__ volatile("smc #0");              // 预期汇编：SMC #0x0
}

// ============================================================================
// 测试
// ============================================================================

static void test_syscall() {
    group("§27 系统调用 SVC / HVC / SMC");

    // SVC 可在 EL0 使用：内联汇编 svc 与 libc getpid 应返回同一进程 PID
    CHECK_EQ(sys_getpid_asm(), static_cast<long>(::getpid()));

    // HVC/SMC 在 EL0 触发异常（进程崩溃），故不在此调用；
    // 指令形式见反汇编中的 hvc #0x0 / smc #0x0。
}

int main() {
    test_syscall();
    return report();
}