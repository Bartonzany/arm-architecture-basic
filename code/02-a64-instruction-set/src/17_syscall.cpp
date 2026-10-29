// 17_syscall.cpp — 验证 §27 系统调用 SVC
//
// 概念：SVC（Supervisor Call）触发异常，以受控方式切到更高特权级（EL1），
//   应用程序借此调用操作系统。Linux AArch64 系统调用约定：
//   x8 = 系统调用号，x0-x5 = 参数，svc #0 陷入内核，返回值在 x0。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 17_syscall.cpp -o 17_syscall.s
//
// 注意：本 demo 需在 AArch64 真机或 qemu-user 上运行才能看到真实返回值；
//       交叉编译 + 反汇编可看到 SVC #0 指令本身。

#include <cstdint>
#include <cstdio>

// 直接用内联汇编发起 getpid 系统调用（__NR_getpid == 172 on AArch64 Linux）
// 预期汇编：
//   MOV X8, #172     ; 系统调用号
//   SVC #0           ; 陷入 EL1
//   （返回值已在 X0）
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

// 对照：同样的系统调用，走 libc 的 getpid()（内部同样会执行 SVC）
#include <unistd.h>

int main() {
    long a = sys_getpid_asm();
    long b = ::getpid();
    std::printf("svc_getpid=%ld libc_getpid=%ld\n", a, b);
    return (a == b) ? 0 : 1;   // 两者应返回同一个 PID
}