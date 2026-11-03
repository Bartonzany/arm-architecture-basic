// 01_rgb_deinterleave.cpp —— RGB 去交错：朴素 C vs Neon 内建函数
//
// 对应文档：《用 Neon 内建函数优化 C 代码》第 4 节「RGB 去交错」
// 验证点：交错 RGB 数据（R G B 交替）如何拆成三个独立颜色数组。
//   - 朴素 C 版：逐字节存取，-O3 下编译器也不会用 Neon，每个 8 位值
//     塞进一个 64 位通用寄存器（浪费位宽）。
//   - Neon 版：vld3q_u8 生成 LD3，一次交错加载 3×16 字节，三条 vst1q_u8
//     分别存回 R/G/B。
//
// 预期反汇编：C 版全是标量 ldrb/strb；Neon 版出现 ld3/vld3 + st1。

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arm_neon.h>

#include "bench_util.hpp"

// 朴素 C 实现：每个 8 位值单独存取
__attribute__((noinline))
void rgb_deinterleave_c(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *rgb, int len_color) {
    for (int i = 0; i < len_color; i++) {
        r[i] = rgb[3 * i];
        g[i] = rgb[3 * i + 1];
        b[i] = rgb[3 * i + 2];
    }
}

// Neon 实现：LD3 一次交错加载 3×16 字节
__attribute__((noinline))
void rgb_deinterleave_neon(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *rgb, int len_color) {
    int num8x16 = len_color / 16;
    uint8x16x3_t intlv_rgb;
    for (int i = 0; i < num8x16; i++) {
        intlv_rgb = vld3q_u8(rgb + 3 * 16 * i);
        vst1q_u8(r + 16 * i, intlv_rgb.val[0]);
        vst1q_u8(g + 16 * i, intlv_rgb.val[1]);
        vst1q_u8(b + 16 * i, intlv_rgb.val[2]);
    }
}

// 防止编译器把基准循环优化掉
volatile uint8_t g_sink = 0;

int main() {
    // ===== 正确性对比 =====
    const int LEN = 16 * 4;   // 4 批，每批 16 像素
    uint8_t rgb[LEN * 3];
    uint8_t r_c[LEN], g_c[LEN], b_c[LEN];
    uint8_t r_n[LEN], g_n[LEN], b_n[LEN];

    for (int i = 0; i < LEN; i++) {
        rgb[3 * i + 0] = (uint8_t)(i * 3 + 0);
        rgb[3 * i + 1] = (uint8_t)(i * 3 + 1);
        rgb[3 * i + 2] = (uint8_t)(i * 3 + 2);
    }

    rgb_deinterleave_c(r_c, g_c, b_c, rgb, LEN);
    rgb_deinterleave_neon(r_n, g_n, b_n, rgb, LEN);

    int ok = (memcmp(r_c, r_n, LEN) == 0)
          && (memcmp(g_c, g_n, LEN) == 0)
          && (memcmp(b_c, b_n, LEN) == 0);
    printf("RGB deinterleave: C vs Neon => %s\n", ok ? "一致 (PASS)" : "不一致 (FAIL)");

    // ===== 性能对比 =====
    const int NPIX = 1024 * 1024;   // 1M 像素
    uint8_t* big_rgb = new uint8_t[NPIX * 3];
    uint8_t* big_r   = new uint8_t[NPIX];
    uint8_t* big_g   = new uint8_t[NPIX];
    uint8_t* big_b   = new uint8_t[NPIX];
    for (int i = 0; i < NPIX; i++) {
        big_rgb[3 * i + 0] = (uint8_t)(i * 3 + 0);
        big_rgb[3 * i + 1] = (uint8_t)(i * 3 + 1);
        big_rgb[3 * i + 2] = (uint8_t)(i * 3 + 2);
    }

    auto bench_c = [&]() { rgb_deinterleave_c(big_r, big_g, big_b, big_rgb, NPIX); };
    auto bench_n = [&]() { rgb_deinterleave_neon(big_r, big_g, big_b, big_rgb, NPIX); };

    const long REPEAT = 20;
    BenchResult results[] = {
        { "rgb_deinterleave_c",    ns_per_op(bench_c, REPEAT, NPIX) },
        { "rgb_deinterleave_neon", ns_per_op(bench_n, REPEAT, NPIX) },
    };
    print_bench_table("性能对比（每次操作 = 去交错 1 个像素）", results, 2);

    g_sink += big_r[0] + big_g[0] + big_b[0];

    delete[] big_rgb;
    delete[] big_r;
    delete[] big_g;
    delete[] big_b;

    return ok ? 0 : 1;
}
