// 16_conditional_fault.cpp — 验证 §24 程序流（四）：条件故障指令
//
// 概念：§24 讲运行时一致性检查（如边界检查）常被编译成
//   "比较 + 条件跳转到 BRK 断点"的序列。Armv9.8-A 的 FEAT_CFLT
//   用 cflt* 指令直接把"条件成立"变成异常，省掉那条分支：
//     传统：cmp  x1, x10 ; b.hs .Lerror ; ... ; .Lerror: brk #0x1
//     CFLT：cflths #0, x1, x10 ; ...
//   本文件演示传统模式（可编译）；cflt* 指令需 -march=armv9.8-a，
//   而本工具链 GCC 13 最高支持 armv9.3-a，汇编器也不识别 cflt，
//   故该指令无法编译验证，仅作文字说明。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 16_conditional_fault.cpp -o 16_conditional_fault.s

#include <cstdint>
#include "test_util.hpp"

// ============================================================================
// 被测函数
// ============================================================================

// 边界检查 + 断点（§24 传统模式）：
//   index >= size 则触发 BRK，否则正常访问 a[index]
//   预期汇编：cmp  x2, x1
//             b.cs .Ltrap        ; index >= size 则跳 trap
//             ldr  w0, [x0, x2, lsl #2]
//             ret
//         .Ltrap:
//             brk  #0x3e8        ; 断点（GCC 默认 trap 立即数 1000）
__attribute__((noinline))
std::uint32_t bounds_check_load(const std::uint32_t* a, std::size_t size,
                                std::size_t index) {
    if (index >= size) {
        __builtin_trap();           // BRK #0x3e8（触发 SIGTRAP）
    }
    return a[index];                // LDR W0, [X0, X2, LSL #2]
}

// ============================================================================
// 测试
// ============================================================================

static void test_conditional_fault() {
    group("§24 条件故障（传统模式 cmp + b.cond + brk）");

    std::uint32_t arr[4] = {10, 20, 30, 40};
    CHECK_EQ(bounds_check_load(arr, 4, 2), 30u);   // 合法索引正常访问
    // 非法索引 bounds_check_load(arr, 4, 5) 会触发 BRK 导致进程崩溃，
    // 故测试中不直接调用——可通过注释中的反汇编观察该分支。
}

int main() {
    test_conditional_fault();
    return report();
}