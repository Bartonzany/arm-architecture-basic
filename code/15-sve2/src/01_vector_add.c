// 01_vector_add.c —— SVE2 谓词向量加法（向量长度无关）
//
// 对应文档：《SVE2 入门指南》第 3 节「逐通道谓词」
// 验证点：
//   - 用 svwhilelt_b32 生成谓词（只处理 i < n 的活动通道），消除循环尾处理。
//   - 用 svcntw() 得到运行时向量通道数，代码不硬编码 128 位 —— 同一份二进制
//     可在 128~2048 位各种向量长度的硬件上跑。
//   - svld1 / svadd_x / svst1 是谓词化的加载 / 加法 / 存储。
//
// 预期反汇编：whilelo / ld1w / add z0.s, p0/m / st1w，而不是标量循环。

#include <arm_sve.h>
#include <stdio.h>

void vector_add(const int32_t *a, const int32_t *b, int32_t *c, int n) {
    for (int i = 0; i < n; i += (int)svcntw()) {
        svbool_t pg = svwhilelt_b32(i, n);   // 谓词：i < n 的通道为真
        svint32_t va = svld1(pg, a + i);
        svint32_t vb = svld1(pg, b + i);
        svint32_t vc = svadd_x(pg, va, vb);  // 谓词下的向量加法
        svst1(pg, c + i, vc);
    }
}

int main(void) {
    enum { N = 16 };
    int32_t a[N], b[N], c[N];

    for (int i = 0; i < N; i++) {
        a[i] = i;
        b[i] = i * 10;
    }

    vector_add(a, b, c, N);

    printf("vector_add 结果 (a[i] + b[i]):\n");
    for (int i = 0; i < N; i++) {
        printf("  %d + %d = %d\n", a[i], b[i], c[i]);
    }
    return 0;
}
