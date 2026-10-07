#ifndef ASSIGNMENT2_MY_STRING_H
#define ASSIGNMENT2_MY_STRING_H

#include <cstddef>
#include <iosfwd>

class String {
public:
    // ---- 构造 / 析构（Rule of Five）----
    String();                                     // 空字符串，初始容量至少 16
    String(const char* str);                      // 由 C 字符串构造；nullptr 视为空串
    String(const String& other);                  // 拷贝构造（深拷贝）
    String(String&& other) noexcept;              // 移动构造（窃取缓冲区）
    ~String();

    // ---- 赋值 ----
    String& operator=(const String& other);       // 复制赋值（强异常安全）
    String& operator=(String&& other) noexcept;   // 移动赋值（自移动安全）

    // ---- 拼接 ----
    String operator+(const String& other) const;

    // ---- 下标访问：与 std::string::operator[] 一样不做边界检查 ----
    char& operator[](std::size_t index) noexcept;
    const char& operator[](std::size_t index) const noexcept;

    // ---- 带边界检查的访问：越界抛出 std::out_of_range ----
    char& at(std::size_t index);
    const char& at(std::size_t index) const;

    // ---- 长度 / 容量 ----
    std::size_t size() const noexcept;
    std::size_t capacity() const noexcept;        // 不含结尾 '\0'

    // ---- 插入 / 追加 ----
    void insert(std::size_t pos, const String& str);
    void push_back(char ch);

    // ---- 转换为 C 字符串 ----
    const char* c_str() const noexcept;
    operator const char*() const noexcept;

    // ---- 交换全部内容（自交换也必须安全）----
    void swap(String& other) noexcept;

    // ---- 流操作 ----
    friend std::ostream& operator<<(std::ostream& os, const String& str);
    friend std::istream& operator>>(std::istream& is, String& str);

private:
    char* data_;                // 缓冲区，始终以 '\0' 结尾
    std::size_t size_;          // 当前长度（不含 '\0'）
    std::size_t capacity_;      // 容量（不含 '\0'），实际缓冲区大小 = capacity_ + 1

    void reserve(std::size_t new_capacity);
    static std::size_t next_capacity(std::size_t current, std::size_t needed);
};

#endif  // ASSIGNMENT2_MY_STRING_H