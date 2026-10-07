#include "my_string.h"

#include <cstring>
#include <cctype>
#include <stdexcept>
#include <algorithm>
#include <ostream>
#include <istream>
#include <utility>

// ==================== 私有辅助函数 ====================
std::size_t String::next_capacity(std::size_t current, std::size_t needed) {
    std::size_t cap = (current < 16) ? 16 : current;
    while (cap < needed) {
        if (cap > (static_cast<std::size_t>(-1) / 2)) {
            cap = needed;
            break;
        }
        cap *= 2;
    }
    return cap;
}

void String::reserve(std::size_t new_capacity) {
    if (new_capacity <= capacity_) return;

    char* new_data = new char[new_capacity + 1];
    if (data_ != nullptr) {
        std::memcpy(new_data, data_, size_ + 1); // 包括结尾 '\0'
    } else {
        new_data[0] = '\0';
    }
    delete[] data_;
    data_ = new_data;
    capacity_ = new_capacity;
}

// ==================== 构造 / 析构 ====================
String::String() : data_(new char[17]), size_(0), capacity_(16) {
    data_[0] = '\0';
}

String::String(const char* str) {
    if (str == nullptr) {
        size_ = 0;
        capacity_ = 16;
        data_ = new char[capacity_ + 1];
        data_[0] = '\0';
    } else {
        size_ = std::strlen(str);
        capacity_ = std::max<std::size_t>(16, size_);
        data_ = new char[capacity_ + 1];
        std::memcpy(data_, str, size_ + 1);
    }
}

String::String(const String& other)
    : size_(other.size_), capacity_(other.capacity_) {
    data_ = new char[capacity_ + 1];
    std::memcpy(data_, other.data_, size_ + 1);
}

String::String(String&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

String::~String() {
    delete[] data_;
}

// ==================== 赋值 ====================
String& String::operator=(const String& other) {
    if (this != &other) {
        String temp(other);
        swap(temp);
    }
    return *this;
}

String& String::operator=(String&& other) noexcept {
    if (this != &other) {
        delete[] data_;
        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }
    return *this;
}

// ==================== 拼接 ====================
String String::operator+(const String& other) const {
    String result;
    result.reserve(size_ + other.size_);
    std::memcpy(result.data_, data_, size_);
    std::memcpy(result.data_ + size_, other.data_, other.size_ + 1);
    result.size_ = size_ + other.size_;
    return result;
}

// ==================== 下标访问 ====================
char& String::operator[](std::size_t index) noexcept {
    return data_[index];
}

const char& String::operator[](std::size_t index) const noexcept {
    return data_[index];
}

char& String::at(std::size_t index) {
    if (index >= size_) {
        throw std::out_of_range("String::at: index out of range");
    }
    return data_[index];
}

const char& String::at(std::size_t index) const {
    if (index >= size_) {
        throw std::out_of_range("String::at: index out of range");
    }
    return data_[index];
}

// ==================== 长度 / 容量 ====================
std::size_t String::size() const noexcept { return size_; }
std::size_t String::capacity() const noexcept { return capacity_; }

// ==================== 插入 / 追加 ====================
void String::insert(std::size_t pos, const String& str) {
    if (pos > size_) {
        throw std::out_of_range("String::insert: position out of range");
    }
    if (str.size_ == 0) return;

    std::size_t new_size = size_ + str.size_;
    String temp;
    temp.reserve(std::max(new_size, capacity_));

    std::memcpy(temp.data_, data_, pos);                           // 前半
    std::memcpy(temp.data_ + pos, str.data_, str.size_);           // 插入内容
    std::memcpy(temp.data_ + pos + str.size_, data_ + pos,
                size_ - pos + 1);                                  // 后半含 '\0'
    temp.size_ = new_size;

    swap(temp);
}

void String::push_back(char ch) {
    if (size_ + 1 > capacity_) {
        reserve(next_capacity(capacity_, size_ + 1));
    }
    data_[size_] = ch;
    ++size_;
    data_[size_] = '\0';
}

// ==================== C 字符串转换 ====================
const char* String::c_str() const noexcept {
    return data_ ? data_ : "";
}

String::operator const char*() const noexcept {
    return c_str();
}

// ==================== 交换 ====================
void String::swap(String& other) noexcept {
    std::swap(data_, other.data_);
    std::swap(size_, other.size_);
    std::swap(capacity_, other.capacity_);
}

// ==================== 流操作 ====================
std::ostream& operator<<(std::ostream& os, const String& str) {
    os << str.c_str();
    return os;
}

std::istream& operator>>(std::istream& is, String& str) {
    str = String();

    char ch;
    while (is.get(ch)) {
        if (!std::isspace(static_cast<unsigned char>(ch))) break;
    }
    if (!is) return is;

    do {
        str.push_back(ch);
    } while (is.get(ch) && !std::isspace(static_cast<unsigned char>(ch)));

    if (is) is.unget();
    return is;
}