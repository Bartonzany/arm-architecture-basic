// 05_fir_filter.c —— FIR 滤波器（B4）：实数单精度浮点
//
// 对应文档：《SVE Programming Examples》B4 章「FIR 滤波器」
// 验证点：FIR 滤波 y[n] = Σ h[j] * x[n+j]（系数已按倒序存放）。
//   - 朴素 C：双重循环标量乘累加。
//   - SVE2：svmla（FMLA）融合乘加 + svdup 广播系数。
//
// 说明：这里 SVE2 版演示"用内建函数写乘累加"，为清晰起见按朴素结构逐输出点
//   计算（与文档里跨输出位置向量化的版本不同，但结果一致、指令对应 FMLA）。

#include <arm_sve.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bench_util.h"

// 朴素 C 参考实现（系数 h 已按倒序存放）
void fir_c(float *x, float *h, float *y, int32_t n, int32_t t) {
    for (int64_t i = 0; i < n; i++) {
        float acc = 0;
        for (int64_t j = 0; j < t; j++) {
            acc += x[i + j] * h[j];
        }
        y[i] = acc;
    }
}

// SVE2 实现：按输出位置向量化，svmla 融合乘加 + svdup 广播系数
// 每个向量通道计算一个输出 y[i+k] = Σ_j x[i+k+j] * h[j]（k 为通道索引）
void fir_sve(float *x, float *h, float *y, int32_t n, int32_t t) {
    for (int64_t i = 0; i < n; i += (int64_t)svcntw()) {
        svbool_t pg = svwhilelt_b32(i, (int64_t)n);
        svfloat32_t acc = svdup_f32(0);
        for (int64_t j = 0; j < t; j++) {
            svfloat32_t hb = svdup_f32(h[j]);              // 广播系数 h[j]
            svfloat32_t xs = svld1_f32(pg, x + i + j);     // 加载 x[i+j..]（向量）
            acc = svmla_f32_m(pg, acc, xs, hb);            // acc += x*h[j]
        }
        svst1_f32(pg, y + i, acc);
    }
}

// 基准上下文与包装函数
static float *g_x, *g_h, *g_y;
static int32_t g_n, g_t;
static void bench_c(void)   { fir_c(g_x, g_h, g_y, g_n, g_t); }
static void bench_sve(void) { fir_sve(g_x, g_h, g_y, g_n, g_t); }

int main(void) {
    enum { N = 16, T = 4 };
    float x[N + T - 1], h[T], y_c[N], y_sve[N];

    for (int i = 0; i < N + T - 1; i++) x[i] = (float)(i % 5);
    for (int j = 0; j < T; j++) h[j] = (float)(j + 1) * 0.5f;

    fir_c(x, h, y_c, N, T);
    fir_sve(x, h, y_sve, N, T);

    int ok = 1;
    for (int i = 0; i < N; i++) {
        if (y_c[i] != y_sve[i]) ok = 0;
    }

    printf("FIR 滤波器: 朴素 C y[0]=%.2f, SVE2 y[0]=%.2f  %s\n",
           y_c[0], y_sve[0], ok ? "一致 (PASS)" : "不一致 (FAIL)");

    // ===== 性能对比 =====
    const int32_t BN = 1 << 14, BT = 16;
    float *bigx = malloc(sizeof(float) * (BN + BT - 1));
    float *bigh = malloc(sizeof(float) * BT);
    float *bigy = malloc(sizeof(float) * BN);
    for (int i = 0; i < BN + BT - 1; i++) bigx[i] = (float)(i % 5);
    for (int j = 0; j < BT; j++) bigh[j] = (float)(j + 1) * 0.5f;
    g_x = bigx; g_h = bigh; g_y = bigy; g_n = BN; g_t = BT;
    const long REPEAT = 20;
    BenchResult results[] = {
        { "fir_c",   ns_per_op(bench_c,   REPEAT, BN) },
        { "fir_sve", ns_per_op(bench_sve, REPEAT, BN) },
    };
    print_bench_table("性能对比（每次操作 = 1 个输出点）", results, 2);
    free(bigx); free(bigh); free(bigy);

    return ok ? 0 : 1;
}
