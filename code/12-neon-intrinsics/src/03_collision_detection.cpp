// 03_collision_detection.cpp —— 碰撞检测：朴素 C / 基本向量化 / 去交错高级向量化
//
// 对应文档：《用 Neon 内建函数优化 C 代码》第 6 节「碰撞检测」
// 验证点（核心结论：光换指令不够，数据布局决定向量化收益）：
//   - does_collide（朴素）：标量 fsub/fmul/fadd，几乎不向量化。
//   - does_collide_neon（基本向量化）：用 float32x2_t（2 通道 64 位）并行
//     x、y 的减法和平方，但 x/y/radius 交错存放，收益被跨通道加法抵消。
//   - does_collide_neon_deinterleaved（去交错）：x/y/radius 各进独立数组，
//     一次处理 4 个圆，float32x4_t 满寄存器打包 → 近 3 倍提速。
//
// 注意：
//   - Neon 版 struct circle 字段顺序为 {x, y, radius}，这样 x、y 在内存中
//     连续，vld1_f32(&c1.x) 才能一次把 x、y 加载进 2 通道寄存器。
//   - 三个函数都标了 noinline，防止编译器在基准测试里把朴素版也自动向量化，
//     从而能如实测出"仅靠内建函数"带来的性能差异（与文档实验方法一致）。

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <arm_neon.h>

#include "bench_util.hpp"

// x、y 在前（连续），radius 在后 —— 供 Neon 的 vld1_f32(&c.x) 一次取 x、y
struct circle {
    float x;
    float y;
    float radius;
} __attribute__((aligned(64)));

// 结构体：指向多个圆（去交错）数据的数组指针
struct circles {
    size_t size;
    float* xs;
    float* ys;
    float* radii;
};

// 朴素实现（不向量化）
__attribute__((noinline))
bool does_collide(circle const& c1, circle const& c2) {
    float delta_x = c1.x - c2.x;
    float delta_y = c1.y - c2.y;
    float deltas_squared = (delta_x * delta_x) + (delta_y * delta_y);
    float radius_sum_squared = (c1.radius * c1.radius) + (c2.radius * c2.radius);
    return deltas_squared <= radius_sum_squared;
}

// 基本向量化（2 通道 64 位，并行 x、y 的减法和平方）
__attribute__((noinline))
bool does_collide_neon(circle const& c1, circle const& c2) {
    float32x2_t c1_coords = vld1_f32(&c1.x);
    float32x2_t c2_coords = vld1_f32(&c2.x);
    float32x2_t deltas = vsub_f32(c1_coords, c2_coords);
    float32x2_t deltas_squared = vmul_f32(deltas, deltas);
    float sum_deltas_squared = vpadds_f32(deltas_squared);
    float radius_sum = c1.radius + c2.radius;
    float radius_sum_squared = radius_sum * radius_sum;
    return sum_deltas_squared <= radius_sum_squared;
}

// 高级向量化（去交错数据布局，一次处理 4 个圆）
__attribute__((noinline))
void does_collide_neon_deinterleaved(circles const& input, circle& collider, bool* out) {
    // 把碰撞圆属性广播到 3 个独立的 4 通道向量寄存器
    float32x4_t c1_x = vdupq_n_f32(collider.x);
    float32x4_t c1_y = vdupq_n_f32(collider.y);
    float32x4_t c1_r = vdupq_n_f32(collider.radius);

    for (size_t offset = 0; offset != input.size; offset += 4) {
        // 一次执行 4 个碰撞测试
        float32x4_t x = vld1q_f32(input.xs + offset);
        float32x4_t y = vld1q_f32(input.ys + offset);
        float32x4_t delta_x = vsubq_f32(c1_x, x);
        float32x4_t delta_y = vsubq_f32(c1_y, y);
        float32x4_t delta_x_squared = vmulq_f32(delta_x, delta_x);
        float32x4_t delta_y_squared = vmulq_f32(delta_y, delta_y);
        float32x4_t sum_deltas_squared = vaddq_f32(delta_x_squared, delta_y_squared);
        float32x4_t r = vld1q_f32(input.radii + offset);
        float32x4_t radius_sum = vaddq_f32(c1_r, r);
        float32x4_t radius_sum_squared = vmulq_f32(radius_sum, radius_sum);
        uint32x4_t mask = vcltq_f32(sum_deltas_squared, radius_sum_squared);
        // 把每个通道的结果解包出来
        out[offset]     = 1 & vgetq_lane_u32(mask, 0);
        out[offset + 1] = 1 & vgetq_lane_u32(mask, 1);
        out[offset + 2] = 1 & vgetq_lane_u32(mask, 2);
        out[offset + 3] = 1 & vgetq_lane_u32(mask, 3);
    }
}

// 防止编译器把循环优化掉
volatile int g_sink = 0;

int main() {
    // ===== 场景一：正确性（两个圆，原文示例，圆心距 5 > 半径和 3，不碰撞）=====
    circle c1; c1.x = 2.0f; c1.y = 4.0f; c1.radius = 2.0f;
    circle c2; c2.x = 6.0f; c2.y = 1.0f; c2.radius = 1.0f;

    bool r_plain = does_collide(c1, c2);
    bool r_neon  = does_collide_neon(c1, c2);
    printf("两个圆(圆心距5,半径和3): 朴素=%s  Neon=%s  %s\n",
           r_plain ? "碰撞" : "不碰撞",
           r_neon  ? "碰撞" : "不碰撞",
           r_plain == r_neon ? "(一致 PASS)" : "(不一致 FAIL)");

    // 去交错版：一次检测碰撞圆与 4 个输入圆
    const int num_input = 4;
    alignas(64) float input_x[num_input];
    alignas(64) float input_y[num_input];
    alignas(64) float input_r[num_input];
    alignas(64) bool  output[num_input];

    for (int i = 0; i < num_input; i++) {
        input_x[i] = (float)(i * 2);
        input_y[i] = (float)(i * 3);
        input_r[i] = (float)i;
        output[i]  = false;
    }

    circles small_input;
    small_input.size  = num_input;
    small_input.xs    = input_x;
    small_input.ys    = input_y;
    small_input.radii = input_r;

    circle collider; collider.x = 10.0f; collider.y = 10.0f; collider.radius = 5.0f;
    does_collide_neon_deinterleaved(small_input, collider, output);

    for (int i = 0; i < num_input; i++) {
        printf("  圆%d 位于(%.1f, %.1f) 半径%.1f：%s\n",
               i, input_x[i], input_y[i], input_r[i],
               output[i] ? "碰撞" : "不碰撞");
    }

    // ===== 场景二：性能对比（与文档第 6 节性能表同构）=====
    // 每个函数各做 N 次碰撞测试，统计"每次测试耗时"与相对朴素版的加速比。
    // 注：数值随硬件/仿真环境而异，重点看相对趋势。
    const int N = 16384;   // 圆的数量（16,384，与文档一致）
    alignas(64) float xs[N], ys[N], radii[N];
    alignas(64) bool  out_buf[N];
    for (int i = 0; i < N; i++) {
        xs[i]    = (float)((i * 7)  % 20);
        ys[i]    = (float)((i * 11) % 20);
        radii[i] = (float)(i % 5);
    }

    circles input{ (size_t)N, xs, ys, radii };
    circle col; col.x = 10.0f; col.y = 10.0f; col.radius = 5.0f;

    // 朴素版：N 次单圆检测
    auto bench_plain = [&]() {
        for (int i = 0; i < N; i++) {
            circle c{ xs[i], ys[i], radii[i] };
            if (does_collide(col, c)) g_sink++;
        }
    };
    // 基本向量化版：N 次单圆检测
    auto bench_neon = [&]() {
        for (int i = 0; i < N; i++) {
            circle c{ xs[i], ys[i], radii[i] };
            if (does_collide_neon(col, c)) g_sink++;
        }
    };
    // 去交错版：一次调用处理全部 N 个圆（每 4 个一批）
    auto bench_deint = [&]() {
        does_collide_neon_deinterleaved(input, col, out_buf);
    };

    const long REPEAT = 50;
    BenchResult results[] = {
        { "does_collide",                     ns_per_op(bench_plain, REPEAT, N) },
        { "does_collide_neon",                ns_per_op(bench_neon,   REPEAT, N) },
        { "does_collide_neon_deinterleaved",  ns_per_op(bench_deint,  REPEAT, N) },
    };
    print_bench_table("性能对比（每次操作 = 1 次碰撞检测）", results, 3);

    (void)g_sink;
    return 0;
}
