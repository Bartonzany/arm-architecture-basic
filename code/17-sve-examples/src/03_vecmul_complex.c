// 03_vecmul_complex.c —— 向量乘法（B2）：复数 16 位整数元素
//
// 对应文档：《SVE Programming Examples》B2 章「向量乘法」
// 验证点：复数 16 位整数逐元素乘法，含饱和舍入（Q 格式 15 位小数）。
//   - 朴素 C：逐元素复数乘 + Sat 饱和。
//   - SVE2：svqrdcmlah（SQRDCMLAH，饱和舍入加倍复数乘累加）——文档优化的核心指令。
//
// 说明：复数乘法 (a.re+i*a.im)*(b.re+i*b.im) = (re*re - im*im) + i*(re*im + im*re)。
//   文档用 SQRDCMLAH 旋转 #0/#90 两次实现；这里 C 参考实现与 SVE2 版结果一致。

#include <arm_sve.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bench_util.h"
typedef struct { int16_t re; int16_t im; } cplx_int16_t;

// 饱和到 16 位有符号整数
static inline int16_t Sat(int32_t a) {
    int16_t b = (int16_t)a;
    if (a > 0x7FFF)  b = 0x7FFF;   // MAX_INT16
    if (a < -0x8000) b = -0x8000;  // MIN_INT16
    return b;
}

// 朴素 C 参考实现（Q15 定点：先乘再加 0x4000 舍入，右移 15）
void vecmul_c(int64_t n, cplx_int16_t *a, cplx_int16_t *b, cplx_int16_t *c) {
    for (int64_t i = 0; i < n; i++) {
        c[i].re = Sat((((int32_t)a[i].re * b[i].re + 0x4000) >> 15) -
                      (((int32_t)a[i].im * b[i].im + 0x4000) >> 15));
        c[i].im = Sat((((int32_t)a[i].re * b[i].im + 0x4000) >> 15) +
                      (((int32_t)a[i].im * b[i].re + 0x4000) >> 15));
    }
}

// SVE2 实现：SQRDCMLAH 复数乘累加（旋转 0 和 90 各一次）
void vecmul_sve(int64_t n, cplx_int16_t *a, cplx_int16_t *b, cplx_int16_t *c) {
    // 每个复数是 2 个 int16 元素，故总元素数是 2*n，谓词/循环按 int16 元素计
    int64_t total = 2 * n;
    for (int64_t i = 0; i < total; i += (int64_t)svcnth()) {
        svbool_t pg = svwhilelt_b16(i, total);
        // 加载 a、b 的 16 位元素（复数的实部/虚部交替存放）
        svint16_t va = svld1_s16(pg, (int16_t *)a + i);
        svint16_t vb = svld1_s16(pg, (int16_t *)b + i);
        // 先清零累加器，两次复数乘累加（旋转 0 和 90）
        svint16_t acc = svdup_s16(0);
        acc = svqrdcmlah_s16(acc, va, vb, 0);
        acc = svqrdcmlah_s16(acc, va, vb, 90);
        svst1_s16(pg, (int16_t *)c + i, acc);
    }
}

// 基准上下文与包装函数
static cplx_int16_t *g_a, *g_b, *g_c;
static int64_t g_n;
static void bench_c(void)   { vecmul_c(g_n, g_a, g_b, g_c); }
static void bench_sve(void) { vecmul_sve(g_n, g_a, g_b, g_c); }

int main(void) {
    enum { N = 16 };
    cplx_int16_t a[N], b[N], c_ref[N], c_sve[N];
    for (int i = 0; i < N; i++) {
        a[i].re = (int16_t)(i * 100 - 800);
        a[i].im = (int16_t)(i * 50 - 400);
        b[i].re = (int16_t)(i * 30 - 240);
        b[i].im = (int16_t)(i * 70 - 560);
    }

    vecmul_c(N, a, b, c_ref);
    vecmul_sve(N, a, b, c_sve);

    int ok = 1;
    for (int i = 0; i < N; i++) {
        if (c_ref[i].re != c_sve[i].re || c_ref[i].im != c_sve[i].im) ok = 0;
    }

    printf("复数 16 位向量乘法: 朴素 C vs SVE2 => %s\n", ok ? "一致 (PASS)" : "不一致 (FAIL)");

    // ===== 性能对比 =====
    const int64_t BN = 1 << 16;
    cplx_int16_t *biga = malloc(sizeof(cplx_int16_t) * BN);
    cplx_int16_t *bigb = malloc(sizeof(cplx_int16_t) * BN);
    cplx_int16_t *bigc = malloc(sizeof(cplx_int16_t) * BN);
    for (int64_t i = 0; i < BN; i++) {
        biga[i].re = (int16_t)((i * 100) % 2000 - 1000);
        biga[i].im = (int16_t)((i * 50) % 2000 - 1000);
        bigb[i].re = (int16_t)((i * 30) % 2000 - 1000);
        bigb[i].im = (int16_t)((i * 70) % 2000 - 1000);
    }
    g_a = biga; g_b = bigb; g_c = bigc; g_n = BN;
    const long REPEAT = 20;
    BenchResult results[] = {
        { "vecmul_c",   ns_per_op(bench_c,   REPEAT, BN) },
        { "vecmul_sve", ns_per_op(bench_sve, REPEAT, BN) },
    };
    print_bench_table("性能对比（每次操作 = 1 个复数元素乘）", results, 2);
    free(biga); free(bigb); free(bigc);

    return ok ? 0 : 1;
}
