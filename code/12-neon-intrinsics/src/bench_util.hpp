// bench_util.hpp — 轻量基准测试辅助（零依赖、自包含）
//
// 把各 demo 的性能对比统一成一致的工程化输出：
//   - 每个被测函数独立计时（可设重复次数取平均，降低噪声）
//   - 结果归一化到"单次逻辑操作"的纳秒数
//   - 统一打印"函数 / 每次调用耗时 / 加速比"表格（第一个结果为 1x 基准）
//
// 用法：
//
//   #include "bench_util.hpp"
//
//   auto bench_scalar = [&]() { scalar_impl(...); };
//   auto bench_neon   = [&]() { neon_impl(...);   };
//
//   const long REPEAT = 20;     // 重复测量次数（取平均）
//   const long OPS    = 1024;   // 每次调用完成的逻辑操作数（用于归一化）
//
//   BenchResult results[] = {
//       { "scalar_impl", ns_per_op(bench_scalar, REPEAT, OPS) },
//       { "neon_impl",   ns_per_op(bench_neon,   REPEAT, OPS) },
//   };
//   print_bench_table("标题", results, 2);
//
// 说明：speedup 在 print_bench_table 内按 results[0]（基准）自动计算，无需手动填。

#ifndef ARM64_VERIFY_BENCH_UTIL_HPP
#define ARM64_VERIFY_BENCH_UTIL_HPP

#include <cstdio>
#include <chrono>

struct BenchResult {
    const char* name;       // 被测函数名
    double      ns_per_op;  // 每次逻辑操作耗时（ns）
    double      speedup;    // 加速比（基准 = 1x，由 print_bench_table 计算）
};

// 计时：重复运行 fn 共 repeat 次，返回每次逻辑操作的纳秒数。
//   repeat        —— 重复测量次数（用于取平均）
//   ops_per_call  —— fn() 单次执行完成的"逻辑操作"数，用于归一化
template <typename F>
double ns_per_op(F fn, long repeat, long ops_per_call) {
    // 预热一次，减少首次调用的冷启动开销
    fn();

    auto t0 = std::chrono::steady_clock::now();
    for (long r = 0; r < repeat; r++) {
        fn();
    }
    auto t1 = std::chrono::steady_clock::now();

    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    return ns / static_cast<double>(repeat * ops_per_call);
}

// 打印性能对比表（以 results[0] 作为 1x 基准）
inline void print_bench_table(const char* title, BenchResult* results, int count) {
    double base = results[0].ns_per_op;
    for (int i = 0; i < count; i++) {
        results[i].speedup = base / results[i].ns_per_op;
    }

    std::printf("\n===== %s =====\n", title);
    std::printf("%-32s  %14s  %10s\n", "函数", "每次调用耗时", "加速比");
    std::printf("%-32s  %14s  %10s\n", "--------------------------------", "--------------", "----------");
    for (int i = 0; i < count; i++) {
        std::printf("%-32s  %12.3f ns  %9.3fx\n",
                    results[i].name, results[i].ns_per_op, results[i].speedup);
    }
    std::printf("\n");
}

#endif
