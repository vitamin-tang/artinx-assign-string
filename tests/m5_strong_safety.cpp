// ============================================================================
// M5 · 强异常安全（进阶，依赖 M1 + M2）
//
// 通过替换全局 operator new[] / delete[]，在指定的第 k 次数组分配时抛出
// std::bad_alloc，用来验证 TASKS.md 4.1 要求的强异常安全：
//   一旦分配失败，原对象的内容与容量必须保持原样，且不能泄漏内存。
//
// 说明：
//   * 只替换/统计**数组**分配（new[] / delete[]），不影响 iostream 等内部
//     使用的标量 new，也不影响测试框架本身；
//   * 前提是实现用 new char[] 分配缓冲区（本作业的推荐做法）。如果你用了
//     malloc / std::allocator 等其它方式，本测试注入不到（作业要求用 new[]）；
//   * 在默认的 ASan/UBSan 构建与普通构建下都可以运行。
// ============================================================================

#include "test_util.h"

#include <cstddef>
#include <cstdlib>
#include <new>

// ---------------------------------------------------------------------------
// 可注入失败的全局 new[] / delete[]
// ---------------------------------------------------------------------------

namespace {

bool g_fail_armed = false;      // 是否处于注入窗口
long g_fail_countdown = 0;      // 距离失败还剩几次分配（0 = 下一次就失败）
long g_alloc_balance = 0;       // 当前未释放的 new[] 分配数

}  // namespace

void* operator new[](std::size_t size) {
    if (g_fail_armed) {
        if (g_fail_countdown == 0) {
            g_fail_armed = false;  // 只失败一次，避免后续清理也被注入
            throw std::bad_alloc();
        }
        --g_fail_countdown;
    }
    void* memory = std::malloc(size == 0u ? 1u : size);
    if (memory == nullptr) {
        throw std::bad_alloc();
    }
    ++g_alloc_balance;
    return memory;
}

void operator delete[](void* memory) noexcept {
    if (memory != nullptr) {
        --g_alloc_balance;
        std::free(memory);
    }
}

void operator delete[](void* memory, std::size_t) noexcept {
    ::operator delete[](memory);
}

namespace {

void arm_alloc_failure(long fail_at) {
    g_fail_countdown = fail_at;
    g_fail_armed = true;
}

void disarm_alloc_failure() {
    g_fail_armed = false;
}

long alloc_balance() {
    return g_alloc_balance;
}

bool filled_with(const String& value, char ch) {
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] != ch) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// push_back：扩容失败时长度、容量、内容都不能变
// ---------------------------------------------------------------------------

void test_push_back_strong_safety() {
    String s;
    testutil::append_n(s, 'a', s.capacity());  // 恰好填满容量
    const std::size_t cap = s.capacity();
    const long before = alloc_balance();

    arm_alloc_failure(0);                      // 下一次 new[] 失败
    bool threw = false;
    try {
        s.push_back('z');
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    disarm_alloc_failure();

    CHECK(threw);                              // 容量不足需要分配 → 异常必须传出来
    CHECK(s.size() == cap);                    // 强保证：长度、容量、内容都不变
    CHECK(s.capacity() == cap);
    CHECK(filled_with(s, 'a'));
    CHECK_INVARIANTS(s);
    CHECK(alloc_balance() == before);          // 失败不能泄漏

    s.push_back('z');                          // 失败之后对象仍可正常使用
    CHECK(s.size() == cap + 1u);
    CHECK(s[cap] == 'z');
    CHECK_INVARIANTS(s);
}

// ---------------------------------------------------------------------------
// insert：在每一次分配上分别注入失败，要么完整成功、要么完全不变
// ---------------------------------------------------------------------------

void test_insert_strong_safety() {
    for (long fail_at = 0; fail_at < 3; ++fail_at) {
        String s;
        testutil::append_n(s, 'a', s.capacity());  // size == capacity，插入必然要扩容
        const std::size_t n = s.size();
        const String chunk("XYZ");
        const long before = alloc_balance();

        bool threw = false;
        bool succeeded = false;
        arm_alloc_failure(fail_at);
        try {
            s.insert(5, chunk);
            succeeded = true;
        } catch (const std::bad_alloc&) {
            threw = true;
        }
        disarm_alloc_failure();

        if (fail_at == 0) {
            CHECK(threw);                          // 第一次分配就失败，必须抛出
        }
        if (threw) {
            CHECK(s.size() == n);                  // 失败：内容与长度完全不变
            CHECK(filled_with(s, 'a'));
        } else {
            CHECK(succeeded);                      // 成功：内容必须正确
            CHECK(s.size() == n + 3u);
            CHECK(s[5] == 'X');
            CHECK(s[6] == 'Y');
            CHECK(s[7] == 'Z');
            bool rest_ok = true;                   // 除插入位置外全是 'a'
            for (std::size_t i = 0; i < s.size(); ++i) {
                if (i >= 5u && i < 8u) {
                    continue;
                }
                rest_ok = rest_ok && (s[i] == 'a');
            }
            CHECK(rest_ok);
        }
        CHECK_INVARIANTS(s);
        CHECK(alloc_balance() == before);          // 无论成功失败都不泄漏
    }
}

// ---------------------------------------------------------------------------
// 复制赋值：失败时 *this 保持原样（copy-and-swap 的强保证）
// ---------------------------------------------------------------------------

void test_copy_assign_strong_safety() {
    String target("original");
    String source;
    testutil::append_n(source, 'b', 200);  // 长内容，赋值必然需要重新分配
    const long before = alloc_balance();

    arm_alloc_failure(0);
    bool threw = false;
    try {
        target = source;
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    disarm_alloc_failure();

    CHECK(threw);
    CHECK_CSTR(target, "original");        // 失败时 *this 完全不变
    CHECK(target.size() == 8u);
    CHECK(target.capacity() >= 8u);
    CHECK_INVARIANTS(target);
    CHECK(alloc_balance() == before);

    target = source;                       // 失败后仍可正常赋值
    CHECK(target.size() == 200u);
    CHECK(target[0] == 'b');
    CHECK_INVARIANTS(target);
}

// ---------------------------------------------------------------------------
// 拷贝构造 / 默认构造：构造失败不能泄漏
// ---------------------------------------------------------------------------

void test_copy_construct_failure_no_leak() {
    String source("a source string that needs its own allocation");
    const long before = alloc_balance();

    arm_alloc_failure(0);
    bool threw = false;
    try {
        String copy(source);
        (void)copy;
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    disarm_alloc_failure();

    CHECK(threw);
    CHECK_CSTR(source, "a source string that needs its own allocation");  // 源对象不受影响
    CHECK(alloc_balance() == before);                                     // 不能泄漏
}

void test_default_construct_failure_no_leak() {
    const long before = alloc_balance();

    arm_alloc_failure(0);
    bool threw = false;
    try {
        String s;
        (void)s;
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    disarm_alloc_failure();

    CHECK(threw);
    CHECK(alloc_balance() == before);
}

// ---------------------------------------------------------------------------
// operator+：失败时两个操作数不受影响，也不泄漏
// ---------------------------------------------------------------------------

void test_operator_plus_failure() {
    String a("hello");
    String b(" world");
    const long before = alloc_balance();

    arm_alloc_failure(0);
    bool threw = false;
    try {
        String sum = a + b;
        (void)sum;
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    disarm_alloc_failure();

    CHECK(threw);
    CHECK_CSTR(a, "hello");                // 操作数不受影响
    CHECK_CSTR(b, " world");
    CHECK(alloc_balance() == before);      // 失败不泄漏

    CHECK_CSTR(a + b, "hello world");      // 之后仍能正常拼接
}

}  // namespace

int main() {
    testutil::run("push_back_strong_safety", test_push_back_strong_safety);
    testutil::run("insert_strong_safety", test_insert_strong_safety);
    testutil::run("copy_assign_strong_safety", test_copy_assign_strong_safety);
    testutil::run("copy_construct_failure_no_leak", test_copy_construct_failure_no_leak);
    testutil::run("default_construct_failure_no_leak",
                  test_default_construct_failure_no_leak);
    testutil::run("operator_plus_failure", test_operator_plus_failure);

    return testutil::finish();
}
