// 02_matrix_multiply.cpp —— 矩阵乘法：朴素 C vs Neon（4x4 分块 + 融合乘加）
//
// 对应文档：《用 Neon 内建函数优化 C 代码》第 5 节「矩阵乘法」
// 验证点：
//   - 朴素 C 三重循环：C[n*j+i] += A[n*k+i] * B[k*j+k]，标量 FMUL/FADD。
//   - Neon 版：4x4 分块，float32x4_t 每列各占一个 128 位寄存器，
//     vfmaq_laneq_f32 生成 FMLA（融合乘加），把"乘 + 加"合成一条。
//   - 编译结果：算术指令更少、FMLA 取代 FMUL、循环迭代更少。
//
// 说明：原文中矩阵乘法的融合乘加内建函数统一写作 vfmaq_laneq_f32（从
//   float32x4_t 的某个 lane 取标量）；第 5 节小结表里的 vfmaq_lane_f32
//   系笔误，本文件已按架构规范订正为 vfmaq_laneq_f32。

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <arm_neon.h>

#include "bench_util.hpp"

#define BLOCK_SIZE 4

// 朴素 C 实现（列主序）
__attribute__((noinline))
void matrix_multiply_c(float32_t *A, float32_t *B, float32_t *C, uint32_t n, uint32_t m, uint32_t k) {
    for (int i_idx = 0; i_idx < n; i_idx++) {
        for (int j_idx = 0; j_idx < m; j_idx++) {
            C[n * j_idx + i_idx] = 0;
            for (int k_idx = 0; k_idx < k; k_idx++) {
                C[n * j_idx + i_idx] += A[n * k_idx + i_idx] * B[k * j_idx + k_idx];
            }
        }
    }
}

// Neon 通用实现：把大矩阵当 4x4 块处理
__attribute__((noinline))
void matrix_multiply_neon(float32_t *A, float32_t *B, float32_t *C, uint32_t n, uint32_t m, uint32_t k) {
    int A_idx;
    int B_idx;
    int C_idx;

    // 这些是 A 的 4x4 子矩阵的列
    float32x4_t A0, A1, A2, A3;
    // 这些是 B 的 4x4 子矩阵的列
    float32x4_t B0, B1, B2, B3;
    // 这些是 C 的 4x4 子矩阵的列
    float32x4_t C0, C1, C2, C3;

    for (int i_idx = 0; i_idx < n; i_idx += 4) {
        for (int j_idx = 0; j_idx < m; j_idx += 4) {
            // 矩阵运算前清零累加器
            C0 = vmovq_n_f32(0);
            C1 = vmovq_n_f32(0);
            C2 = vmovq_n_f32(0);
            C3 = vmovq_n_f32(0);
            for (int k_idx = 0; k_idx < k; k_idx += 4) {
                // 计算 4x4 块的基索引
                A_idx = i_idx + n * k_idx;
                B_idx = k * j_idx + k_idx;

                // 加载行中最新的 A 值
                A0 = vld1q_f32(A + A_idx);
                A1 = vld1q_f32(A + A_idx + n);
                A2 = vld1q_f32(A + A_idx + 2 * n);
                A3 = vld1q_f32(A + A_idx + 3 * n);

                // 按 4x1 块做乘加，即 C 的每一列
                B0 = vld1q_f32(B + B_idx);
                C0 = vfmaq_laneq_f32(C0, A0, B0, 0);
                C0 = vfmaq_laneq_f32(C0, A1, B0, 1);
                C0 = vfmaq_laneq_f32(C0, A2, B0, 2);
                C0 = vfmaq_laneq_f32(C0, A3, B0, 3);

                B1 = vld1q_f32(B + B_idx + k);
                C1 = vfmaq_laneq_f32(C1, A0, B1, 0);
                C1 = vfmaq_laneq_f32(C1, A1, B1, 1);
                C1 = vfmaq_laneq_f32(C1, A2, B1, 2);
                C1 = vfmaq_laneq_f32(C1, A3, B1, 3);

                B2 = vld1q_f32(B + B_idx + 2 * k);
                C2 = vfmaq_laneq_f32(C2, A0, B2, 0);
                C2 = vfmaq_laneq_f32(C2, A1, B2, 1);
                C2 = vfmaq_laneq_f32(C2, A2, B2, 2);
                C2 = vfmaq_laneq_f32(C2, A3, B2, 3);

                B3 = vld1q_f32(B + B_idx + 3 * k);
                C3 = vfmaq_laneq_f32(C3, A0, B3, 0);
                C3 = vfmaq_laneq_f32(C3, A1, B3, 1);
                C3 = vfmaq_laneq_f32(C3, A2, B3, 2);
                C3 = vfmaq_laneq_f32(C3, A3, B3, 3);
            }
            // 计算存储的基索引
            C_idx = n * j_idx + i_idx;
            vst1q_f32(C + C_idx, C0);
            vst1q_f32(C + C_idx + n, C1);
            vst1q_f32(C + C_idx + 2 * n, C2);
            vst1q_f32(C + C_idx + 3 * n, C3);
        }
    }
}

// Neon 固定 4x4 实现（循环完全展开）
__attribute__((noinline))
void matrix_multiply_4x4_neon(float32_t *A, float32_t *B, float32_t *C) {
    float32x4_t A0, A1, A2, A3;
    float32x4_t B0, B1, B2, B3;
    float32x4_t C0, C1, C2, C3;

    A0 = vld1q_f32(A);
    A1 = vld1q_f32(A + 4);
    A2 = vld1q_f32(A + 8);
    A3 = vld1q_f32(A + 12);

    C0 = vmovq_n_f32(0);
    C1 = vmovq_n_f32(0);
    C2 = vmovq_n_f32(0);
    C3 = vmovq_n_f32(0);

    B0 = vld1q_f32(B);
    C0 = vfmaq_laneq_f32(C0, A0, B0, 0);
    C0 = vfmaq_laneq_f32(C0, A1, B0, 1);
    C0 = vfmaq_laneq_f32(C0, A2, B0, 2);
    C0 = vfmaq_laneq_f32(C0, A3, B0, 3);
    vst1q_f32(C, C0);

    B1 = vld1q_f32(B + 4);
    C1 = vfmaq_laneq_f32(C1, A0, B1, 0);
    C1 = vfmaq_laneq_f32(C1, A1, B1, 1);
    C1 = vfmaq_laneq_f32(C1, A2, B1, 2);
    C1 = vfmaq_laneq_f32(C1, A3, B1, 3);
    vst1q_f32(C + 4, C1);

    B2 = vld1q_f32(B + 8);
    C2 = vfmaq_laneq_f32(C2, A0, B2, 0);
    C2 = vfmaq_laneq_f32(C2, A1, B2, 1);
    C2 = vfmaq_laneq_f32(C2, A2, B2, 2);
    C2 = vfmaq_laneq_f32(C2, A3, B2, 3);
    vst1q_f32(C + 8, C2);

    B3 = vld1q_f32(B + 12);
    C3 = vfmaq_laneq_f32(C3, A0, B3, 0);
    C3 = vfmaq_laneq_f32(C3, A1, B3, 1);
    C3 = vfmaq_laneq_f32(C3, A2, B3, 2);
    C3 = vfmaq_laneq_f32(C3, A3, B3, 3);
    vst1q_f32(C + 12, C3);
}

void print_matrix(float32_t *M, uint32_t cols, uint32_t rows) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%f ", M[j * rows + i]);
        }
        printf("\n");
    }
    printf("\n");
}

void matrix_init_rand(float32_t *M, uint32_t numvals) {
    for (int i = 0; i < numvals; i++) {
        M[i] = (float)rand() / (float)(RAND_MAX);
    }
}

void matrix_init(float32_t *M, uint32_t cols, uint32_t rows, float32_t val) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            M[j * rows + i] = val;
        }
    }
}

bool f32comp_noteq(float32_t a, float32_t b) {
    return fabs(a - b) >= 0.000001f;
}

bool matrix_comp(float32_t *A, float32_t *B, uint32_t rows, uint32_t cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (f32comp_noteq(A[rows * j + i], B[rows * j + i])) {
                printf("i=%d, j=%d, A=%f, B=%f\n", i, j, A[rows * j + i], B[rows * j + i]);
                return false;
            }
        }
    }
    return true;
}

int main() {
    uint32_t n = 2 * BLOCK_SIZE;   // A 中的行数
    uint32_t m = 2 * BLOCK_SIZE;   // B 中的列数
    uint32_t k = 2 * BLOCK_SIZE;   // A 中的列数 = B 中的行数

    float32_t A[n * k];
    float32_t B[k * m];
    float32_t C[n * m];
    float32_t D[n * m];
    float32_t E[n * m];

    matrix_init_rand(A, n * k);
    matrix_init_rand(B, k * m);
    matrix_init(C, n, m, 0);

    matrix_multiply_c(A, B, E, n, m, k);        // 朴素 C
    matrix_multiply_neon(A, B, D, n, m, k);     // Neon 通用
    bool c_eq_neon = matrix_comp(E, D, n, m);

    matrix_multiply_4x4_neon(A, B, C);          // Neon 固定 4x4（仅当 n=m=k=4 时才有意义，
                                                 // 这里 n=m=k=8，仅演示该函数能编译运行）
    printf("matrix_multiply_c vs matrix_multiply_neon => %s\n",
           c_eq_neon ? "一致 (PASS)" : "不一致 (FAIL)");

    // ===== 性能对比 =====
    // 用较大的 64x64x64 矩阵计时，统计"每次乘加（MAC）"的纳秒数。
    const uint32_t BN = 64, BM = 64, BK = 64;
    float32_t* bigA = new float32_t[BN * BK];
    float32_t* bigB = new float32_t[BK * BM];
    float32_t* bigC = new float32_t[BN * BM];
    float32_t* bigD = new float32_t[BN * BM];
    matrix_init_rand(bigA, BN * BK);
    matrix_init_rand(bigB, BK * BM);

    auto bench_c = [&]() { matrix_multiply_c(bigA, bigB, bigC, BN, BM, BK); };
    auto bench_n = [&]() { matrix_multiply_neon(bigA, bigB, bigD, BN, BM, BK); };

    const long REPEAT = 10;
    const long MACS   = (long)BN * BM * BK;   // 一次乘法的乘加总数
    BenchResult results[] = {
        { "matrix_multiply_c",    ns_per_op(bench_c, REPEAT, MACS) },
        { "matrix_multiply_neon", ns_per_op(bench_n, REPEAT, MACS) },
    };
    print_bench_table("性能对比（每次操作 = 1 次乘加 MAC）", results, 2);

    delete[] bigA;
    delete[] bigB;
    delete[] bigC;
    delete[] bigD;

    return c_eq_neon ? 0 : 1;
}
