// 12_fp_vector_load.cpp — 验证 §19 加载与存储（五）：浮点/向量寄存器 + memcpy
//
// 概念：加载/存储也可作用于浮点与向量寄存器：
//   LDR D1, [X0] 加载 64 位到浮点寄存器；STR Q0, [X0,X1] 存 128 位向量。
//   memcpy 这类例程常用宽寄存器减少迭代次数，所以代码里没有浮点值
//   也可能用到浮点寄存器（编译器用 Q 寄存器搬运 16 字节块）。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 12_fp_vector_load.cpp -o 12_fp_vector_load.s

#include <cstring>
#include <cstdint>

// 复制 16 字节：编译器可能用 Q 寄存器（LDR Q0 / STR Q0）单条搬运
__attribute__((noinline))
void copy_16_bytes(const std::uint8_t* src, std::uint8_t* dst) {
    std::memcpy(dst, src, 16);
}

// 复制 32 字节：两条 Q 寄存器搬运（LDP Q0, Q1 / STP）
__attribute__((noinline))
void copy_32_bytes(const std::uint8_t* src, std::uint8_t* dst) {
    std::memcpy(dst, src, 32);
}

#include <cstdio>

int main() {
    std::uint8_t src[32], dst[32];
    for (int i = 0; i < 32; ++i) src[i] = (std::uint8_t)i;
    copy_16_bytes(src, dst);
    copy_32_bytes(src, dst);
    int ok = 1;
    for (int i = 0; i < 32; ++i) ok &= (dst[i] == (std::uint8_t)i);
    std::printf("memcpy_ok=%d\n", ok);
    return ok ? 0 : 1;
}