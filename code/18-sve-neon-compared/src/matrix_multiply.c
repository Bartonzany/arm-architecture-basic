// matrix_multiply.c —— 矩阵乘法（列主序）：C / Neon / SVE 三种实现对比
//
// 对应文档：《SVE and Neon coding compared》第 7 章「内建函数」
// 验证点：同一个矩阵乘法（4x4 分块 + 融合乘加），Neon 和 SVE 内建函数的对应关系。
//   - matrix_multiply_c   ：朴素 C 三重循环（标量 FMUL/FADD）
//   - matrix_multiply_neon：Neon 内建函数（vld1q/vfmaq_laneq/vst1q），固定 128 位
//   - matrix_multiply_sve ：SVE 内建函数（svld1/svmla_lane/svst1 + 谓词），向量长度无关
//
// 关键差异：Neon 把"向量多宽"写死在类型里（float32x4_t）；SVE 用 svfloat32_t + 谓词，
//   n 维度不必是 4 的倍数（svwhilelt 生成谓词防越界）。B 的两个维度仍须是 4 的倍数。

#include <arm_neon.h>
#include <arm_sve.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bench_util.h"

// 朴素 C 参考实现（列主序）
void matrix_multiply_c(const float32_t *A, const float32_t *B, float32_t *C,
                       uint32_t n, uint32_t m, uint32_t k) {
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < m; j++) {
            C[n * j + i] = 0;
            for (uint32_t kk = 0; kk < k; kk++) {
                C[n * j + i] += A[n * kk + i] * B[k * j + kk];
            }
        }
    }
}

// Neon 实现：4x4 分块 + 融合乘加（n/m/k 须是 4 的倍数）
void matrix_multiply_neon(const float32_t *A, const float32_t *B, float32_t *C,
                          uint32_t n, uint32_t m, uint32_t k) {
    for (uint32_t i = 0; i < n; i += 4) {
        for (uint32_t j = 0; j < m; j += 4) {
            float32x4_t C0 = vdupq_n_f32(0), C1 = vdupq_n_f32(0);
            float32x4_t C2 = vdupq_n_f32(0), C3 = vdupq_n_f32(0);
            for (uint32_t kk = 0; kk < k; kk += 4) {
                uint32_t a_idx = i + n * kk;
                uint32_t b_idx = k * j + kk;
                float32x4_t A0 = vld1q_f32(A + a_idx);
                float32x4_t A1 = vld1q_f32(A + a_idx + n);
                float32x4_t A2 = vld1q_f32(A + a_idx + 2 * n);
                float32x4_t A3 = vld1q_f32(A + a_idx + 3 * n);

                float32x4_t B0 = vld1q_f32(B + b_idx);
                C0 = vfmaq_laneq_f32(C0, A0, B0, 0);
                C0 = vfmaq_laneq_f32(C0, A1, B0, 1);
                C0 = vfmaq_laneq_f32(C0, A2, B0, 2);
                C0 = vfmaq_laneq_f32(C0, A3, B0, 3);

                float32x4_t B1 = vld1q_f32(B + b_idx + k);
                C1 = vfmaq_laneq_f32(C1, A0, B1, 0);
                C1 = vfmaq_laneq_f32(C1, A1, B1, 1);
                C1 = vfmaq_laneq_f32(C1, A2, B1, 2);
                C1 = vfmaq_laneq_f32(C1, A3, B1, 3);

                float32x4_t B2 = vld1q_f32(B + b_idx + 2 * k);
                C2 = vfmaq_laneq_f32(C2, A0, B2, 0);
                C2 = vfmaq_laneq_f32(C2, A1, B2, 1);
                C2 = vfmaq_laneq_f32(C2, A2, B2, 2);
                C2 = vfmaq_laneq_f32(C2, A3, B2, 3);

                float32x4_t B3 = vld1q_f32(B + b_idx + 3 * k);
                C3 = vfmaq_laneq_f32(C3, A0, B3, 0);
                C3 = vfmaq_laneq_f32(C3, A1, B3, 1);
                C3 = vfmaq_laneq_f32(C3, A2, B3, 2);
                C3 = vfmaq_laneq_f32(C3, A3, B3, 3);
            }
            vst1q_f32(C + n * j + i, C0);
            vst1q_f32(C + n * j + i + n, C1);
            vst1q_f32(C + n * j + i + 2 * n, C2);
            vst1q_f32(C + n * j + i + 3 * n, C3);
        }
    }
}

// SVE 实现：向量长度无关（n 不必是 4 的倍数；m/k 须是 4 的倍数）
void matrix_multiply_sve(const float32_t *A, const float32_t *B, float32_t *C,
                         uint32_t n, uint32_t m, uint32_t k) {
    for (uint32_t i = 0; i < n; i += svcntw()) {
        svbool_t pred = svwhilelt_b32_u32(i, n);
        for (uint32_t j = 0; j < m; j += 4) {
            svfloat32_t C0 = svdup_n_f32(0), C1 = svdup_n_f32(0);
            svfloat32_t C2 = svdup_n_f32(0), C3 = svdup_n_f32(0);
            for (uint32_t kk = 0; kk < k; kk += 4) {
                uint32_t a_idx = i + n * kk;
                uint32_t b_idx = k * j + kk;
                svfloat32_t A0 = svld1_f32(pred, A + a_idx);
                svfloat32_t A1 = svld1_f32(pred, A + a_idx + n);
                svfloat32_t A2 = svld1_f32(pred, A + a_idx + 2 * n);
                svfloat32_t A3 = svld1_f32(pred, A + a_idx + 3 * n);

                svfloat32_t B0 = svld1rq_f32(svptrue_b32(), B + b_idx);
                C0 = svmla_lane_f32(C0, A0, B0, 0);
                C0 = svmla_lane_f32(C0, A1, B0, 1);
                C0 = svmla_lane_f32(C0, A2, B0, 2);
                C0 = svmla_lane_f32(C0, A3, B0, 3);

                svfloat32_t B1 = svld1rq_f32(svptrue_b32(), B + b_idx + k);
                C1 = svmla_lane_f32(C1, A0, B1, 0);
                C1 = svmla_lane_f32(C1, A1, B1, 1);
                C1 = svmla_lane_f32(C1, A2, B1, 2);
                C1 = svmla_lane_f32(C1, A3, B1, 3);

                svfloat32_t B2 = svld1rq_f32(svptrue_b32(), B + b_idx + 2 * k);
                C2 = svmla_lane_f32(C2, A0, B2, 0);
                C2 = svmla_lane_f32(C2, A1, B2, 1);
                C2 = svmla_lane_f32(C2, A2, B2, 2);
                C2 = svmla_lane_f32(C2, A3, B2, 3);

                svfloat32_t B3 = svld1rq_f32(svptrue_b32(), B + b_idx + 3 * k);
                C3 = svmla_lane_f32(C3, A0, B3, 0);
                C3 = svmla_lane_f32(C3, A1, B3, 1);
                C3 = svmla_lane_f32(C3, A2, B3, 2);
                C3 = svmla_lane_f32(C3, A3, B3, 3);
            }
            svst1_f32(pred, C + n * j + i, C0);
            svst1_f32(pred, C + n * j + i + n, C1);
            svst1_f32(pred, C + n * j + i + 2 * n, C2);
            svst1_f32(pred, C + n * j + i + 3 * n, C3);
        }
    }
}

// 基准上下文
static const float32_t *g_A, *g_B;
static float32_t *g_C;
static uint32_t g_n, g_m, g_k;
static void bench_c(void)    { matrix_multiply_c(g_A, g_B, g_C, g_n, g_m, g_k); }
static void bench_neon(void) { matrix_multiply_neon(g_A, g_B, g_C, g_n, g_m, g_k); }
static void bench_sve(void)  { matrix_multiply_sve(g_A, g_B, g_C, g_n, g_m, g_k); }

int main(void) {
    // ===== 正确性对比（8x8x8，均为 4 的倍数）=====
    enum { N = 8, M = 8, K = 8 };
    float32_t A[N * K], B[K * M], Cc[N * M], Cn[N * M], Cs[N * M];
    for (uint32_t i = 0; i < N * K; i++) A[i] = (float32_t)(i % 4) * 0.5f;
    for (uint32_t i = 0; i < K * M; i++) B[i] = (float32_t)(i % 5) * 0.25f;

    matrix_multiply_c(A, B, Cc, N, M, K);
    matrix_multiply_neon(A, B, Cn, N, M, K);
    matrix_multiply_sve(A, B, Cs, N, M, K);

    int ok = 1;
    for (uint32_t i = 0; i < N * M; i++) {
        if (Cc[i] != Cn[i] || Cc[i] != Cs[i]) ok = 0;
    }
    printf("矩阵乘法: C=%.2f Neon=%.2f SVE=%.2f  %s\n",
           Cc[0], Cn[0], Cs[0], ok ? "(一致 PASS)" : "(不一致 FAIL)");

    // ===== 性能对比（64x64x64）=====
    const uint32_t BN = 64, BM = 64, BK = 64;
    float32_t *bigA = malloc(sizeof(float32_t) * BN * BK);
    float32_t *bigB = malloc(sizeof(float32_t) * BK * BM);
    float32_t *bigC = malloc(sizeof(float32_t) * BN * BM);
    for (uint32_t i = 0; i < BN * BK; i++) bigA[i] = (float32_t)(i % 4) * 0.5f;
    for (uint32_t i = 0; i < BK * BM; i++) bigB[i] = (float32_t)(i % 5) * 0.25f;
    g_A = bigA; g_B = bigB; g_C = bigC; g_n = BN; g_m = BM; g_k = BK;
    const long REPEAT = 10;
    const long MACS = (long)(BN * BM * BK);
    BenchResult results[] = {
        { "matrix_multiply_c",    ns_per_op(bench_c,    REPEAT, MACS) },
        { "matrix_multiply_neon", ns_per_op(bench_neon, REPEAT, MACS) },
        { "matrix_multiply_sve",  ns_per_op(bench_sve,  REPEAT, MACS) },
    };
    print_bench_table("性能对比（每次操作 = 1 次乘加 MAC）", results, 3);
    free(bigA); free(bigB); free(bigC);

    return ok ? 0 : 1;
}
