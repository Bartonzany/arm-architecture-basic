// 02_async_interrupt.cpp — 验证 §6 异步异常（中断）
//
// 概念：异步异常（中断）由外部产生，与当前指令流不同步，随时可能打断执行。
//   AArch64 的异步异常是硬件中断（IRQ/FIQ/SError，§6），用户态无法直接触发；
//   本文件用 POSIX 信号模拟"外部事件异步打断执行流"的机制：
//     SIGALRM 定时器到期（对应 §6.1 定时器/设备中断）
//   信号由内核通过"硬件 IRQ -> 异常处理 -> 信号投递"的流程送达，
//   在用户态间接体现了异步异常的"外部、随时打断"特性。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 02_async_interrupt.cpp -o 02_async_interrupt.s

#include <cstdint>
#include <csignal>
#include <unistd.h>
#include "test_util.hpp"

static volatile sig_atomic_t g_interrupted = 0;

// "中断处理程序"：异步事件到来时被内核调用（模拟 ISR）
static void irq_like_handler(int sig) {
    g_interrupted = 1;          // 模拟 ISR 置标志位
}

// 忙循环工作，等待定时器"中断"异步打断执行流
__attribute__((noinline))
int busy_work_with_interrupt() {
    signal(SIGALRM, irq_like_handler);        // 注册"中断处理程序"
    alarm(1);                                 // 1 秒后定时器"中断"

    volatile int acc = 0;
    while (!g_interrupted) {                  // 被 SIGALRM 异步打断
        ++acc;
    }
    return acc;                               // 返回中断发生前的迭代次数
}

// ============================================================================
// 测试
// ============================================================================

static void test_async_interrupt() {
    group("§6 异步异常（定时器中断模拟）");

    g_interrupted = 0;
    int iters = busy_work_with_interrupt();

    CHECK(g_interrupted == 1);   // 定时器"中断"确实异步发生了
    CHECK(iters > 0);            // 中断发生前执行了若干次迭代
}

int main() {
    test_async_interrupt();
    return report();
}