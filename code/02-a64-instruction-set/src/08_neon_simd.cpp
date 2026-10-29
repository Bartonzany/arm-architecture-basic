// 08_neon_simd.cpp — 验证 §14 向量与矩阵：Advanced SIMD (NEON)
//
// 概念：A64 支持向量数据处理。高级 SIMD（NEON）用 128 位寄存器
//   （V0-V31/Q0-Q31），一次并行处理多个独立元素。
//   V 形式把寄存器视为"装着多个独立值的向量"。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 08_neon_simd.cpp -o 08_neon_simd.s

#include <arm_neon.h>
#include <cstdint>

// 128 位 int8 向量加法：16 个元素并行相加（ADD V0.16B, V0.16B, V1.16B）
__attribute__((noinline))
void vec_add_int8(const std::int8_t* a, const std::int8_t* b, std::int8_t* out) {
    int8x16_t va = vld1q_s8(a);
    int8x16_t vb = vld1q_s8(b);
    vst1q_s8(out, vaddq_s8(va, vb));   // 一条 NEON 指令加 16 个 int8
}

// 向量浮点加法：4 个 float 并行相加（FADD V0.4S, ...）
__attribute__((noinline))
void vec_add_f32(const float* a, const float* b, float* out) {
    float32x4_t va = vld1q_f32(a);
    float32x4_t vb = vld1q_f32(b);
    vst1q_f32(out, vaddq_f32(va, vb));
}

#include <cstdio>

int main() {
    std::int8_t a[16], b[16], o[16];
    for (int i = 0; i < 16; ++i) { a[i] = (std::int8_t)i; b[i] = 1; }
    vec_add_int8(a, b, o);

    float fa[4] = {1, 2, 3, 4}, fb[4] = {10, 20, 30, 40}, fo[4];
    vec_add_f32(fa, fb, fo);

    std::printf("int8[0]=%d int8[15]=%d f32[0]=%f\n", (int)o[0], (int)o[15], (double)fo[0]);
    return (o[0] == 1 && o[15] == 16 && fo[0] == 11.0f) ? 0 : 1;
}