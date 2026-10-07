#ifndef ASSIGNMENT2_TEST_UTIL_H
#define ASSIGNMENT2_TEST_UTIL_H

// ============================================================================
// 里程碑测试共用的轻量测试工具 + 公开接口契约检查。
//
// 说明（给同学）：
//   * 每个里程碑一个测试可执行文件（m1_basics / m2_value_semantics /
//     m3_move / m4_edge_cases），它们只引用"本里程碑及更早里程碑"的函数，
//     所以只实现 M1 时 m1_basics 也能正常链接并运行；
//   * 断言宏在 -DNDEBUG 下依然生效；
//   * 本文件与 tests/ 下的其它文件一样，**请勿修改**。
//
// 说明（给出题人）：
//   * 文件末尾的 static_assert 只检查 include/my_string.h 里的公开声明，
//     因此任何对公开接口（返回类型 / const / noexcept）的改动都会让
//     所有里程碑测试**编译失败**，无需人工比对签名。
// ============================================================================

#include "my_string.h"

#include <cstddef>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace testutil {

inline int g_checks = 0;
inline int g_failures = 0;

inline void check(bool ok, const char* expr, const char* file, int line) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::cerr << file << ':' << line << ": CHECK failed: " << expr << '\n';
    }
}

#define CHECK(cond) ::testutil::check((cond), #cond, __FILE__, __LINE__)

inline bool equals_chars(const char* actual, const char* expected) {
    std::size_t i = 0;
    for (; expected[i] != '\0'; ++i) {
        if (actual[i] != expected[i]) {
            return false;
        }
    }
    return actual[i] == '\0';
}

inline bool equals_cstr(const String& value, const char* expected) {
    return equals_chars(value.c_str(), expected);
}

inline void check_cstr(const String& value, const char* expected, const char* expr,
                       const char* file, int line) {
    ++g_checks;
    if (!equals_cstr(value, expected)) {
        ++g_failures;
        std::cerr << file << ':' << line << ": CHECK failed: " << expr
                  << " (actual=\"" << value.c_str() << "\", expected=\"" << expected
                  << "\")\n";
    }
}

#define CHECK_CSTR(value, expected)                                                   \
    ::testutil::check_cstr((value), (expected), "c_str(" #value ") == expected",      \
                           __FILE__, __LINE__)

#define CHECK_OUT_OF_RANGE(expr)                                                      \
    do {                                                                              \
        bool caught = false;                                                          \
        try {                                                                         \
            (void)(expr);                                                             \
        } catch (const std::out_of_range&) {                                          \
            caught = true;                                                            \
        } catch (...) {                                                               \
        }                                                                             \
        ::testutil::check(caught, #expr " throws std::out_of_range", __FILE__,        \
                          __LINE__);                                                  \
    } while (false)

// 结构不变量：size() <= capacity()，且 c_str()[size()] 恒为 '\0'
#define CHECK_INVARIANTS(value)                                                       \
    do {                                                                              \
        ::testutil::check((value).size() <= (value).capacity(),                       \
                          "size() <= capacity() (" #value ")", __FILE__, __LINE__);   \
        ::testutil::check((value).c_str()[(value).size()] == '\0',                    \
                          "c_str()[size()] == '\\0' (" #value ")", __FILE__,          \
                          __LINE__);                                                  \
    } while (false)

// 只验证"c_str() 返回有效且以 '\0' 结尾的指针"：不假设内容，
// 因此对空串、"被移动后"的对象同样适用（被移动后内容未指定）。
// 读到 '\0' 的过程也会让 ASan 抓到悬空指针（use-after-free）。
inline void check_valid_cstr(const String& value, const char* expr, const char* file,
                             int line) {
    ++g_checks;
    const char* data = value.c_str();
    if (data == nullptr) {
        ++g_failures;
        std::cerr << file << ':' << line << ": CHECK failed: c_str(" << expr
                  << ") != nullptr\n";
        return;
    }
    const std::size_t kLimit = 1u << 20;  // 仅防止实现有 bug 时无限读
    std::size_t n = 0;
    while (n < kLimit && data[n] != '\0') {
        ++n;
    }
    if (n == kLimit) {
        ++g_failures;
        std::cerr << file << ':' << line << ": CHECK failed: c_str(" << expr
                  << ") is a valid C string (no '\\0' found)\n";
    }
}

#define CHECK_VALID_CSTR(value)                                                       \
    ::testutil::check_valid_cstr((value), #value, __FILE__, __LINE__)

// 向 out 末尾追加 count 个 ch。只用到 M1 的 push_back，
// 因此不会因为"返回 String 临时对象"而依赖拷贝/移动构造。
inline void append_n(String& out, char ch, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        out.push_back(ch);
    }
}

// 运行一条用例：捕获意外抛出的异常，避免整个测试进程 abort 后
// 连"是哪条用例挂了"都看不到。
inline void run(const char* name, void (*test)()) {
    const int before = g_failures;
    std::cerr << "[ RUN      ] " << name << '\n';
    try {
        test();
    } catch (const std::exception& e) {
        ++g_checks;
        ++g_failures;
        std::cerr << "[ EXCEPTION] " << name << ": " << e.what() << '\n';
    } catch (...) {
        ++g_checks;
        ++g_failures;
        std::cerr << "[ EXCEPTION] " << name << ": unknown exception\n";
    }
    std::cerr << (g_failures == before ? "[       OK ] " : "[   FAILED ] ") << name
              << '\n';
}

inline int finish() {
    std::cout << '\n' << "checks: " << g_checks << ", failures: " << g_failures << '\n';
    if (g_failures != 0) {
        std::cerr << "TESTS FAILED\n";
        return 1;
    }
    std::cout << "ALL TESTS PASSED\n";
    return 0;
}

// ---------------------------------------------------------------------------
// 公开接口契约（编译期检查）
//
// 这些断言只看 include/my_string.h 的声明：如果公开接口的函数名、参数、
// 返回类型、const / noexcept 被改动，所有里程碑测试都会在编译期失败。
// 实现是否正确由运行期测试负责，这里管的是"签名不许动"。
// ---------------------------------------------------------------------------

// 构造 / 析构 / 赋值
static_assert(std::is_default_constructible<String>::value,
              "String 必须可以默认构造");
static_assert(std::is_constructible<String, const char*>::value,
              "String 必须可以由 const char* 构造");
static_assert(std::is_copy_constructible<String>::value, "String 必须可以拷贝构造");
static_assert(std::is_move_constructible<String>::value, "String 必须可以移动构造");
static_assert(std::is_copy_assignable<String>::value, "String 必须可以复制赋值");
static_assert(std::is_move_assignable<String>::value, "String 必须可以移动赋值");
static_assert(std::is_destructible<String>::value, "String 必须可以析构");
static_assert(std::is_nothrow_move_constructible<String>::value,
              "移动构造必须是 noexcept");
static_assert(std::is_nothrow_move_assignable<String>::value,
              "移动赋值必须是 noexcept");

// noexcept 契约
static_assert(noexcept(std::declval<String&>()[0]), "operator[] 必须是 noexcept");
static_assert(noexcept(std::declval<const String&>()[0]),
              "const operator[] 必须是 noexcept");
static_assert(noexcept(std::declval<const String&>().size()), "size() 必须是 noexcept");
static_assert(noexcept(std::declval<const String&>().capacity()),
              "capacity() 必须是 noexcept");
static_assert(noexcept(std::declval<const String&>().c_str()),
              "c_str() 必须是 noexcept");
static_assert(noexcept(static_cast<const char*>(std::declval<const String&>())),
              "operator const char* 必须是 noexcept");
static_assert(noexcept(std::declval<String&>().swap(std::declval<String&>())),
              "swap() 必须是 noexcept");

// 返回类型契约
static_assert(std::is_same<decltype(std::declval<String&>()[0]), char&>::value,
              "非 const operator[] 必须返回 char&");
static_assert(std::is_same<decltype(std::declval<const String&>()[0]),
                           const char&>::value,
              "const operator[] 必须返回 const char&");
static_assert(std::is_same<decltype(std::declval<String&>().at(0)), char&>::value,
              "非 const at() 必须返回 char&");
static_assert(std::is_same<decltype(std::declval<const String&>().at(0)),
                           const char&>::value,
              "const at() 必须返回 const char&");
static_assert(std::is_same<decltype(std::declval<const String&>().size()),
                           std::size_t>::value,
              "size() 必须返回 std::size_t");
static_assert(std::is_same<decltype(std::declval<const String&>().capacity()),
                           std::size_t>::value,
              "capacity() 必须返回 std::size_t");
static_assert(std::is_same<decltype(std::declval<const String&>().c_str()),
                           const char*>::value,
              "c_str() 必须返回 const char*");
static_assert(std::is_same<decltype(std::declval<const String&>() +
                                    std::declval<const String&>()),
                           String>::value,
              "operator+ 必须返回 String");
static_assert(std::is_same<decltype(std::declval<String&>() =
                                    std::declval<const String&>()),
                           String&>::value,
              "复制赋值必须返回 String&");
static_assert(std::is_same<decltype(std::declval<String&>() =
                                    std::declval<String&&>()),
                           String&>::value,
              "移动赋值必须返回 String&");
static_assert(std::is_same<decltype(std::declval<String&>().insert(
                               0u, std::declval<const String&>())),
                           void>::value,
              "insert(pos, str) 必须返回 void");
static_assert(std::is_same<decltype(std::declval<String&>().push_back('a')),
                           void>::value,
              "push_back(ch) 必须返回 void");
static_assert(std::is_same<decltype(std::declval<String&>().swap(
                               std::declval<String&>())),
                           void>::value,
              "swap(other) 必须返回 void");
static_assert(std::is_convertible<const String&, const char*>::value,
              "String 必须能隐式转换为 const char*");

}  // namespace testutil

#endif  // ASSIGNMENT2_TEST_UTIL_H
