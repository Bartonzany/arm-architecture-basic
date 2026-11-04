// saxpy_asm.S —— SAXPY 的 SVE 汇编内核
//
// 对应文档：《SVE 优化指南》5.2 / 6.2 章
// 验证点：手工 SVE 汇编的向量长度无关循环。
//   参数（PCS）：x0=x, x1=y, s0=a, w2=n
//   核心：whilelo 生成谓词 + ld1w/fmla/st1w + incw 按向量长度递增。
//
// 说明：本内核只用调用者保存的寄存器（x5、p0、z0-z2），无需保存/恢复；
//   文档原文用 .req 别名 + 保存 x19-x28/d8-d15 的写法，此处简化为等价形式。

.text
.global saxpy_asm
.type   saxpy_asm, %function

saxpy_asm:
    mov     x5, xzr              // i = 0
    dup     z0.s, z0.s[0]        // 把标量 a 广播到整个 z0

    whilelo p0.s, x5, x2         // 生成谓词：i < n 的通道为真
    b.none  .L_end               // 没有活动元素则退出

.L_loop:
    // 谓词加载 x 和 y
    ld1w    z1.s, p0/z, [x0, x5, lsl #2]
    ld1w    z2.s, p0/z, [x1, x5, lsl #2]

    // y = a*x + y（融合乘加）
    fmla    z2.s, p0/m, z1.s, z0.s

    // 谓词存储新的 y
    st1w    z2.s, p0, [x1, x5, lsl #2]

    // i += Z 寄存器中 32 位元素的个数
    incw    x5

    whilelo p0.s, x5, x2         // 更新谓词
    b.any   .L_loop

.L_end:
    ret
