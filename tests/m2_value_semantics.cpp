// ============================================================================
// M2 · 值语义（Rule of Three）、拼接与插入（必做）
//
// 覆盖：拷贝构造 / 复制赋值 / 自赋值 / operator+ / insert
//       （基本路径、多次扩容、越界异常）。
//
// 只引用 M1 + M2 的函数：本文件**刻意不使用移动构造 / 移动赋值**，
// 所以实现完 M1、M2 后即可链接并通过（M3/M4 的函数还没实现也不影响）。
// ============================================================================

#include "test_util.h"

#include <cstddef>

namespace {

// ---------------------------------------------------------------------------
// 深拷贝、复制赋值与自赋值
// ---------------------------------------------------------------------------

void test_copy_semantics() {
    String a("hello");
    String b(a);                      // 拷贝构造
    CHECK(b.size() == a.size());
    CHECK_CSTR(b, "hello");
    CHECK(b.c_str() != a.c_str());    // 深拷贝：缓冲区相互独立

    b[0] = 'H';
    CHECK_CSTR(b, "Hello");
    CHECK_CSTR(a, "hello");           // 修改副本不影响原对象

    String c;
    c = a;                            // 复制赋值
    CHECK_CSTR(c, "hello");
    CHECK(c.c_str() != a.c_str());
    c[4] = '!';
    CHECK_CSTR(c, "hell!");
    CHECK_CSTR(a, "hello");

    String d("some much longer content that forces its own allocation");
    d = a;                            // 覆盖旧内容
    CHECK_CSTR(d, "hello");

    String e("nonempty");
    String empty;
    e = empty;                        // 赋值为空串（复制赋值，不依赖移动赋值）
    CHECK(e.size() == 0u);
    CHECK_CSTR(e, "");
}

void test_self_copy_assignment() {
    String s("hello");
    String& ref = s;                  // 经引用赋值，避免编译器对 x = x 的告警
    s = ref;
    CHECK_CSTR(s, "hello");
    CHECK(s.size() == 5u);
}

// ---------------------------------------------------------------------------
// 拼接与下标
// ---------------------------------------------------------------------------

void test_operator_plus() {
    String a("hello");
    String b(" world");
    String c = a + b;
    CHECK_CSTR(c, "hello world");
    CHECK_CSTR(a, "hello");           // 原对象不受影响
    CHECK_CSTR(b, " world");

    String empty;
    CHECK_CSTR(empty + a, "hello");
    CHECK_CSTR(a + empty, "hello");
    CHECK((empty + empty).size() == 0u);
    CHECK_CSTR(a + a, "hellohello");  // 自身拼接

    String mixed = String() + a + String() + b;  // 空串参与链式拼接
    CHECK_CSTR(mixed, "hello world");

    // 连续拼接触发多次扩容，逐字符校验 "ab" x 100。
    // 写成"先算结果、再复制赋值"是为了不依赖移动赋值（那是 M3 的内容）。
    String chain;
    for (int i = 0; i < 100; ++i) {
        String part("ab");
        String next = chain + part;
        chain = next;
    }
    CHECK(chain.size() == 200u);
    CHECK(chain.capacity() >= chain.size());
    bool chain_ok = true;
    for (std::size_t i = 0; i < chain.size(); ++i) {
        chain_ok = chain_ok && (chain[i] == (i % 2u == 0u ? 'a' : 'b'));
    }
    CHECK(chain_ok);
    CHECK_INVARIANTS(chain);

    String a500;
    String b500;
    testutil::append_n(a500, 'a', 500);
    testutil::append_n(b500, 'b', 500);
    String big = a500 + b500;
    CHECK(big.size() == 1000u);
    CHECK(big[0] == 'a');
    CHECK(big[499] == 'a');
    CHECK(big[500] == 'b');
    CHECK(big[999] == 'b');
}

// ---------------------------------------------------------------------------
// 插入
// ---------------------------------------------------------------------------

void test_insert_basic() {
    String s("abcdef");
    s.insert(3, String("XY"));
    CHECK_CSTR(s, "abcXYdef");

    String t("world");
    t.insert(0, String("hello "));
    CHECK_CSTR(t, "hello world");

    String u("hello");
    u.insert(u.size(), String("!"));  // pos == size() 合法（追加）
    CHECK_CSTR(u, "hello!");

    String v("same");
    v.insert(2, String());            // 插入空串不改变内容
    CHECK_CSTR(v, "same");

    String w;                         // 向空串插入
    w.insert(0, String("first"));
    CHECK_CSTR(w, "first");

    // 插入触发扩容
    String chunk;
    testutil::append_n(chunk, 'a', 100);
    String big("z");
    big.insert(1, chunk);
    CHECK(big.size() == 101u);
    CHECK(big[0] == 'z');
    CHECK(big[1] == 'a');
    CHECK(big[100] == 'a');
    CHECK(big.c_str()[101] == '\0');
}

void test_insert_out_of_range() {
    String s("abc");
    CHECK_OUT_OF_RANGE(s.insert(4, String("x")));
    CHECK_CSTR(s, "abc");             // 失败时原对象内容不被破坏
    CHECK(s.size() == 3u);

    s.insert(3, String("d"));         // pos == size() 合法
    CHECK_CSTR(s, "abcd");

    String empty;
    CHECK_OUT_OF_RANGE(empty.insert(1, String("x")));
    CHECK_CSTR(empty, "");
}

void test_insert_into_long_string() {
    String s;
    testutil::append_n(s, 'a', 1000);
    String chunk("0123456789");
    s.insert(500, chunk);
    CHECK(s.size() == 1010u);
    CHECK_INVARIANTS(s);

    // 全内容校验：仅 [500, 510) 是数字，其余都是 'a'
    bool content_ok = true;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char expected =
            (i >= 500u && i < 510u) ? static_cast<char>('0' + (i - 500u)) : 'a';
        content_ok = content_ok && (s[i] == expected);
    }
    CHECK(content_ok);

    // 长串的深拷贝仍然正确且独立
    String copy(s);
    CHECK(copy.size() == s.size());
    CHECK(copy.c_str() != s.c_str());
    bool copy_ok = true;
    for (std::size_t i = 0; i < copy.size(); ++i) {
        copy_ok = copy_ok && (copy[i] == s[i]);
    }
    CHECK(copy_ok);
}

void test_insert_position_matrix() {
    // 在 "abcde"（长度 5）的每个合法位置插入 "XYZ"
    static const char* const expected[6] = {
        "XYZabcde", "aXYZbcde", "abXYZcde", "abcXYZde", "abcdXYZe", "abcdeXYZ",
    };
    for (std::size_t pos = 0; pos < 6u; ++pos) {
        String s("abcde");
        s.insert(pos, String("XYZ"));
        CHECK(s.size() == 8u);
        CHECK_CSTR(s, expected[pos]);
        CHECK_INVARIANTS(s);
    }
}

}  // namespace

int main() {
    testutil::run("copy_semantics", test_copy_semantics);
    testutil::run("self_copy_assignment", test_self_copy_assignment);
    testutil::run("operator_plus", test_operator_plus);
    testutil::run("insert_basic", test_insert_basic);
    testutil::run("insert_out_of_range", test_insert_out_of_range);
    testutil::run("insert_into_long_string", test_insert_into_long_string);
    testutil::run("insert_position_matrix", test_insert_position_matrix);

    return testutil::finish();
}
