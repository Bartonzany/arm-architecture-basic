// 04_vecdot_complex.c —— 向量点积（B3.2）：复数单精度浮点
//
// 对应文档：《SVE Programming Examples》B3.2 章「复数单精度浮点元素向量的点积」
// 验证点：复数点积 c = Σ (a.re*b.re - a.im*b.im) + i*(a.re*b.im + a.im*b.re)。
//   - 朴素 C：逐元素复数乘累加。
//   - SVE2：FCMLA（svcmla）复数乘累加，旋转 0/90 各一次得到完整复数乘法，
//           再去交错（svuzp1/svuzp2）+ 归约（svaddv）分离实部/虚部。
//
// 说明：复数数据在向量里按 [re,im,re,im,...] 交替存放。文档 B3.4 还给出复数
//   16 位整数点积（用 CDOT），语义类似，此处不重复。

#include <arm_sve.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bench_util.h"

typedef struct { float re; float im; } cplx_f32_t;

void vecdot_cplx_f32_c(int64_t n, cplx_f32_t *a, cplx_f32_t *b, cplx_f32_t *c) {
    cplx_f32_t acc;
    acc.re = 0;
    acc.im = 0;
    for (int64_t i = 0; i < n; ++i) {
        acc.re += a[i].re * b[i].re - a[i].im * b[i].im;
        acc.im += a[i].re * b[i].im + a[i].im * b[i].re;
    }
    c->re = acc.re;
    c->im = acc.im;
}

void vecdot_cplx_f32_sve(int64_t n, cplx_f32_t *a, cplx_f32_t *b, cplx_f32_t *c) {
    // 每个复数是 2 个 float 元素，总元素数是 2*n，谓词/循环按 float 元素计
    int64_t total = 2 * n;
    svfloat32_t acc = svdup_f32(0);
    for (int64_t i = 0; i < total; i += (int64_t)svcntw()) {
        svbool_t pg = svwhilelt_b32(i, total);
        svfloat32_t va = svld1_f32(pg, (float *)a + i);
        svfloat32_t vb = svld1_f32(pg, (float *)b + i);
        // 复数乘累加：旋转 0 和 90 各一次得到完整复数乘法
        acc = svcmla_f32_m(pg, acc, va, vb, 0);
        acc = svcmla_f32_m(pg, acc, va, vb, 90);
    }
    // 实部/虚部交替存放，按偶/奇通道去交错（配 0 向量）后分别归约
    svfloat32_t zero = svdup_f32(0);
    svfloat32_t re_vec = svuzp1_f32(acc, zero);   // 偶通道（实部）+ 0
    svfloat32_t im_vec = svuzp2_f32(acc, zero);   // 奇通道（虚部）+ 0
    c->re = svaddv_f32(svptrue_b32(), re_vec);
    c->im = svaddv_f32(svptrue_b32(), im_vec);
}

// 基准上下文与包装函数
static cplx_f32_t *g_a, *g_b, *g_c;
static int64_t g_n;
static void bench_c(void)   { vecdot_cplx_f32_c(g_n, g_a, g_b, g_c); }
static void bench_sve(void) { vecdot_cplx_f32_sve(g_n, g_a, g_b, g_c); }

int main(void) {
    enum { N = 16 };
    cplx_f32_t a[N], b[N], c_ref, c_sve;
    for (int i = 0; i < N; i++) {
        a[i].re = (float)(i + 1) * 0.5f;  a[i].im = (float)(i + 2) * 0.25f;
        b[i].re = (float)(i + 3) * 0.5f;  b[i].im = (float)(i + 4) * 0.25f;
    }

    vecdot_cplx_f32_c(N, a, b, &c_ref);
    vecdot_cplx_f32_sve(N, a, b, &c_sve);

    int ok = (c_ref.re == c_sve.re) && (c_ref.im == c_sve.im);
    printf("复数 SP 点积: 朴素 C re=%.2f im=%.2f, SVE2 re=%.2f im=%.2f  %s\n",
           c_ref.re, c_ref.im, c_sve.re, c_sve.im, ok ? "(一致 PASS)" : "(不一致 FAIL)");

    // ===== 性能对比 =====
    const int64_t BN = 1 << 16;
    cplx_f32_t *biga = malloc(sizeof(cplx_f32_t) * BN);
    cplx_f32_t *bigb = malloc(sizeof(cplx_f32_t) * BN);
    cplx_f32_t bigc;
    for (int64_t i = 0; i < BN; i++) {
        biga[i].re = (float)(i % 7) * 0.5f;  biga[i].im = (float)(i % 11) * 0.25f;
        bigb[i].re = (float)(i % 13) * 0.5f; bigb[i].im = (float)(i % 17) * 0.25f;
    }
    g_a = biga; g_b = bigb; g_c = &bigc; g_n = BN;
    const long REPEAT = 50;
    BenchResult results[] = {
        { "vecdot_cplx_f32_c",   ns_per_op(bench_c,   REPEAT, BN) },
        { "vecdot_cplx_f32_sve", ns_per_op(bench_sve, REPEAT, BN) },
    };
    print_bench_table("性能对比（每次操作 = 1 个复数元素乘累加）", results, 2);
    free(biga); free(bigb);

    return ok ? 0 : 1;
}
