// 09_load_store.cpp — 验证 §15/16 加载与存储：访问宽度与零/符号扩展
//
// 概念：LDR/STR 访问宽度由寄存器类型 + Size 决定：X=64、W=32、B=8、H=16。
//   加载窄于寄存器宽度时默认清零（零扩展）；操作名加 S 则符号扩展：
//   LDRSB 字节符号扩展，LDRSH 半字符号扩展。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 09_load_store.cpp -o 09_load_store.s

#include <cstdint>
#include "test_util.hpp"

// ============================================================================
// 被测函数
// ============================================================================

// 从字节地址加载并零扩展到 64 位（LDRB W0, [X0]）
__attribute__((noinline))
std::uint64_t load_byte_zero_ext(const std::uint8_t* p) {
    return *p;
}

// 从字节地址加载并符号扩展到 64 位（LDRSB X0, [X0]）
__attribute__((noinline))
std::int64_t load_byte_sign_ext(const std::int8_t* p) {
    return *p;
}

// 半字符号扩展（LDRSH）
__attribute__((noinline))
std::int64_t load_half_sign_ext(const std::int16_t* p) {
    return *p;
}

// 存储低字节 / 低半字
__attribute__((noinline))
void store_byte_and_half(std::uint8_t* b, std::uint16_t* h, std::uint32_t v) {
    *b = static_cast<std::uint8_t>(v);   // STRB
    *h = static_cast<std::uint16_t>(v);  // STRH
}

// ============================================================================
// 测试
// ============================================================================

static void test_load_store() {
    group("§15/16 访问宽度与零/符号扩展");

    std::uint8_t ub = 0xFFu;    std::int8_t sb = -1; std::int16_t sh = -2;
    std::uint8_t ob;            std::uint16_t oh;

    CHECK_EQ(load_byte_zero_ext(&ub), 255ull);  // LDRB 零扩展
    CHECK_EQ(load_byte_sign_ext(&sb), -1ll);    // LDRSB 符号扩展
    CHECK_EQ(load_half_sign_ext(&sh), -2ll);    // LDRSH 符号扩展

    store_byte_and_half(&ob, &oh, 0x1234);
    CHECK_EQ(ob, 0x34u);        // STRB 存低 8 位
    CHECK_EQ(oh, 0x1234u);      // STRH 存低 16 位
}

int main() {
    test_load_store();
    return report();
}