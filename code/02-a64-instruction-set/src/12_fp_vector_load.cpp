// 12_fp_vector_load.cpp — 验证 §19 加载与存储（五）：浮点/向量寄存器
//
// 概念：加载/存储也可作用于浮点与向量寄存器（文档 §19 的三个示例）：
//   LDR D1, [X0]          把 64 位加载到浮点寄存器 D1
//   STR Q0, [X0, X1]      把 128 位向量存到 [X0 + X1]（寄存器偏移）
//   LDP Q1, Q3, [X5], #256 加载一对 128 位，并把 X5 后移 256
//   此外 memcpy 常用宽向量寄存器（Q）减少迭代次数，代码里没有浮点值
//   也可能用到浮点寄存器。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 12_fp_vector_load.cpp -o 12_fp_vector_load.s

#include <cstdint>
#include <cstring>
#include <arm_neon.h>
#include "test_util.hpp"

// ============================================================================
// 被测函数
// ============================================================================

// 1. LDR D1, [X0]：64 位加载到浮点寄存器 D
__attribute__((noinline))
double load_d(const double* p) {
    return *p;                          // LDR D0, [X0]
}

// 2. STR Q0, [X0, X1]：128 位向量存到 [base + i]（基址 + 寄存器偏移）
__attribute__((noinline))
void store_q_at(std::uint8_t* base, std::size_t i, std::uint8_t v0) {
    uint8x16_t v = vdupq_n_u8(v0);      // 把单字节复制成 16 字节向量
    vst1q_u8(base + i, v);              // STR Q0, [X0, X1]
}

// 3. LDP Q1, Q3, [X5], #256：加载一对 128 位，指针后移 256
//    （GCC 不自然生成 ldp q + 后索引 + 大偏移，故用内联汇编精确展示）
__attribute__((noinline))
void ldp_pair_q_256(const std::uint8_t* src, std::uint8_t* out1, std::uint8_t* out2) {
    std::uint8_t* p = const_cast<std::uint8_t*>(src);
    __asm__ volatile(
        "ldp q1, q3, [%0], #256\n\t"    // 加载 q1/q3，p += 256
        "str q1, [%1]\n\t"
        "str q3, [%2]\n\t"
        : "+r"(p)
        : "r"(out1), "r"(out2)
        : "q1", "q3", "memory");
}

// （补充）memcpy 用宽向量寄存器：32 字节 = 2 × Q（LDP Q0, Q1 / STP Q0, Q1）
__attribute__((noinline))
void copy_32_bytes(const std::uint8_t* src, std::uint8_t* dst) {
    std::memcpy(dst, src, 32);
}

// ============================================================================
// 测试
// ============================================================================

static void test_fp_vector_load() {
    group("§19 浮点/向量寄存器加载存储");

    // 1. LDR D1, [X0]
    double d = 3.14;
    CHECK_EQ(load_d(&d), 3.14);

    // 2. STR Q0, [X0, X1]：写入 base[3..18] = 0xAB
    std::uint8_t buf[32] = {0};
    store_q_at(buf, 3, 0xABu);
    CHECK_EQ(buf[3], 0xABu);
    CHECK_EQ(buf[18], 0xABu);

    // 3. LDP Q1, Q3, [X5], #256
    std::uint8_t src[32], o1[16], o2[16];
    for (int i = 0; i < 32; ++i) src[i] = static_cast<std::uint8_t>(i);
    ldp_pair_q_256(src, o1, o2);
    CHECK_EQ(o1[0], 0u);   // src[0]
    CHECK_EQ(o2[0], 16u);  // src[16]

    // （补充）memcpy 32 字节
    std::uint8_t dst[32];
    copy_32_bytes(src, dst);
    bool ok = true;
    for (int i = 0; i < 32; ++i) ok = ok && (dst[i] == static_cast<std::uint8_t>(i));
    CHECK(ok);
}

int main() {
    test_fp_vector_load();
    return report();
}