// 02_uaddlb.c —— SVE2 加宽加法内建函数（UADDLB）
//
// 对应文档：《SVE2 入门指南》第 5 节「使用指令函数」
// 验证点：svaddlb 内建函数直接对应 SVE2 的 uaddlb 指令（无符号加长-底部）。
//   这是 SVE2 把 Neon 的 UADDL 指令"可扩展化"的一个例子 —— Neon 按低半/高半
//   处理，SVE2 按奇偶元素交错处理（见文档图 4-1）。
//
// 预期反汇编：uaddlb z0.d, z0.s, z1.s（一条指令完成 32 位偶元素到 64 位的加宽加）。

#include <arm_sve.h>
#include <stdio.h>

// 偶元素（底部）加宽加法：Zs1、Zs2 的偶 32 位元素相加，结果为 64 位
svuint64_t uaddlb_array(svuint32_t Zs1, svuint32_t Zs2) {
    svuint64_t result = svaddlb(Zs1, Zs2);
    return result;
}

int main(void) {
    // 用全真谓词 + 已知长度，演示 uaddlb 的效果
    svbool_t pg = svptrue_b32();
    svuint32_t a = svdup_u32(100);
    svuint32_t b = svdup_u32(23);

    svuint64_t c = uaddlb_array(a, b);

    uint64_t r[2];
    svst1(svptrue_b64(), r, c);

    printf("uaddlb 结果 (100 + 23 的加宽加): %llu, %llu\n",
           (unsigned long long)r[0], (unsigned long long)r[1]);
    return 0;
}
