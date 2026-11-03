// vec_add.c —— 向量加法：观察不同优化级别下自动向量化的效果
//
// 对应文档：《为 Neon 自动向量化编译》第 4 节「向量加法」
// 验证点：同一个 C 循环，在不同 -O 级别下编译，反汇编从标量 FADD 变成向量 FADD：
//   - -O1（无自动向量化）：FADD s0, s0, s1   （标量，一次加一个）
//   - -O2 及以上（自动向量化）：FADD v0.4s, v0.4s, v1.4s（向量，一次加四个）
//
// 用法：本文件用于交叉编译 + 反汇编对照；也提供一个 main 验证结果正确。

#include <stdio.h>

void vec_add(float *vec_A, float *vec_B, float *vec_C, int len_vec) {
    int i;
    for (i = 0; i < len_vec; i++) {
        vec_C[i] = vec_A[i] + vec_B[i];
    }
}

int main(void) {
    float A[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    float B[8] = {8, 7, 6, 5, 4, 3, 2, 1};
    float C[8] = {0};
    vec_add(A, B, C, 8);
    printf("vec_add 结果: ");
    for (int i = 0; i < 8; i++) {
        printf("%.0f ", C[i]);
    }
    printf("\n");
    return 0;
}
