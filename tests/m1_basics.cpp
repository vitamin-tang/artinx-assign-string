// ============================================================================
// M1 · 基础接口与不变量（必做）
//
// 覆盖：默认构造 / const char* 构造 / 析构（隐式）/ size / capacity /
//       operator[] / at（含越界异常）/ c_str 与隐式转换 / push_back。
//
// 只引用 M1 的函数：实现完 M1 后，本文件就能链接并通过。
// 公开接口签名由 tests/test_util.h 里的 static_assert 检查。
// ============================================================================

#include "test_util.h"

#include <cstddef>

namespace {

// ---------------------------------------------------------------------------
// 构造、析构与空字符串不变量
// ---------------------------------------------------------------------------

void test_default_constructed_empty() {
    String s;
    CHECK(s.size() == 0u);
    CHECK(s.capacity() >= 16u);       // 契约：默认构造的空串容量不小于 16（不含 '\0'）
    CHECK(s.c_str() != nullptr);
    CHECK(s.c_str()[0] == '\0');      // 空串仍是以 '\0' 结尾的有效字符串
    CHECK_CSTR(s, "");
}

void test_construct_from_c_str() {
    String s("hello");
    CHECK(s.size() == 5u);
    CHECK(s.capacity() >= 5u);
    CHECK_CSTR(s, "hello");
    CHECK(s[0] == 'h');
    CHECK(s[4] == 'o');

    String empty("");
    CHECK(empty.size() == 0u);
    CHECK(empty.c_str()[0] == '\0');
    CHECK_CSTR(empty, "");

    String from_null(nullptr);        // 作业要求：nullptr 视为空串
    CHECK(from_null.size() == 0u);
    CHECK_CSTR(from_null, "");

    const char* long_literal =
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    String big(long_literal);
    CHECK(big.size() == 62u);
    CHECK_CSTR(big, long_literal);
    CHECK(big.c_str()[62] == '\0');
}

// ---------------------------------------------------------------------------
// 下标与带检查访问
// ---------------------------------------------------------------------------

void test_subscript() {
    String s("abc");
    CHECK(s[0] == 'a');
    s[1] = 'B';
    CHECK_CSTR(s, "aBc");
    CHECK(s[3] == '\0');              // s[size()] 可读，与 std::string 一致

    const String cs("const");
    CHECK(cs[0] == 'c');
    CHECK(cs[4] == 't');
    CHECK(cs[5] == '\0');             // const 对象也保持以 '\0' 结尾

    const char* converted = cs;       // 隐式转换为 const char*
    CHECK(converted == cs.c_str());
    CHECK(testutil::equals_chars(converted, "const"));
}

void test_at() {
    String s("abc");
    CHECK(s.at(0) == 'a');
    s.at(1) = 'B';
    CHECK_CSTR(s, "aBc");

    const String cs("xy");
    CHECK(cs.at(1) == 'y');
    CHECK_OUT_OF_RANGE(cs.at(2));
    CHECK_OUT_OF_RANGE(s.at(3));
    CHECK_OUT_OF_RANGE(String().at(0));
}

// ---------------------------------------------------------------------------
// 长度与容量
// ---------------------------------------------------------------------------

void test_size_and_capacity_semantics() {
    String s;                          // 空串，容量 >= 16（见上面的契约）
    const std::size_t cap = s.capacity();

    // 填满到 capacity：capacity() 不含结尾 '\0'，因此不应触发扩容
    while (s.size() < cap) {
        s.push_back('a');
    }
    CHECK(s.size() == cap);
    CHECK(s.capacity() == cap);

    // 超出容量必须扩容，且内容完好、仍以 '\0' 结尾
    s.push_back('b');
    CHECK(s.size() == cap + 1);
    CHECK(s.capacity() >= s.size());
    CHECK(s[0] == 'a');
    CHECK(s[cap] == 'b');
    CHECK(s.c_str()[cap + 1] == '\0');
}

void test_capacity_growth_contract() {
    // 连续追加：容量单调不减，且始终 >= size()
    String s;
    std::size_t prev_cap = s.capacity();

    for (std::size_t i = 0; i < 100u; ++i) {
        s.push_back(static_cast<char>('a' + static_cast<int>(i % 26u)));
        CHECK(s.capacity() >= prev_cap);
        CHECK(s.capacity() >= s.size());
        prev_cap = s.capacity();
    }
    CHECK(s.size() == 100u);
    CHECK_INVARIANTS(s);
}

}  // namespace

int main() {
    testutil::run("default_constructed_empty", test_default_constructed_empty);
    testutil::run("construct_from_c_str", test_construct_from_c_str);
    testutil::run("subscript", test_subscript);
    testutil::run("at", test_at);
    testutil::run("size_and_capacity_semantics", test_size_and_capacity_semantics);
    testutil::run("capacity_growth_contract", test_capacity_growth_contract);

    return testutil::finish();
}
