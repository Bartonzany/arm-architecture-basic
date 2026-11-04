// saxpy_c_only.c —— 仅含 saxpy_c 函数，用于各优化级别反汇编对照
//
// 说明：本文件单独存在，是为了在 scripts/build_all.sh 里用 -O1/-O2/-O3/-Ofast
//   分别编译，观察自动向量化的逐步变化：
//   -O1    标量 fmul + fadd（无向量化）
//   -O2    SVE 向量 fmul + fadd（谓词循环）
//   -O3    同上（更强优化）
//   -Ofast SVE 向量 fmla/fmad（融合乘加，一条指令完成 a*x+y）
//
// 本文件只编译反汇编，不参与链接运行。

#include <arm_sve.h>

void saxpy_c(float *x, float *y, float a, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        y[i] = a * x[i] + y[i];
    }
}
