// 04_floating_point.cpp — 验证 §10 数据处理（二）：浮点运算
//
// 概念：浮点指令与整数数据处理格式相同，改用浮点寄存器（B/H/S/D/Q）。
//   操作名以 F 开头（FADD/FSUB/FDIV）。操作宽度由寄存器名决定：
//   S=32 位单精度，D=64 位双精度，H=16 位半精度（Armv8.2-A FEAT_FP16）。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 04_floating_point.cpp -o 04_floating_point.s

#include <cstdint>

// 单精度加法：FADD S0, S0, S1
__attribute__((noinline))
float fadd32(float a, float b) { return a + b; }

// 双精度减法：FSUB D0, D0, D1
__attribute__((noinline))
double fsub64(double a, double b) { return a - b; }

// 单精度除法：FDIV S0, S0, S1
__attribute__((noinline))
float fdiv32(float a, float b) { return a / b; }

// 乘加融合 FMA：a*b+c 编译为 FMADD（单条指令）
__attribute__((noinline))
float fmadd32(float a, float b, float c) { return a * b + c; }

#include <cstdio>

int main() {
    float  r1 = fadd32(1.5f, 2.5f);
    double r2 = fsub64(10.0, 3.0);
    float  r3 = fdiv32(1.0f, 4.0f);
    float  r4 = fmadd32(2.0f, 3.0f, 4.0f);
    std::printf("add=%f sub=%f div=%f fma=%f\n",
                (double)r1, r2, (double)r3, (double)r4);
    return (r1 == 4.0f && r2 == 7.0 && r3 == 0.25f && r4 == 10.0f) ? 0 : 1;
}
