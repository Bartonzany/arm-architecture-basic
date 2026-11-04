// 06_matmul_f64.c —— 矩阵乘法（B5.1）：实数双精度浮点
//
// 对应文档：《SVE Programming Examples》B5.1 章「实数双精度浮点矩阵乘法」
// 验证点：矩阵乘法 C = A * B（行主序）。
//   - 朴素 C：三重循环标量乘累加。
//   - SVE2：svmla 融合乘加 + svdup 广播 + svld1 加载（向量长度无关）。
//
// 说明：文档的 matmul_f64 用 4 个累加器 + 4x 展开的向量长度无关写法；这里为
//   清晰起见用最简的"按行遍历、按列用向量、沿 k 广播累加"结构，指令对应 FMLA。

#include <arm_sve.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bench_util.h"

// 朴素 C 参考实现（行主序）
void matmul_f64_c(uint64_t M, uint64_t K, uint64_t N,
                  double *inLeft, double *inRight, double *out) {
    for (uint64_t x = 0; x < M; x++) {
        for (uint64_t y = 0; y < N; y++) {
            double acc = 0.0;
            for (uint64_t z = 0; z < K; z++) {
                acc += inLeft[x * K + z] * inRight[z * N + y];
            }
            out[x * N + y] = acc;
        }
    }
}

// SVE2 实现：svmla 融合乘加（每行输出向量化）
void matmul_f64_sve(uint64_t M, uint64_t K, uint64_t N,
                    double *inLeft, double *inRight, double *out) {
    uint64_t vl = svcntd();   // 每向量 64 位元素数（运行时）
    for (uint64_t x = 0; x < M; x++) {
        for (uint64_t y = 0; y < N; y += vl) {
            svbool_t pg = svwhilelt_b64((int64_t)y, (int64_t)N);
            svfloat64_t acc = svdup_f64(0);
            for (uint64_t z = 0; z < K; z++) {
                double a = inLeft[x * K + z];
                svfloat64_t va = svdup_f64(a);                       // 广播 a[x][z]
                svfloat64_t vb = svld1_f64(pg, inRight + z * N + y); // 加载 b[z][y..]
                acc = svmla_f64_x(pg, acc, va, vb);                  // acc += a * b
            }
            svst1_f64(pg, out + x * N + y, acc);
        }
    }
}

// 基准上下文与包装函数
static uint64_t g_M, g_K, g_N;
static double *g_A, *g_B, *g_C;
static void bench_c(void)   { matmul_f64_c(g_M, g_K, g_N, g_A, g_B, g_C); }
static void bench_sve(void) { matmul_f64_sve(g_M, g_K, g_N, g_A, g_B, g_C); }

int main(void) {
    enum { M = 8, K = 8, N = 8 };
    double A[M * K], B[K * N], C_c[M * N], C_sve[M * N];

    for (int i = 0; i < M * K; i++) A[i] = (double)(i % 4) * 0.5;
    for (int i = 0; i < K * N; i++) B[i] = (double)(i % 5) * 0.25;

    matmul_f64_c(M, K, N, A, B, C_c);
    matmul_f64_sve(M, K, N, A, B, C_sve);

    int ok = 1;
    for (int i = 0; i < M * N; i++) {
        if (C_c[i] != C_sve[i]) ok = 0;
    }

    printf("矩阵乘法(DP): 朴素 C C[0]=%.2f, SVE2 C[0]=%.2f  %s\n",
           C_c[0], C_sve[0], ok ? "一致 (PASS)" : "不一致 (FAIL)");

    // ===== 性能对比 =====
    const uint64_t BM = 64, BK = 64, BN = 64;
    double *bigA = malloc(sizeof(double) * BM * BK);
    double *bigB = malloc(sizeof(double) * BK * BN);
    double *bigC = malloc(sizeof(double) * BM * BN);
    for (uint64_t i = 0; i < BM * BK; i++) bigA[i] = (double)(i % 4) * 0.5;
    for (uint64_t i = 0; i < BK * BN; i++) bigB[i] = (double)(i % 5) * 0.25;
    g_M = BM; g_K = BK; g_N = BN; g_A = bigA; g_B = bigB; g_C = bigC;
    const long REPEAT = 10;
    const long MACS = (long)(BM * BK * BN);   // 一次乘法的乘加总数
    BenchResult results[] = {
        { "matmul_f64_c",   ns_per_op(bench_c,   REPEAT, MACS) },
        { "matmul_f64_sve", ns_per_op(bench_sve, REPEAT, MACS) },
    };
    print_bench_table("性能对比（每次操作 = 1 次乘加 MAC）", results, 2);
    free(bigA); free(bigB); free(bigC);

    return ok ? 0 : 1;
}
