// 09_load_store.cpp — 验证 §15/16 加载与存储：访问宽度与零/符号扩展
//
// 概念：LDR/STR 在内存与寄存器间搬运单值，访问宽度由寄存器类型 + Size 决定：
//   X=64 位，W=32 位；LDRB/STRB=字节，LDRH/STRH=半字。
//   加载窄于寄存器宽度时默认清零（零扩展）；操作名加 S 则符号扩展：
//   LDRSB 字节符号扩展，LDRSH 半字符号扩展，LDRSW 字符号扩展到 64 位。
//
// 编译：
//   aarch64-linux-gnu-g++ -O2 -march=armv8-a+simd -fno-stack-protector \
//       -S 09_load_store.cpp -o 09_load_store.s

#include <cstdint>

// 从字节地址加载并零扩展到 64 位（LDRB W0, [X0] -> 高 32 位清零）
__attribute__((noinline))
std::uint64_t load_byte_zero_ext(const std::uint8_t* p) {
    return *p;                      // 预期：LDRB W0, [X0]
}

// 从字节地址加载并符号扩展到 64 位（LDRSB X0, [X0]）
__attribute__((noinline))
std::int64_t load_byte_sign_ext(const std::int8_t* p) {
    return *p;                      // 预期：LDRSB X0, [X0]
}

// 半字符号扩展（LDRSH）
__attribute__((noinline))
std::int64_t load_half_sign_ext(const std::int16_t* p) {
    return *p;                      // 预期：LDRSH X0, [X0]
}

// 存储低字节 / 低半字
__attribute__((noinline))
void store_byte_and_half(std::uint8_t* b, std::uint16_t* h, std::uint32_t v) {
    *b = static_cast<std::uint8_t>(v);   // STRB W2, [X0]
    *h = static_cast<std::uint16_t>(v);  // STRH W2, [X1]
}

#include <cstdio>

int main() {
    std::uint8_t ub = 0xFFu;   std::int8_t sb = -1; std::int16_t sh = -2;
    std::uint8_t ob; std::uint16_t oh;
    std::printf("zero_ext=%llu sign_ext=%lld half_sign=%lld\n",
                (unsigned long long)load_byte_zero_ext(&ub),
                (long long)load_byte_sign_ext(&sb),
                (long long)load_half_sign_ext(&sh));
    store_byte_and_half(&ob, &oh, 0x1234);
    // 0xFF 零扩展 = 255；-1 符号扩展 = -1（全 1）
    return (load_byte_zero_ext(&ub) == 255 && load_byte_sign_ext(&sb) == -1) ? 0 : 1;
}