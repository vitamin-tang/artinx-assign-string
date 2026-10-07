// ============================================================================
// M3 · 移动语义与 noexcept（进阶）
//
// 覆盖：移动构造 / 移动赋值 / 自移动赋值 / "被移动后"对象的有效性与重新赋值。
//
// 只引用 M1 + M2 + M3 的函数：实现完 M3 后即可链接并通过。
// "被移动后"的对象内容未指定，因此这里只用 CHECK_VALID_CSTR 验证
// c_str() 返回的指针有效、以 '\0' 结尾，不断言它的具体内容。
// ============================================================================

#include "test_util.h"

#include <utility>

namespace {

void test_move_semantics() {
    const char* text = "a fairly long string used to exercise move construction";
    String a(text);
    String b(std::move(a));           // 移动构造
    CHECK_CSTR(b, text);
    CHECK_VALID_CSTR(a);              // 被移动后 c_str() 仍必须返回有效 C 字符串

    a = String("reused");             // 被移动后仍可重新赋值
    CHECK_CSTR(a, "reused");

    String c;
    c = std::move(b);                 // 移动赋值
    CHECK_CSTR(c, text);
    CHECK_VALID_CSTR(b);

    b = String("again");              // 被移动后仍可重新赋值
    CHECK_CSTR(b, "again");
    CHECK_INVARIANTS(b);
}

void test_moved_from_object_is_valid() {
    // 对"被移动后"的对象再次实施移动：允许，且不得 UB / 双重释放
    const char* text = "0123456789abcdefghijklmnopqrstuvwxyz";
    String x(text);
    String y(std::move(x));           // y 拿到内容；x 变成"被移动后"
    CHECK_CSTR(y, text);
    CHECK_VALID_CSTR(x);

    String z(std::move(x));           // 再移动一次：内容未指定，但操作必须安全
    CHECK_VALID_CSTR(z);              // z 是正常对象：必须是有效 C 字符串
    CHECK_INVARIANTS(z);              // 且满足 size() <= capacity() 与结尾 '\0'
    CHECK_VALID_CSTR(x);              // x 仍可安全析构、继续使用

    // 被移动后的对象重新赋值后，与普通对象一样参与运算
    x = String("recovered");
    CHECK_CSTR(x, "recovered");
    CHECK_CSTR(x + y, "recovered0123456789abcdefghijklmnopqrstuvwxyz");
    CHECK_INVARIANTS(x);
}

void test_self_move_assignment() {
    String s("self move");
    String& ref = s;                  // 经引用赋值，避免编译器对 x = std::move(x) 的告警
    s = std::move(ref);               // 自移动赋值：内容未指定，但对象必须仍然有效
    CHECK_VALID_CSTR(s);
    s = String("after self move");    // 可以重新赋值
    CHECK_CSTR(s, "after self move");
}

}  // namespace

int main() {
    testutil::run("move_semantics", test_move_semantics);
    testutil::run("moved_from_object_is_valid", test_moved_from_object_is_valid);
    testutil::run("self_move_assignment", test_self_move_assignment);

    return testutil::finish();
}
