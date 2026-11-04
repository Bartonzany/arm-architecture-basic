// saxpy_example.c —— SAXPY（y = a*x + y）四种实现 + 性能对比
//
// 对应文档：《SVE 优化指南》第 3~7 章
// 验证点（对齐文档表 6 的四路对比）：
//   - saxpy_c_O1 ：朴素 C，用 __attribute__((optimize("O1"))) 强制 -O1（无向量化，基准）
//   - saxpy_c    ：朴素 C，全局 -O3 编译（自动向量化）
//   - saxpy_sve  ：SVE 内建函数（svlen / svwhilelt / svld1 / svmla / svst1）
//   - saxpy_asm  ：SVE 汇编（whilelo / ld1w / fmla / st1w，见 saxpy_asm.S）
//
// 文档结论（真机 perf）：自动向量化 +27.5%、内建函数 +33.2%、汇编 +34.7%。
// 本 demo 用 clock_gettime 实测，趋势应与之一致（仿真环境下数值不同）。

#include <arm_sve.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

// 朴素 C，强制 -O1（无自动向量化）——基准
__attribute__((optimize("O1")))
void saxpy_c_O1(float *x, float *y, float a, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        y[i] = a * x[i] + y[i];
    }
}

// 朴素 C，随全局 -O3 编译（自动向量化）
void saxpy_c(float *x, float *y, float a, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        y[i] = a * x[i] + y[i];
    }
}

// SVE 内建函数实现
void saxpy_sve(float *x, float *y, float a, uint32_t n) {
    uint32_t i;
    svbool_t predicate;
    svfloat32_t xseg;
    svfloat32_t yseg;

    uint64_t numVals = svlen_f32(xseg);   // 运行时向量长度（CNTW）

    for (i = 0; i < n; i += numVals) {
        predicate = svwhilelt_b32_s32(i, n);      // 谓词防越界（WHILELO）
        xseg = svld1_f32(predicate, x + i);       // 谓词加载 x（LD1W）
        yseg = svld1_f32(predicate, y + i);       // 谓词加载 y（LD1W）
        yseg = svmla_n_f32_m(predicate, yseg, xseg, a);  // y + a*x（MLA）
        svst1_f32(predicate, y + i, yseg);        // 谓词存储（ST1W）
    }
}

// 汇编内核（见 saxpy_asm.S）
extern void saxpy_asm(float *x, float *y, float a, uint32_t n);

// 计时辅助：返回每次操作的纳秒数
static double time_ns_per_elem(void (*fn)(float *, float *, float, uint32_t),
                               float *x, float *y, float a, uint32_t n, int repeat) {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int r = 0; r < repeat; r++) {
        fn(x, y, a, n);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ns = (double)(t1.tv_sec - t0.tv_sec) * 1e9 + (double)(t1.tv_nsec - t0.tv_nsec);
    return ns / (double)(repeat * n);
}

void vecset(float *vec, float val, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) vec[i] = val;
}

int main(void) {
    // ===== 正确性对比 =====
    enum { N = 16 };
    float x0[N], y0[N], x1[N], y1[N], x2[N], y2[N], x3[N], y3[N];
    const float a = 2.0f;
    vecset(x0, 1.5f, N); vecset(y0, 2.0f, N);
    vecset(x1, 1.5f, N); vecset(y1, 2.0f, N);
    vecset(x2, 1.5f, N); vecset(y2, 2.0f, N);
    vecset(x3, 1.5f, N); vecset(y3, 2.0f, N);

    saxpy_c_O1(x0, y0, a, N);
    saxpy_c(x1, y1, a, N);
    saxpy_sve(x2, y2, a, N);
    saxpy_asm(x3, y3, a, N);

    int ok = 1;
    for (uint32_t i = 0; i < N; i++) {
        if (y0[i] != y1[i] || y0[i] != y2[i] || y0[i] != y3[i]) ok = 0;
    }
    printf("正确性: 四种实现 y[0] = %.1f/%.1f/%.1f/%.1f  %s\n",
           y0[0], y1[0], y2[0], y3[0], ok ? "(一致 PASS)" : "(不一致 FAIL)");

    // ===== 性能对比（对齐文档表 6）=====
    const uint32_t BN = 1u << 20;   // 1M 元素
    float *bx = malloc(sizeof(float) * BN);
    float *by = malloc(sizeof(float) * BN);
    vecset(bx, 1.5f, BN);
    vecset(by, 2.0f, BN);

    const int REPEAT = 20;
    double t_O1  = time_ns_per_elem(saxpy_c_O1, bx, by, a, BN, REPEAT);
    double t_O3  = time_ns_per_elem(saxpy_c,    bx, by, a, BN, REPEAT);
    double t_sve = time_ns_per_elem(saxpy_sve,  bx, by, a, BN, REPEAT);
    double t_asm = time_ns_per_elem(saxpy_asm,  bx, by, a, BN, REPEAT);

    printf("\n方法                      每次操作耗时      加速比\n");
    printf("---------------------------------------------------------\n");
    printf("C 无向量化 (-O1)          %8.3f ns       1x\n",     t_O1);
    printf("C 自动向量化 (-O3)        %8.3f ns       %.3fx\n",  t_O3,  t_O1 / t_O3);
    printf("SVE 内建函数              %8.3f ns       %.3fx\n",  t_sve, t_O1 / t_sve);
    printf("SVE 汇编                  %8.3f ns       %.3fx\n",  t_asm, t_O1 / t_asm);

    free(bx);
    free(by);
    return ok ? 0 : 1;
}
