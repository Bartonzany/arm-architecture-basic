// cubed.c —— 循环中的函数：内联 + restrict 触发自动向量化
//
// 对应文档：《为 Neon 自动向量化编译》第 5 节「循环中的函数」
// 验证点：函数调用挡在循环里时，编译器无法向量化；通过
//   __attribute__((always_inline)) 强制内联 + restrict 消除重叠检查，
//   编译器就能用 ASIMD 向量指令（FMUL v0.2D）实现数组立方。
//
//   - vec_cubed     （朴素）：cubed 未内联，标量 FMUL d0 逐个计算
//   - vec_cubed_opt （优化）：cubed_i 内联 + restrict，FMUL v0.2D 向量化
//
// 说明：文档用 armclang 的 `#pragma clang loop interleave_count(1)`，本文件
//   改用 GCC 的等价位 `#pragma GCC ivdep`（GCC 不支持 clang loop pragma）。

#include <stdio.h>

void vec_init(double *vec, int len_vec, double init_val) {
    int i;
    for (i = 0; i < len_vec; i++) {
        vec[i] = init_val * i - len_vec / 2;
    }
}

void vec_print(double *vec, int len_vec) {
    int i;
    for (i = 0; i < len_vec; i++) {
        printf("%f, ", vec[i]);
    }
    printf("\n");
}

// 朴素版：cubed 是独立函数，-O1 下不会被内联 → 无法向量化
double cubed(double x) {
    return x * x * x;
}

void vec_cubed(double *x_vec, double *y_vec, int len_vec) {
    int i;
    for (i = 0; i < len_vec; i++) {
        y_vec[i] = cubed(x_vec[i]);
    }
}

// 优化版：always_inline 强制内联 + restrict 声明不重叠 → 可向量化
static inline __attribute__((always_inline)) double cubed_i(double x) {
    return x * x * x;
}

void vec_cubed_opt(double *restrict x_vec, double *restrict y_vec, int len_vec) {
    int i;
#pragma GCC ivdep
    for (i = 0; i < len_vec; i++) {
        y_vec[i] = cubed_i(x_vec[i]);
    }
}

int main(void) {
    int N = 10;
    double X[N];
    double Y[N];

    vec_init(X, N, 1.0);
    printf("输入:   ");
    vec_print(X, N);

    vec_cubed(X, Y, N);
    printf("朴素:   ");
    vec_print(Y, N);

    vec_cubed_opt(X, Y, N);
    printf("优化:   ");
    vec_print(Y, N);

    return 0;
}
