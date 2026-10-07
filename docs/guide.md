# 实现指南：异常、移动语义与 String 实现

这份文档面向**没有系统学过异常、右值引用与 `noexcept` 的同学**。目标很明确：

> 读完第一部分，你能看懂 `TASKS.md` 和随附测试里的相关写法；
> 读完第二部分，你能按步骤把 `include/my_string.h` 里的接口实现出来。

文档分两部分：

- **第一部分 先修概念**（第 1~5 节）：异常、异常安全、移动语义、`noexcept` 的最小必要知识；
- **第二部分 实现指南**（第 6~11 节）：内部表示、每个函数的做法、关键难点的代码骨架、
  推荐实现顺序与常见错误。

> 建议配合源码阅读：接口定义在 `../include/my_string.h`，要求见 `../TASKS.md`，
> 构建与测试命令见 [`build-and-test.md`](build-and-test.md)。

---

# 第一部分 先修概念

## 1. 异常最小知识

**异常（exception）** 是函数无法完成工作时"通知调用者"的机制：用 `throw` 抛出，
调用者用 `try` / `catch` 捕获。没人捕获时，异常会一路上抛，最终程序调用
`std::terminate()` 终止（通常看到 `terminate called after throwing an instance of ...`）。

```cpp
#include <stdexcept>   // std::out_of_range

void f() {
    throw std::out_of_range("index out of range");   // 抛出
}

int main() {
    try {
        f();                                  // 可能抛异常的调用
    } catch (const std::out_of_range& e) {    // 捕获（推荐按 const 引用捕获）
        // e.what() 是抛出时传入的错误信息
    }
}
```

要点：

- 捕获请写 `catch (const T& e)`，不要按值捕获（会丢失派生类型信息）；
- `catch (...)` 可以捕获所有异常，但本作业用不到；
- 异常类型都定义在标准库头文件里：`std::out_of_range` 在 `<stdexcept>`，
  `std::bad_alloc` 在 `<new>`；
- **不要用异常做正常流程控制**，只在"确实无法完成"时使用。

### 本作业涉及哪些异常

| 异常 | 谁抛出 | 什么时候 |
| --- | --- | --- |
| `std::out_of_range` | **你**（在 `at()` / `insert()` 中 `throw`） | 下标越界、`insert` 位置非法 |
| `std::bad_alloc` | `new[]` 自动抛出 | 内存不足时，你不需要写 `throw`，但实现要在这种失败下保持正确 |
| —— | —— | `operator[]` 越界是未定义行为（**不抛异常**）；`operator>>` 读取失败用流的 `failbit`，**也不抛异常** |

### 本作业唯一需要你主动写的 throw

`at()` 与 `insert()` 越界时抛 `std::out_of_range`：

```cpp
#include <stdexcept>

char& String::at(std::size_t index) {
    if (index >= size_) {
        throw std::out_of_range("String::at: index out of range");
    }
    return data_[index];
}
```

随附测试用下面的方式验证（你不需要写这段，只要保证确实抛出即可）：

```cpp
try {
    (void)s.at(999);
} catch (const std::out_of_range&) {
    // 捕获到就说明实现正确
}
```

---

## 2. 异常安全（exception safety）

"异常安全"讨论的是：**一个操作抛异常时，对象和程序会处于什么状态**。常见三个级别：

| 级别 | 含义 |
| --- | --- |
| no-throw（不抛保证） | 操作保证不抛异常 |
| strong（强保证） | 要么成功，要么对象与操作前**完全一样**（提交/回滚语义） |
| basic（基本保证） | 允许状态改变，但不能泄漏内存、不能破坏对象不变量 |

本作业要求相关操作达到**强异常安全**，具体到代码就是一句话：

> `new[]` 失败抛出 `std::bad_alloc` 时，原字符串的内容必须保持原样，
> 不允许出现"旧缓冲区已经释放、新缓冲区又没分配成功"的悬空状态。

### 实现口诀：先分配成功，再释放旧的

```cpp
// 扩容（ensure_capacity / insert 的扩容路径）
char* fresh = new char[target + 1];   // 1. 先分配：失败会抛出，此时对象还没被改动
/* 拷贝旧数据、写结尾 '\0' */
delete[] data_;                       // 2. 成功之后才释放旧缓冲区
data_ = fresh;
capacity_ = target;
```

反例（**会出问题**）：

```cpp
delete[] data_;                       // 先释放旧缓冲区
data_ = new char[target + 1];         // 如果这里抛出 bad_alloc……
                                      // data_ 指向已释放内存 → 悬空指针、double free
```

其它两条常用规则：

- **复制赋值用 copy-and-swap**：先拷贝构造一个临时对象（可能抛出，此时 `*this` 未变），
  成功后再 `swap`；
- **自赋值、自插入、自交换**：保证源数据在被覆盖之前不会被销毁。

> 随附的 `m5_strong_safety` 会替换全局 `new[]`，在指定的分配次数上抛
> `std::bad_alloc`，真实检查这一点（前提是你用 `new char[]` 分配）；
> 把上面两条规则写反，很容易造成真实的内存泄漏或悬空指针，**ASan 也会直接报出来**。

---

## 3. 移动语义速览

### 为什么需要移动

拷贝是 O(n) 的深拷贝。而像 `String("hello")`、函数返回值这类**临时对象**马上就销毁了，
如果可以"偷走"它的缓冲区指针，就能把 O(n) 降到 O(1)。

```cpp
String a("hello");
String b(std::move(a));   // 移动构造：b 接管 a 的缓冲区
                          // 之后 a 处于"被移动后"状态，不要再依赖其内容
```

- `std::move(x)` 本身**不移动任何东西**，它只是把 `x` 转成右值引用，表示"允许被偷"；
  需要 `#include <utility>`；
- `String&&`（右值引用）只能绑定到临时对象或 `std::move` 的结果。

### 被移动后（moved-from）的对象

本作业的约定是"**有效但内容未指定**"：

- 可以安全析构、可以重新赋值（随附测试就是这么用的）；
- **不要**读它的内容或 `size()`；
- 析构函数要能处理"缓冲区指针为空"的情况（`delete[] nullptr` 是合法的空操作）；
- 源对象的指针要被置空（`nullptr`），否则它析构时会把你刚偷走的缓冲区释放掉。

### 自移动（`s = std::move(s)`）

移动赋值的第一步通常是"释放自己原来的缓冲区"。如果 `this == &other`，
就会先把自己的缓冲区删掉，再去偷……已经没有的东西。所以必须先判断：

```cpp
String& String::operator=(String&& other) noexcept {
    if (this == &other) {
        return *this;          // 自移动：直接返回
    }
    delete[] data_;            // 释放自己的旧缓冲区
    data_ = other.data_;       // 接管 other 的缓冲区
    /* 交换/复制 size_、capacity_ 并把 other 置空 */
    return *this;
}
```

---

## 4. `noexcept` 是什么

`noexcept` 写在函数声明后面，是**函数签名的一部分**（C++17 起更是函数类型的一部分）：

```cpp
String(String&& other) noexcept;              // 声明
String& operator=(String&& other) noexcept;
void swap(String& other) noexcept;
```

它表示"承诺这个函数不会让异常逃出去"。注意：

- 它**不是**"函数里不能出现 throw"，而是"如果异常真的逃出去，程序会直接调用
  `std::terminate()` 终止"——比不写更严格；
- 本作业里给 `noexcept` 的函数都是不会分配内存、不会抛异常的操作：
  移动构造 / 移动赋值、`operator[]`、`size()` / `capacity()`、`c_str()`、
  `const char*` 转换、`swap()`；
- `at()` / `insert()` **不是** `noexcept`，因为它们需要抛 `std::out_of_range`；
- 为什么移动操作要 `noexcept`：标准库容器在扩容时据此决定"移动还是拷贝"
  （`std::move_if_noexcept`）。本作业的移动只是窃取指针，天然不会抛。

### 新手最容易踩的坑：定义里漏写 `noexcept`

如果声明里有 `noexcept`，`.cpp` 里的定义必须**原样保留**，否则编译报错：

```cpp
// 声明（include/my_string.h）
String(String&& other) noexcept;

// 定义（src/my_string.cpp）—— 必须也写 noexcept
String::String(String&& other) noexcept { /* ... */ }
```

漏写时的报错（GCC）：

```text
error: declaration of 'String::String(String&&)' has a different exception specifier
note: from previous declaration 'String(String&&) noexcept'
```

看到这句话，补上 `noexcept` 即可。其它带 `noexcept` 的函数（`operator[]`、
`size`、`capacity`、`c_str`、转换、`swap`）同理，定义都要和声明保持一致。

---

## 5. 常见困惑

**Q：`at()` 和 `operator[]` 有什么区别？**
`operator[]` 与 `std::string` 一致，不做检查，但 `index == size()` 是合法的：
返回结尾 `'\0'` 的引用（`s[size()] == '\0'`，可以直接读）；`index > size()` 才是
未定义行为，另外不要写 `s[size()]`。`at()` 做检查，`index >= size()`
抛 `std::out_of_range`——这就是为什么 `at()` 不能是 `noexcept`。

**Q：我能在 `noexcept` 函数里 `throw` 吗？**
能编译，但异常逃出去程序会立即 `terminate`，等于制造崩溃。移动构造 / 移动赋值里
不要做会分配内存的事（本作业的移动只是窃取指针，所以没问题）。

**Q：被移动后的对象能调用 `c_str()` 吗？**
按本作业的接口约定要返回一个有效的 C 字符串；但它的**内容未指定**，
不要拿它去比较或计算长度。

**Q：为什么 `insert(pos > size())` 要抛异常而不是返回错误码？**
因为接口与 `std::string` 保持一致；异常能保证"要么成功、要么什么都没发生"，
调用者不容易忽略错误。这是本作业唯一需要你主动 `throw` 的地方。

**Q：如果我不小心先 `delete[]` 再 `new`，但测试全过了，算对吗？**
不算。测试不会制造 `bad_alloc`，所以这种做法在测试里可能看不出来，但它违反了
强异常安全要求；写 ASan 检查 + 代码审查时会被发现。

---

# 第二部分 实现指南

## 6. 先想清楚内部表示

动手写代码前，先把 `include/my_string.h` 的 `private:` 部分补上。推荐三个成员：

```cpp
private:
    char* data_;            // 指向 new char[capacity_ + 1] 的缓冲区
    std::size_t size_;      // 当前长度（不含结尾 '\0'）
    std::size_t capacity_;  // 当前容量（不含结尾 '\0'）
```

你必须始终维持三条**不变量**：

1. `data_[size_] == '\0'`（缓冲区始终以 `'\0'` 结尾，空串也一样）；
2. `size_ <= capacity_`；
3. 未处于"被移动后"状态时，`data_` 指向一块 `capacity_ + 1` 字节的 `new char[]`。

被移动后的对象：`data_ = nullptr`、`size_ = capacity_ = 0`；
`c_str()` 对它返回 `""`，析构时 `delete[] nullptr` 安全。

### 建议的工具函数（放匿名 namespace，不对外暴露）

不要用 `<cstring>`，自己写循环；这些函数都放在 `.cpp` 的
`namespace { ... }` 里，`noexcept` 只写声明处：

```cpp
namespace {
constexpr std::size_t kMinCapacity = 16;  // 容量契约：默认构造的空串容量 >= 16

std::size_t raw_length(const char* s) noexcept { /* 数到 '\0' 为止 */ }

void raw_copy(char* dst, const char* src, std::size_t count) noexcept {
    /* 逐字节正向复制（memcpy 语义） */
}

// 与 memmove 语义相同：允许 src、dst 指向同一缓冲区的重叠区间
void raw_move(char* dst, const char* src, std::size_t count) noexcept {
    /* 若 dst < src：从前往后复制；否则从后往前复制 */
}

std::size_t growth_target(std::size_t current_capacity, std::size_t needed) noexcept {
    // 增长策略：max(所需长度, 当前容量 x 2, kMinCapacity)
    // 这样连续 push_back 的摊销复杂度是 O(1)
}
}  // namespace
```

---

## 7. 最基础的几个函数

### 7.1 默认构造与析构

```cpp
String::String()
    : data_(new char[kMinCapacity + 1]), size_(0), capacity_(kMinCapacity) {
    data_[0] = '\0';
}

String::~String() {
    delete[] data_;   // delete[] nullptr 合法，被移动后的对象也能正常析构
}
```

### 7.2 由 C 字符串构造

要点（不用写出完整代码也可以照着做）：

1. `str == nullptr` 视为空串（长度为 0），**不能解引用空指针**；
2. 先算出长度 `len`，容量取 `max(len, kMinCapacity)`；
3. **先 `new` 成功再写成员**：如果 `new` 抛出，对象应当仍然是"空指针 + 0"的可析构状态；
4. 复制内容后写 `data_[len] = '\0'`。

### 7.3 只读接口与带检查访问

```cpp
std::size_t String::size() const noexcept { return size_; }
std::size_t String::capacity() const noexcept { return capacity_; }

char& String::operator[](std::size_t index) noexcept { return data_[index]; }
const char& String::operator[](std::size_t index) const noexcept { return data_[index]; }

const char* String::c_str() const noexcept {
    return data_ != nullptr ? data_ : "";   // 被移动后的对象也要返回有效字符串
}
String::operator const char*() const noexcept { return c_str(); }

char& String::at(std::size_t index) {                 // 注意：不是 noexcept
    if (index >= size_) {
        throw std::out_of_range("String::at: index out of range");
    }
    return data_[index];
}
// const 版本同理，返回 const char&
```

---

## 8. 拷贝与移动（Rule of Five）

### 8.1 拷贝构造

```cpp
String::String(const String& other) : data_(nullptr), size_(0), capacity_(0) {
    // 1. other 可能是"被移动后"的对象（data_ == nullptr）：按空串处理
    // 2. 分配 max(other.size_, kMinCapacity) + 1 字节（先分配，失败时本对象仍可析构）
    // 3. 逐字节复制 other 的内容，并在末尾写 '\0'
    // 4. 最后才写回 data_ / size_ / capacity_
}
```

关键是**深拷贝**：新对象有自己的缓冲区，`b.c_str() != a.c_str()`。

### 8.2 复制赋值：copy-and-swap

```cpp
String& String::operator=(const String& other) {
    if (this == &other) {          // 自赋值：直接返回
        return *this;
    }
    String tmp(other);             // 先深拷贝（这里抛出的话，*this 完全没变）
    swap(tmp);                     // 接管副本；tmp 析构时释放原来的缓冲区
    return *this;
}
```

### 8.3 移动构造

```cpp
String::String(String&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    other.data_ = nullptr;         // 源对象进入"有效但内容未指定"状态
    other.size_ = 0;
    other.capacity_ = 0;
}
```

### 8.4 移动赋值

```cpp
String& String::operator=(String&& other) noexcept {
    if (this == &other) {          // 自移动：不判断会删掉自己的缓冲区
        return *this;
    }
    delete[] data_;                // 释放自己原来的缓冲区
    data_ = other.data_;           // 接管
    size_ = other.size_;
    capacity_ = other.capacity_;
    other.data_ = nullptr;         // 源对象置空，否则它会释放你刚接管的缓冲区
    other.size_ = 0;
    other.capacity_ = 0;
    return *this;
}
```

---

## 9. 扩容与 insert（本作业的难点）

### 9.1 `ensure_capacity`：增长策略与强异常安全

```cpp
void String::ensure_capacity(std::size_t needed) {
    if (needed <= capacity_) {
        return;                                          // 容量够，什么都不做
    }
    const std::size_t target = growth_target(capacity_, needed);
    char* fresh = new char[target + 1];                  // 1. 先分配
    /* 2. 把旧内容与结尾 '\0' 拷进 fresh */
    delete[] data_;                                      // 3. 成功后才释放旧的
    data_ = fresh;
    capacity_ = target;
}
```

注意：`ensure_capacity` 只负责容量，**不改变 `size_`**。

### 9.2 `operator+`

```cpp
String String::operator+(const String& other) const {
    String result;                                   // 空串
    result.ensure_capacity(size_ + other.size_);     // 一次分配到位
    // 依次把 this 与 other 的内容拷进 result，写结尾 '\0'，设置 result.size_
    return result;                                   // 返回值优化 + 移动语义，不会多拷贝
}
```

### 9.3 `insert`：三条路径

`insert` 要同时处理"容量够 / 容量不够"和"普通插入 / 自插入"，建议按下面的框架写：

```cpp
void String::insert(std::size_t pos, const String& str) {
    if (pos > size_) {
        throw std::out_of_range("String::insert: pos out of range");   // 先检查，后修改
    }
    const std::size_t added = str.size_;
    if (added == 0) {
        return;                                        // 插入空串（包括自插入空串）
    }

    if (size_ + added > capacity_) {
        // 情况 A：容量不足 —— 在新缓冲区里一次性组装，成功后（最后一步）才释放旧缓冲区
        //
        //    旧缓冲区: [0, pos) | [pos, size_)   (末尾还有 '\0')
        //    新缓冲区: [0, pos) | [str]   | [pos, size_) | '\0'
        //
        //    自插入（str 就是 *this）也安全：此时 str.data_ 仍指向旧缓冲区，
        //    只要把 delete[] data_ 放在最后，读取旧数据就没问题。
    } else {
        // 情况 B：容量足够 —— 原地腾位置
        //
        //    1. 把 [pos, size_] 整体后移 added 字节（务必包含结尾 '\0'）
        //       必须使用"允许重叠"的移动（memmove 语义）：
        //       目标在源右侧，要从后往前复制，否则会覆盖还没搬走的数据。
        //    2. 再把 str 的内容写入 [pos, pos + added)
        //
        //    自插入时：第 1 步的目标起点 pos + added >= added，
        //    不会覆盖源区间 [0, added)，所以第 2 步仍能读到完整数据。
    }
    size_ += added;                                    // 最后更新长度
}
```

写完后请特别验证：`s.insert(0, s)`、`s.insert(s.size(), s)`、
`repeat('x', s.capacity())` 之后再做 `insert`（恰好触发/不触发扩容的边界）。

### 9.4 `push_back`、`swap` 与类型转换

```cpp
void String::push_back(char ch) {
    ensure_capacity(size_ + 1);   // 可能抛出；抛出时对象保持不变
    data_[size_] = ch;
    ++size_;
    data_[size_] = '\0';
}

void String::swap(String& other) noexcept {
    // 交换 data_、size_、capacity_ 三个成员即可；
    // 自交换（swap(*this)）天然安全，不需要特判
}
```

`c_str()` / `operator[]` / `at()` 见 7.3 节——它们不涉及分配，写起来最简单，
适合最先完成。

---

## 10. Bonus（选做）：流运算符 `<<` / `>>`

### 10.1 输出流

```cpp
std::ostream& operator<<(std::ostream& os, const String& str) {
    if (str.data_ != nullptr && str.size_ > 0) {
        os.write(str.data_, static_cast<std::streamsize>(str.size_));
    }
    return os;
}
```

按 `size()` 写出，而不是 `os << str.data_`：这样内嵌 `'\0'` 也能正确输出，
并且被移动后的对象也不会解引用空指针。

### 10.2 输入流

要点：

1. 用 `std::istream::sentry` 跳过前导空白并检查流状态；
2. **先读进一个临时 `String`**，全部成功后再提交给 `str`（读取失败时原值不变）；
3. 读到 `eof` 停止；读到空白时用 `is.unget()` 把空白"退回去"，本次结果结束；
4. 一个字符都没读到 → `is.setstate(std::ios_base::failbit)`。

```cpp
std::istream& operator>>(std::istream& is, String& str) {
    std::istream::sentry sentry(is);            // 跳过空白、检查流状态
    if (!sentry) {
        return is;
    }

    String temp;                                // 先读到临时对象
    bool got = false;
    for (;;) {
        const std::istream::int_type next = is.get();
        if (next == std::istream::traits_type::eof()) {
            break;                              // 文件/输入结束
        }
        const char ch = static_cast<char>(next);
        if (/* ch 是空白字符：' ', '\t', '\n', '\v', '\f', '\r' */) {
            is.unget();                         // 空白不属于结果，退还给下一次读取
            break;
        }
        temp.push_back(ch);
        got = true;
    }

    if (got) {
        str = std::move(temp);                  // 提交
    } else {
        is.setstate(std::ios_base::failbit);    // 没读到任何字符；str 保持原值
    }
    return is;
}
```

编写后可用 `-DENABLE_BONUS_TESTS=ON` 构建 `bonus_stream` 验证（见
[`build-and-test.md`](build-and-test.md) 与 `../TASKS.md` 第 6 节）。

---

## 11. 推荐实现顺序与常见错误

### 11.1 按里程碑的顺序写，每步都能看到测试变绿

测试按里程碑拆成 4 个独立程序（见 `../TASKS.md` 第 1.1 节），
推荐按同样的顺序实现——每个里程碑变绿后再往下做：

1. **M1（必做）**：补私有成员 + 工具函数（第 6 节）→ 默认构造、`const char*` 构造、
   析构、`size`/`capacity`/`operator[]`/`c_str`（第 7 节）→ `at()` 越界抛异常（7.3）
   → `push_back`（9.4）→ `./build/m1_basics` 变绿；
2. **M2（必做）**：拷贝构造、复制赋值（8.1、8.2）→ `operator+`、`insert`
   （9.1~9.3）→ `./build/m2_value_semantics` 变绿；
3. **M5（进阶）**：强异常安全——用注入的 `bad_alloc` 验证“先分配成功、再释放旧的”
   （2 节、9.1）→ `./build/m5_strong_safety` 变绿（它只依赖 M1 + M2，可以现在做）；
4. **M3（进阶）**：移动构造、移动赋值（8.3、8.4）→ `./build/m3_move` 变绿；
5. **M4（进阶）**：自插入、自交换、容量边界——重叠复制最容易在这一步出错
   （9.3）→ `./build/m4_edge_cases` 变绿；
6. （选做）流运算符（第 10 节）：开启 `-DENABLE_BONUS_TESTS=ON` 后
   `./build/bonus_stream` 变绿。

每完成一步：

```bash
cmake --build build -j                                   # 构建全部
ctest --test-dir build -R m1_basics --output-on-failure  # 只跑一个里程碑
ctest --test-dir build --output-on-failure               # 跑全部
```

### 11.2 常见错误对照表

| 症状 | 原因 |
| --- | --- |
| `c_str()` 后面读出乱码、ASan 报 `heap-buffer-overflow` | 忘了写结尾 `'\0'`，或容量算错（`capacity` 不含 `'\0'`） |
| ASan 报 `LeakSanitizer: detected memory leaks` | 扩容/赋值途中覆盖了旧指针，漏了 `delete[]` |
| ASan 报 `attempting double-free` 或 `heap-use-after-free` | 移动后源对象没置空；或自赋值/自移动没有特判 |
| 恰好写满 `capacity()` 个字符时多扩容 | `capacity()` 语义搞混（含/不含 `'\0'`） |
| `insert` 自插入后内容错乱 | 原地路径用了不支持重叠的复制方向；或先把 `str` 的数据破坏了 |
| 编译报 `has a different exception specifier` | 定义里漏写 `noexcept`（第 4 节） |
| 编译报 `unused parameter` 之类的警告 | 有函数还没实现完；提交前应清零 |

### 11.3 自检清单

- [ ] `ctest` 五个里程碑全过（m1 242 / m2 79 / m3 20 / m4 76 / m5 56 项），且构建无警告；
- [ ] ASan/UBSan 构建同样全过（默认开启，见 `build-and-test.md`）；
- [ ] 自己额外验证过：空串、长串、多次扩容、自赋值、自移动、自插入、自交换；
- [ ] `insert` 的"容量恰好够 / 恰好差 1"两个边界都试过；
- [ ] （选做）`-DENABLE_BONUS_TESTS=ON` 后流测试 11 项也全过。
