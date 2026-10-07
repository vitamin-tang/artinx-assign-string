// ============================================================================
// M4 · 边界情况与自操作（进阶）
//
// 覆盖：自插入 / 自交换 / 容量边界（恰好填满、恰好差 1）/ 空串的各种自操作 /
//       深拷贝独立性压力测试。
//
// 依赖 M1~M4 的全部函数（本文件也使用了移动赋值）。
// ============================================================================

#include "test_util.h"

#include <cstddef>
#include <utility>

namespace {

// ---------------------------------------------------------------------------
// 自插入
// ---------------------------------------------------------------------------

void test_insert_self() {
    String s("abc");
    s.insert(0, s);                   // s.insert(0, s) 自插入
    CHECK_CSTR(s, "abcabc");

    String t("abcdef");
    t.insert(2, t);
    CHECK_CSTR(t, "ababcdefcdef");

    String u("xy");
    u.insert(u.size(), u);
    CHECK_CSTR(u, "xyxy");

    // 反复自插入：指数增长 + 多次扩容；对全部 512 个字符校验（奇偶位置交替 a/b）
    String v("ab");
    for (int i = 0; i < 8; ++i) {
        v.insert(v.size(), v);
    }
    CHECK(v.size() == 512u);
    CHECK(v.capacity() >= v.size());
    bool pattern_ok = true;
    for (std::size_t i = 0; i < v.size(); ++i) {
        pattern_ok = pattern_ok && (v[i] == (i % 2u == 0u ? 'a' : 'b'));
    }
    CHECK(pattern_ok);
    CHECK_INVARIANTS(v);
}

// ---------------------------------------------------------------------------
// 容量边界
// ---------------------------------------------------------------------------

void test_insert_capacity_boundaries() {
    // 从空串出发利用初始容量构造两个边界（默认构造的容量 >= 16，由 M1 契约保证）
    String s;
    const std::size_t cap = s.capacity();
    CHECK(cap >= 16u);

    // 情况 1：插入后长度恰好等于 capacity（不应重新分配，走原地路径）
    String filler;
    testutil::append_n(filler, 'x', cap);
    s.insert(s.size(), filler);        // 恰好填满到 capacity
    CHECK(s.size() == cap);
    CHECK(s.capacity() == cap);        // 未超过容量：不得重新分配
    CHECK(s[0] == 'x');
    CHECK(s[cap - 1u] == 'x');
    CHECK_INVARIANTS(s);

    // 情况 2：再插入 1 个字符使长度恰为 capacity + 1（必须扩容）
    const std::size_t pos = cap / 2u;
    s.insert(pos, String("y"));
    CHECK(s.size() == cap + 1u);
    CHECK(s.capacity() >= s.size());
    CHECK(s[pos] == 'y');
    CHECK(s[cap] == 'x');              // 原最后一个字符后移一位
    CHECK_INVARIANTS(s);
}

void test_insert_self_with_growth() {
    // 自插入 + 中间位置 + 需要扩容：重叠复制最容易写错的组合
    String s;
    const std::size_t n = s.capacity();  // 先把长度填到恰好等于容量
    CHECK(n >= 6u);                      // 保证下面的 pos = 5 合法
    while (s.size() < n) {
        s.push_back(static_cast<char>('a' + static_cast<int>(s.size() % 26u)));
    }
    CHECK(s.size() == n);

    const std::size_t pos = 5;
    s.insert(pos, s);                    // 长度翻倍，必然触发扩容
    CHECK(s.size() == 2u * n);
    CHECK(s.capacity() >= s.size());
    CHECK_INVARIANTS(s);

    // 全内容校验：前缀 [0, pos)、被插入的原串、以及原串从 pos 起的尾部
    bool content_ok = true;
    for (std::size_t i = 0; i < s.size(); ++i) {
        std::size_t src = 0;
        if (i < pos) {
            src = i;
        } else if (i < pos + n) {
            src = i - pos;
        } else {
            src = i - n;
        }
        const char expected = static_cast<char>('a' + static_cast<int>(src % 26u));
        content_ok = content_ok && (s[i] == expected);
    }
    CHECK(content_ok);
}

// ---------------------------------------------------------------------------
// swap 与空串
// ---------------------------------------------------------------------------

void test_swap() {
    String a("abcd");
    String b("hello, world");
    const std::size_t a_size = a.size();
    const std::size_t b_size = b.size();

    a.swap(b);
    CHECK(a.size() == b_size);
    CHECK(b.size() == a_size);
    CHECK_CSTR(a, "hello, world");
    CHECK_CSTR(b, "abcd");
    CHECK_INVARIANTS(a);
    CHECK_INVARIANTS(b);

    // 自交换
    String& ref = a;
    a.swap(ref);
    CHECK_CSTR(a, "hello, world");

    // 与空串交换
    String empty;
    empty.swap(b);
    CHECK(empty.size() == 4u);
    CHECK_CSTR(empty, "abcd");
    CHECK(b.size() == 0u);
    CHECK_CSTR(b, "");

    // 交换后两边都可继续使用
    a[0] = 'H';
    CHECK_CSTR(a, "Hello, world");
    b = String("reuse");
    CHECK_CSTR(b, "reuse");
}

void test_empty_string_operations() {
    String e;
    CHECK_INVARIANTS(e);

    String copy(e);
    CHECK(copy.size() == 0u);
    String assigned("nonempty");
    assigned = e;
    CHECK(assigned.size() == 0u);

    e.insert(0, String());  // 空串插入空串
    CHECK(e.size() == 0u);
    e.insert(0, e);         // 空串自插入
    CHECK(e.size() == 0u);
    e.insert(e.size(), e);
    CHECK(e.size() == 0u);
    CHECK_INVARIANTS(e);

    String sum = e + e;
    CHECK(sum.size() == 0u);
    CHECK_INVARIANTS(sum);

    String& ref = e;  // 自赋值 / 自移动（经引用避免编译器告警）
    e = ref;
    CHECK(e.size() == 0u);
    e = std::move(ref);
    e = String("recovered");
    CHECK_CSTR(e, "recovered");

    // 空串可以正常长起来（用一个新的空串，避免受前面内容影响）
    String fresh;
    fresh.push_back('x');
    CHECK_CSTR(fresh, "x");
    fresh[0] = 'y';
    CHECK_CSTR(fresh, "y");
    CHECK_INVARIANTS(fresh);
}

// ---------------------------------------------------------------------------
// 深拷贝独立性压力测试
// ---------------------------------------------------------------------------

void test_deep_copy_independence_stress() {
    // 构造 200 字符的周期串 "0123456789" x 20
    String a;
    const String chunk("0123456789");
    for (int i = 0; i < 20; ++i) {
        a.insert(a.size(), chunk);
    }
    CHECK(a.size() == 200u);

    String b(a);  // 拷贝构造
    String c;
    c = a;        // 复制赋值

    // 对 b 做大量修改（自插入、扩容、改字符）
    b.insert(100, b);
    b.push_back('!');
    b[0] = '#';
    CHECK(b.size() == 401u);
    CHECK(b[0] == '#');
    CHECK(b[1] == '1');
    CHECK(b[99] == '9');
    CHECK(b[100] == '0');
    CHECK(b[399] == '9');
    CHECK(b[400] == '!');
    CHECK_INVARIANTS(b);

    // c 被覆盖为短串
    c = String("short");
    CHECK_CSTR(c, "short");

    // a 必须完全不受影响（逐字符校验）
    CHECK(a.size() == 200u);
    bool a_ok = true;
    for (std::size_t i = 0; i < a.size(); ++i) {
        a_ok = a_ok && (a[i] == static_cast<char>('0' + static_cast<int>(i % 10u)));
    }
    CHECK(a_ok);
    CHECK_INVARIANTS(a);
}

}  // namespace

int main() {
    testutil::run("insert_self", test_insert_self);
    testutil::run("insert_capacity_boundaries", test_insert_capacity_boundaries);
    testutil::run("insert_self_with_growth", test_insert_self_with_growth);
    testutil::run("swap", test_swap);
    testutil::run("empty_string_operations", test_empty_string_operations);
    testutil::run("deep_copy_independence_stress", test_deep_copy_independence_stress);

    return testutil::finish();
}
