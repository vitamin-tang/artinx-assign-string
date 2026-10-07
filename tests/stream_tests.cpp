// String 类流运算符（<< / >>）的自动测试 —— 选做（bonus）。
//
// 单独成文件的目的：流操作是选做内容，未实现时里程碑测试
// （tests/m1_basics.cpp ~ tests/m4_edge_cases.cpp）依然可以正常构建与通过。
//
// 构建与运行方式（默认不构建本目标）：
//   cmake -S . -B build -DENABLE_BONUS_TESTS=ON
//   cmake --build build -j
//   ctest --test-dir build --output-on-failure
//
// 说明：
//   * 测试不依赖交互输入，失败时以非零退出码结束（供 CTest 使用）；
//   * 轻量断言宏在 -DNDEBUG 下依然生效。

#include "my_string.h"

#include <cstddef>
#include <iostream>
#include <sstream>

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool ok, const char* expr, const char* file, int line) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::cerr << file << ':' << line << ": CHECK failed: " << expr << '\n';
    }
}

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

bool equals_chars(const char* actual, const char* expected) {
    std::size_t i = 0;
    for (; expected[i] != '\0'; ++i) {
        if (actual[i] != expected[i]) {
            return false;
        }
    }
    return actual[i] == '\0';
}

bool equals_cstr(const String& value, const char* expected) {
    return equals_chars(value.c_str(), expected);
}

void check_cstr(const String& value, const char* expected, const char* expr,
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
    check_cstr((value), (expected), "c_str(" #value ") == expected", __FILE__,        \
               __LINE__)

void run(const char* name, void (*test)()) {
    const int before = g_failures;
    std::cout << "[ RUN      ] " << name << '\n';
    test();
    std::cout << (g_failures == before ? "[       OK ] " : "[   FAILED ] ") << name << '\n';
}

// ---------------------------------------------------------------------------
// 输出流
// ---------------------------------------------------------------------------

void test_stream_output() {
    std::ostringstream os;
    os << String("hello") << ',' << String() << ',' << String("world");
    CHECK(equals_chars(os.str().c_str(), "hello,,world"));

    std::ostringstream os2;
    const String s("const output");
    os2 << s;
    CHECK(equals_chars(os2.str().c_str(), "const output"));
}

// ---------------------------------------------------------------------------
// 输入流
// ---------------------------------------------------------------------------

void test_stream_input() {
    std::istringstream input("hello world");
    String a;
    String b;
    input >> a >> b;
    CHECK_CSTR(a, "hello");
    CHECK_CSTR(b, "world");

    // 连续读取会替换旧内容
    std::istringstream input2("first second");
    String x("old content");
    input2 >> x;
    CHECK_CSTR(x, "first");
    input2 >> x;
    CHECK_CSTR(x, "second");

    // 前导空白被跳过
    std::istringstream input3("  \t\n leading");
    String y;
    input3 >> y;
    CHECK_CSTR(y, "leading");

    // 长单词（超过初始容量，触发多次扩容）
    const char* long_word =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz";
    std::istringstream input4(long_word);
    String z;
    input4 >> z;
    CHECK(z.size() > 64u);
    CHECK_CSTR(z, long_word);

    // 空输入：读取失败，原值保持不变
    std::istringstream empty("");
    String keep("keep");
    empty >> keep;
    CHECK(empty.fail());
    CHECK_CSTR(keep, "keep");
}

}  // namespace

int main() {
    run("stream_output", test_stream_output);
    run("stream_input", test_stream_input);

    std::cout << '\n' << "checks: " << g_checks << ", failures: " << g_failures << '\n';
    if (g_failures != 0) {
        std::cerr << "TESTS FAILED\n";
        return 1;
    }
    std::cout << "ALL TESTS PASSED\n";
    return 0;
}
