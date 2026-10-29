// test_util.hpp — 轻量验证辅助（零依赖、自包含）
//
// 用于把每个知识点的验证代码组织得清晰可读：
//   被测函数（noinline，注释标注预期汇编）与测试断言分离；
//   每条断言都会打印 [OK]/[FAIL] 与实际值，便于直接看到验证结果。
//
// 用法（每个 demo 自包含，直接 #include "test_util.hpp"）：
//
//   #include "test_util.hpp"
//
//   __attribute__((noinline)) int add(int a, int b) { return a + b; }
//
//   static void test_add() {
//       group("整数加法");
//       CHECK_EQ(add(1, 2), 3);
//       CHECK(add(2, 2) == 4);
//   }
//
//   int main() {
//       test_add();
//       return report();
//   }
//
// 说明：本头仅用 <cstdio> + <type_traits>（无 <iostream>），编译开销小，
//       且不影响被测函数的独立汇编。

#ifndef ARM64_VERIFY_TEST_UTIL_HPP
#define ARM64_VERIFY_TEST_UTIL_HPP

#include <cstdio>
#include <type_traits>

static int g_checks   = 0;   // 断言总数
static int g_failures = 0;   // 失败总数

// 打印分组标题
static inline void group(const char* name) {
    std::printf("\n[%s]\n", name);
}

// 打印一个值：整数统一按 long long 打印，浮点按 double 打印
template <typename T>
static inline typename std::enable_if<std::is_floating_point<T>::value>::type
print_val(T v) { std::printf("%g", static_cast<double>(v)); }

template <typename T>
static inline typename std::enable_if<std::is_integral<T>::value>::type
print_val(T v) { std::printf("%lld", static_cast<long long>(v)); }

// 断言 actual == expected，无论通过与否都打印实际值与期望值
#define CHECK_EQ(actual, expected)                                          \
    do {                                                                    \
        auto _a = (actual);                                                 \
        auto _b = (expected);                                               \
        ++g_checks;                                                         \
        if (!(_a == _b)) {                                                  \
            ++g_failures;                                                   \
            std::printf("  [FAIL] %s == %s   (实际 ", #actual, #expected);  \
            print_val(_a); std::printf(" vs 期望 "); print_val(_b);         \
            std::printf(")\n");                                             \
        } else {                                                            \
            std::printf("  [ OK ] %s == %s   = ", #actual, #expected);      \
            print_val(_a); std::printf("\n");                               \
        }                                                                   \
    } while (0)

// 断言任意布尔条件成立
#define CHECK(cond)                                                         \
    do {                                                                    \
        ++g_checks;                                                         \
        if (cond) {                                                         \
            std::printf("  [ OK ] %s\n", #cond);                            \
        } else {                                                            \
            ++g_failures;                                                   \
            std::printf("  [FAIL] %s\n", #cond);                            \
        }                                                                   \
    } while (0)

// 打印汇总并返回退出码：0 = 全部通过，1 = 存在失败
static inline int report() {
    std::printf("\n结果: %d/%d 通过\n", g_checks - g_failures, g_checks);
    return g_failures == 0 ? 0 : 1;
}

#endif  // ARM64_VERIFY_TEST_UTIL_HPP