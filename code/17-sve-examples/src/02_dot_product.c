// 02_dot_product.c —— 向量点积（B3/A3.5）：朴素 C vs SVE2 SDOT 点积
//
// 对应文档：《SVE Programming Examples》A3.5 点积 / B3 章「向量点积」
// 验证点：SVE/SVE2 的 SDOT 指令 —— 每 4 个连续 8 位元素相乘并求和（加宽到 32 位），
//   一条指令完成"4 次乘 + 4 次加 + 累加"。
//   - 朴素 C：标量乘累加循环。
//   - SVE2：svld1 谓词加载 + svdot_s32 点积累加 + svaddv 归约。
// 文档 B3 章还给出复数 SP / 实数 HP / 复数 16 位整数点积（用 CDOT 等），本 demo
// 演示最基础的实数 8 位整数 → 32 位整数结果这一种（SDOT）。

#include <arm_sve.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bench_util.h"

// 朴素 C 参考实现：8 位整数点积，累加为 32 位
int32_t dot_product(const int8_t *a, const int8_t *b, int n) {
    int32_t acc = 0;
    for (int i = 0; i < n; i++) {
        acc += (int32_t)a[i] * b[i];
    }
    return acc;
}

// SVE2 实现：SDOT 点积
int32_t dot_product_sve(const int8_t *a, const int8_t *b, int n) {
    svint32_t acc = svdup_s32(0);
    for (int i = 0; i < n; i += (int)svcntb()) {
        svbool_t pg = svwhilelt_b8(i, n);
        svint8_t va = svld1_s8(pg, a + i);
        svint8_t vb = svld1_s8(pg, b + i);
        acc = svdot_s32(acc, va, vb);   // 每 4 个 8 位元素一组点积累加
    }
    return svaddv_s32(svptrue_b32(), acc);  // 跨通道求和归约
}

// 基准上下文与包装函数
static const int8_t *g_a, *g_b;
static int g_n;
static volatile int32_t g_sink;
static void bench_c(void)   { g_sink = dot_product(g_a, g_b, g_n); }
static void bench_sve(void) { g_sink = dot_product_sve(g_a, g_b, g_n); }

int main(void) {
    enum { N = 64 };
    int8_t a[N], b[N];
    for (int i = 0; i < N; i++) {
        a[i] = (int8_t)(i - 32);
        b[i] = (int8_t)((i % 7) - 3);
    }

    int32_t r_c   = dot_product(a, b, N);
    int32_t r_sve = dot_product_sve(a, b, N);

    printf("向量点积: 朴素 C = %d, SVE2 = %d  %s\n",
           r_c, r_sve, (r_c == r_sve) ? "(一致 PASS)" : "(不一致 FAIL)");

    // ===== 性能对比 =====
    const int BN = 1 << 20;
    int8_t *biga = malloc(sizeof(int8_t) * BN);
    int8_t *bigb = malloc(sizeof(int8_t) * BN);
    for (int i = 0; i < BN; i++) { biga[i] = (int8_t)(i - 128); bigb[i] = (int8_t)((i % 7) - 3); }
    g_a = biga; g_b = bigb; g_n = BN;
    const long REPEAT = 50;
    BenchResult results[] = {
        { "dot_product",     ns_per_op(bench_c,   REPEAT, BN) },
        { "dot_product_sve", ns_per_op(bench_sve, REPEAT, BN) },
    };
    print_bench_table("性能对比（每次操作 = 1 次乘累加）", results, 2);
    free(biga); free(bigb);

    return (r_c == r_sve) ? 0 : 1;
}
