// keep_neon.c —— 什么时候保留 Neon：固定小循环的 VLA 开销（Part Three）
//
// 对应文档：《Migrate Neon to SVE》第 5 章「什么时候保留 Neon 有用」
// 验证点：SVE 的 VLA 写法（svcntw + svwhilelt 谓词）有开销。对于"小而固定"的循环，
//   编译器知道迭代次数，可以完全展开 Neon 循环、省掉谓词；SVE VLA 写法反而要谓词。
//
//   - scalar：朴素 C，编译器 -O3 自动向量化（Neon）
//   - sve_vla：手工 SVE VLA 写法（svcntw + svwhilelt）——迁移后的形态
//
// 反汇编对照：
//   scalar（-O3 自动向量化）应该被完全展开成 2 条 Neon 向量指令（无谓词）
//   sve_vla 应该出现 whilelt 谓词 + 循环（谓词开销）
//
// 结论（文档原话）：有时 VLA 代码的开销不优于高度优化的 Neon 代码。

#include <arm_neon.h>
#include <arm_sve.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bench_util.h"

#define N 8

// 朴素 C，用 __attribute__((target)) 强制编译成 Neon（不含 SVE），
// 让编译器 -O3 完全展开成 Neon 向量指令（无谓词）
__attribute__((target("arch=armv8-a")))
void square_scalar(float *restrict x, const float *restrict y) {
    for (unsigned i = 0; i < N; i++) {
        x[i] = y[i] * y[i];
    }
}

// 手工 SVE VLA 写法（迁移后的形态）
void square_sve_vla(float *restrict x, const float *restrict y) {
    for (int64_t i = 0; i < N; i += (int64_t)svcntw()) {
        svbool_t pg = svwhilelt_b32(i, (int64_t)N);
        svfloat32_t v = svld1_f32(pg, y + i);
        svfloat32_t r = svmul_f32_x(pg, v, v);
        svst1_f32(pg, x + i, r);
    }
}

// 基准上下文（在固定 8 元素上反复跑，放大 VLA 开销）
static float g_x[N], g_y[N];
static void bench_scalar(void) { square_scalar(g_x, g_y); }
static void bench_sve(void)    { square_sve_vla(g_x, g_y); }

int main(void) {
    for (unsigned i = 0; i < N; i++) g_y[i] = (float)(i + 1);

    float xs[N], xv[N];
    square_scalar(xs, g_y);
    square_sve_vla(xv, g_y);

    int ok = 1;
    for (unsigned i = 0; i < N; i++) if (xs[i] != xv[i]) ok = 0;
    printf("固定 8 元素平方: scalar x[0]=%.0f SVE x[0]=%.0f  %s\n",
           xs[0], xv[0], ok ? "(一致 PASS)" : "(不一致 FAIL)");

    // 性能对比：固定小循环上，VLA 谓词开销可能让 SVE 不占优
    const long REPEAT = 100000;
    BenchResult results[] = {
        { "square_scalar(Neon 自动向量化)", ns_per_op(bench_scalar, REPEAT, N) },
        { "square_sve_vla(谓词)",            ns_per_op(bench_sve,    REPEAT, N) },
    };
    print_bench_table("性能对比（每次操作 = 1 个元素平方，固定 N=8）", results, 2);

    return ok ? 0 : 1;
}
