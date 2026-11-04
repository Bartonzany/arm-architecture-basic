// 01_vecmax.c —— 向量最大值（B1）：朴素 C vs SVE2 归约
//
// 对应文档：《SVE Programming Examples》B1 章「向量最大值」
// 验证点：SVE2 的"向量长度无关"循环 + 跨通道归约。
//   - 朴素 C：逐元素比较，标量循环。
//   - SVE2：svwhilelt 生成谓词 + svld1 谓词加载 + svmax 逐通道取最大，
//           最后 svmaxv 做跨通道有符号最大值归约。
// 文档原文还演示了"最大值首次出现位置"（用 SEL + UMINV 归约最小索引），
// 本 demo 只演示最大值本身，位置查找见文档 B1 章汇编清单。

#include <arm_sve.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

#include "bench_util.h"

// 朴素 C 参考实现
int16_t vecmax(const int16_t *src, int n) {
    int16_t m = src[0];
    for (int i = 1; i < n; i++) {
        if (src[i] > m) {
            m = src[i];
        }
    }
    return m;
}

// SVE2 向量长度无关实现
int16_t vecmax_sve(const int16_t *src, int n) {
    svint16_t m = svdup_s16(INT16_MIN);
    for (int i = 0; i < n; i += (int)svcntw()) {
        svbool_t pg = svwhilelt_b16(i, n);       // 谓词：i < n 的通道为真
        svint16_t v = svld1_s16(pg, src + i);    // 谓词加载
        m = svmax_s16_x(pg, m, v);               // 逐通道取最大
    }
    return svmaxv_s16(svptrue_b16(), m);         // 跨通道有符号最大值归约
}

// 基准上下文与包装函数
static const int16_t *g_src;
static int g_n;
static volatile int16_t g_sink;
static void bench_c(void)   { g_sink = vecmax(g_src, g_n); }
static void bench_sve(void) { g_sink = vecmax_sve(g_src, g_n); }

int main(void) {
    int16_t src[32];
    for (int i = 0; i < 32; i++) {
        src[i] = (int16_t)((i * 37) % 1000 - 500);
    }
    src[7]  = 999;
    src[23] = -999;

    int16_t r_c   = vecmax(src, 32);
    int16_t r_sve = vecmax_sve(src, 32);

    printf("向量最大值: 朴素 C = %d, SVE2 = %d  %s\n",
           r_c, r_sve, (r_c == r_sve) ? "(一致 PASS)" : "(不一致 FAIL)");

    // ===== 性能对比 =====
    const int BN = 1 << 20;
    int16_t *big = malloc(sizeof(int16_t) * BN);
    for (int i = 0; i < BN; i++) big[i] = (int16_t)((i * 37) % 1000 - 500);
    g_src = big;
    g_n = BN;
    const long REPEAT = 50;
    BenchResult results[] = {
        { "vecmax",     ns_per_op(bench_c,   REPEAT, BN) },
        { "vecmax_sve", ns_per_op(bench_sve, REPEAT, BN) },
    };
    print_bench_table("性能对比（每次操作 = 1 个元素比较）", results, 2);
    free(big);

    return (r_c == r_sve) ? 0 : 1;
}
