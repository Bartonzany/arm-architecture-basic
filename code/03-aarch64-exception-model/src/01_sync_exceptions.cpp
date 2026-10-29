// 01_sync_exceptions.cpp — 验证 §5 同步异常（异常类型 → Linux 信号）
//
// 概念：同步异常由当前指令引起，与指令流严格对齐。AArch64 的同步异常
//   在 Linux 用户态体现为信号（signal）：
//     非法内存访问（MMU 故障） -> SIGSEGV（§5.2 内存访问异常）
//     无效指令（udf）         -> SIGILL（§5.1 无效指令）
//     断点指令（brk，调试异常）-> SIGTRAP（§5.4 调试异常）
//   注意：AArch64 整数除零（sdiv/udiv）**不触发异常**，而是返回 0——
//   与 x86（div 除零触发 SIGFPE）不同，这是重要的架构差异。
//
// 本文件用 fork 子进程隔离触发各异常，父进程检查子进程收到的信号，
// 验证"异常类型 → 信号"的对应关系。
// 交叉编译 + 反汇编可看到触发异常的指令本身（ldr [xzr] / udf #0 / brk #0）。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 01_sync_exceptions.cpp -o 01_sync_exceptions.s

#include <cstdint>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include "test_util.hpp"

// ============================================================================
// 触发同步异常的函数（noinline，反汇编看指令）
// ============================================================================

// 空指针解引用：MMU 拒绝访问地址 0 -> SIGSEGV（§5.2 内存访问异常）
__attribute__((noinline))
void null_deref() {
    volatile int* p = nullptr;
    volatile int x = *p;                // LDR W0, [XZR]（访问地址 0，触发 SIGSEGV）
    (void)x;
}

// 无效指令：udf #0 -> SIGILL（§5.1 无效指令）
__attribute__((noinline))
void undefined_instruction() {
    __asm__ volatile("udf #0");         // UDF #0（永久未定义指令）
}

// 断点指令：brk #0 -> SIGTRAP（§5.4 调试异常/断点）
__attribute__((noinline))
void breakpoint_trap() {
    __asm__ volatile("brk #0");         // BRK #0（断点）
}

// 整数除法：AArch64 的 SDIV 除零返回 0，不触发异常（负例）
__attribute__((noinline))
int divide(int a, int b) {
    return a / b;                       // SDIV W0, W0, W1（b=0 时返回 0）
}

// ============================================================================
// 在子进程执行 fn，返回子进程被哪个信号终止（0 = 正常退出）
// ============================================================================

static int child_exit_signal(void (*fn)()) {
    pid_t pid = fork();
    if (pid == 0) {
        fn();           // 子进程触发异常
        _exit(0);       // 未触发异常则正常退出
    }
    int status;
    waitpid(pid, &status, 0);
    return WIFSIGNALED(status) ? WTERMSIG(status) : 0;
}

// ============================================================================
// 测试
// ============================================================================

static void test_sync_exceptions() {
    group("§5 同步异常 → Linux 信号");

    CHECK_EQ(child_exit_signal(null_deref), SIGSEGV);            // 内存访问异常
    CHECK_EQ(child_exit_signal(undefined_instruction), SIGILL);  // 无效指令
    CHECK_EQ(child_exit_signal(breakpoint_trap), SIGTRAP);       // 调试异常/断点

    // 负例：AArch64 整数除零不触发 SIGFPE，SDIV 返回 0
    CHECK_EQ(divide(1, 0), 0);
}

int main() {
    test_sync_exceptions();
    return report();
}