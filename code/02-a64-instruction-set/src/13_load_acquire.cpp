// 13_load_acquire.cpp — 验证 §20.1 Load-Acquire 与 Release（自带内存屏障）
//
// 概念：LDAR（load-acquire）/ STLR（store-release）单条指令即含隐式内存屏障，
//   用于并发下的内存序。消息传递模式：生产者 release 写 flag，消费者 acquire 读 flag。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 13_load_acquire.cpp -o 13_load_acquire.s

#include <atomic>
#include <cstdint>
#include "test_util.hpp"

std::atomic<std::uint32_t> data{0};
std::atomic<std::uint32_t> flag{0};

// ============================================================================
// 被测函数
// ============================================================================

// 生产者：普通写 data，然后 release 写 flag（STLR）
__attribute__((noinline))
void producer(std::uint32_t v) {
    data.store(v, std::memory_order_relaxed);  // 普通 STR
    flag.store(1, std::memory_order_release);  // STLR W0, [X1]
}

// 消费者：acquire 读 flag（LDAR），再普通读 data
__attribute__((noinline))
std::uint32_t consumer() {
    while (flag.load(std::memory_order_acquire) == 0) {}  // LDAR + CBZ 自旋
    return data.load(std::memory_order_relaxed);          // 普通 LDR
}

// ============================================================================
// 测试
// ============================================================================

static void test_load_acquire() {
    group("§20.1 Load-Acquire / Release");

    producer(0x42);
    CHECK_EQ(consumer(), 0x42u);   // acquire 读 flag 后读 data，应得到 0x42
}

int main() {
    test_load_acquire();
    return report();
}