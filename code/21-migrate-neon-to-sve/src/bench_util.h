// bench_util.h — 轻量基准测试辅助（C，零依赖、自包含）
//
// 把各 demo 的性能对比统一成一致的工程化输出：
//   - 每个被测函数独立计时（含预热、重复取平均）
//   - 结果归一化到"单次逻辑操作"的纳秒数
//   - 统一打印"函数 / 每次操作耗时 / 加速比"表格（第一个结果为 1x 基准）
//
// 用法（C 无闭包，用 static 包装函数 + static 上下文）：
//
//   #include "bench_util.h"
//
//   static int g_n; static float *g_x, *g_y;   // 基准输入
//   static void bench_scalar(void) { scalar_impl(g_x, g_y, g_n); }
//   static void bench_sve(void)    { sve_impl(g_x, g_y, g_n); }
//
//   BenchResult results[] = {
//       { "scalar_impl", ns_per_op(bench_scalar, REPEAT, OPS) },
//       { "sve_impl",    ns_per_op(bench_sve,    REPEAT, OPS) },
//   };
//   print_bench_table("标题", results, 2);

#ifndef ARM64_VERIFY_BENCH_UTIL_H
#define ARM64_VERIFY_BENCH_UTIL_H

#include <stdio.h>
#include <time.h>

typedef struct {
    const char *name;       // 被测函数名
    double      ns_per_op;  // 每次逻辑操作耗时（ns）
    double      speedup;    // 加速比（基准 = 1x，由 print_bench_table 计算）
} BenchResult;

// 计时：重复运行 fn 共 repeat 次，返回每次逻辑操作的纳秒数。
//   repeat        —— 重复测量次数（用于取平均）
//   ops_per_call  —— fn() 单次执行完成的"逻辑操作"数，用于归一化
static inline double ns_per_op(void (*fn)(void), long repeat, long ops_per_call) {
    fn();  // 预热一次，减少冷启动开销

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (long r = 0; r < repeat; r++) {
        fn();
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);

    double ns = (double)(t1.tv_sec - t0.tv_sec) * 1e9 + (double)(t1.tv_nsec - t0.tv_nsec);
    return ns / (double)(repeat * ops_per_call);
}

// 打印性能对比表（以 results[0] 作为 1x 基准）
static inline void print_bench_table(const char *title, BenchResult *results, int count) {
    double base = results[0].ns_per_op;
    for (int i = 0; i < count; i++) {
        results[i].speedup = base / results[i].ns_per_op;
    }

    printf("\n===== %s =====\n", title);
    printf("%-28s  %14s  %10s\n", "函数", "每次操作耗时", "加速比");
    printf("%-28s  %14s  %10s\n", "----------------------------", "--------------", "----------");
    for (int i = 0; i < count; i++) {
        printf("%-28s  %12.3f ns  %9.3fx\n",
               results[i].name, results[i].ns_per_op, results[i].speedup);
    }
    printf("\n");
}

#endif
