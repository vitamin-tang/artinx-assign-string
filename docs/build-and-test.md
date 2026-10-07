# 构建、测试与内存检查指南

本文档面向学生，适用于 **Linux（GCC / Clang）** 与 **macOS（AppleClang / Homebrew Clang）**。
所有命令均在仓库根目录（`assignment2-string/`）下执行。

---

## 0. 命令一览

```bash
# 默认构建（已开启 ASan + UBSan）+ 测试
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure

# 不带 sanitizer 的普通构建（需要时）
cmake -S . -B build-plain -DENABLE_SANITIZERS=OFF
cmake --build build-plain -j
ctest --test-dir build-plain --output-on-failure

# 只跑一个里程碑（定位更快）
ctest --test-dir build -R m1_basics --output-on-failure

# 便捷目标：只构建全部测试 / 构建并运行全部测试
cmake --build build --target all_tests
cmake --build build --target check
```

---

## 1. 环境准备

### Linux

```bash
# 检查工具链
g++ --version          # 建议 GCC ≥ 9；或 clang++ --version，建议 Clang ≥ 10
cmake --version        # 需要 ≥ 3.14

# Debian / Ubuntu
sudo apt install build-essential cmake gdb

# Fedora / RHEL
sudo dnf install gcc-c++ cmake gdb
```

### macOS

```bash
xcode-select --install      # 安装 AppleClang 与命令行工具
brew install cmake          # 或从 cmake.org 下载安装包
clang++ --version
cmake --version
```

CMake 会自动选择系统默认编译器；想换编译器时在配置阶段指定
`-DCMAKE_CXX_COMPILER=clang++`（或 `g++`）即可。

> 对异常、`noexcept` 或移动语义不熟悉时，先看
> [`guide.md`](guide.md)，里面解释了本作业会用到的全部相关语法，
> 并给出关键函数的实现骨架。
>
> 用 VS Code 的同学：仓库已预置 clangd + CodeLLDB 配置（补全、跳转、断点调试），
> 说明见 [`vscode.md`](vscode.md)。

---

## 2. 构建与测试

### 2.1 配置与编译

```bash
cmake -S . -B build
cmake --build build -j
```

- `-S .` 指定源码目录，`-B build` 指定构建目录（生成物都放在 `build/`，不会污染源码）；
- **默认就开启 ASan + UBSan**（详见第 3 节）：配置阶段会探测编译器支持，
  支持则加上 sanitizer 标志；不支持则直接报错，可加 `-DENABLE_SANITIZERS=OFF` 关闭；
- 工程已统一开启 `-Wall -Wextra -Wpedantic`，完成前应保证没有任何警告；
- 工程已开启 `CMAKE_EXPORT_COMPILE_COMMANDS`，配置后会在 `build/` 下生成
  `compile_commands.json`（clangd / VS Code 补全与跳转依赖它）；
- 未指定 `CMAKE_BUILD_TYPE` 时默认 `Debug`（保留调试信息，便于 ASan 报告定位到行号）；
- 若你的实现拆成了多个 `.cpp`，把它们加入 `CMakeLists.txt` 里的 `STRING_SOURCES` 列表。

### 2.2 首次构建的“预期失败”与里程碑推进

拿到仓库时 `src/my_string.cpp` 是空的，因此链接各个里程碑测试时会出现类似报错：

```text
undefined reference to `String::String()'
undefined reference to `String::size() const'
...
collect2: error: ld returned 1 exit status
```

这**不是环境问题**，而是提醒你还有函数没有实现。四个测试程序相互独立：

- `m1_basics` 的链接错误就是 **M1 的待实现清单**；把 M1 实现完，`m1_basics`
  就能链接并通过；
- 然后看 `m2_value_semantics` 缺哪些函数，依此类推（M3 → M4）；
- `m5_strong_safety` 只依赖 M1 + M2，想提前验证强异常安全的话可以在 M2 之后就跑它。

每次改完代码，重新执行 `cmake --build build -j` 即可；也可以只构建某一个目标，
例如 `cmake --build build --target m1_basics`。

### 2.3 运行测试

```bash
# 方式一：通过 CTest（推荐，等价于验收命令）
ctest --test-dir build --output-on-failure

# 只跑一个里程碑
ctest --test-dir build -R m1_basics --output-on-failure

# 方式二：直接运行某个测试程序
./build/m1_basics
```

测试程序逐条输出用例结果（进度与失败信息在 stderr，最终汇总在 stdout，
CTest 会一并显示），形如：

```text
[ RUN      ] default_constructed_empty
[       OK ] default_constructed_empty
...
[ RUN      ] at
tests/m1_basics.cpp:88: CHECK failed: cs.at(2) throws std::out_of_range
[   FAILED ] at

checks: 242, failures: 1
TESTS FAILED
```

- 失败信息给出 **文件:行号、失败的表达式、实际值与期望值**，直接定位到出错语义；
- 退出码：全部通过为 `0`，有失败为 `1`（CTest 依赖退出码判定成败）；
- 在 CMake < 3.20 上 `ctest --test-dir` 会报 `unrecognized option`，改用：

```bash
cd build && ctest --output-on-failure
```

### 2.4 清理与完全重建

改了 `CMakeLists.txt` 或出现奇怪的缓存问题时：

```bash
rm -rf build && cmake -S . -B build && cmake --build build -j
```

### 2.5 只跑某一个里程碑

测试已按里程碑拆成独立程序，一般不需要自己裁剪测试代码：

```bash
ctest --test-dir build -R m3_move --output-on-failure   # 正则匹配测试名
./build/m3_move                                          # 或直接运行
```

想单步看某一条用例，可以用调试器（见 [`vscode.md`](vscode.md)）；
想写自己的实验代码，请**新建文件**，不要修改 `tests/` 下的文件。

### 2.6 选做（bonus）：流运算符测试

流运算符 `<<` / `>>` 是选做内容，测试单独放在 `tests/stream_tests.cpp`，
**默认不构建也不注册**（所以没做 bonus 时，上面的基线测试依然全绿）：

```bash
cmake -S . -B build -DENABLE_BONUS_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

开启后会多出一个可执行文件 `bonus_stream`，CTest 会同时运行 m1~m5 与
bonus_stream。命令上也已写在 `../TASKS.md` 第 6 节；实现思路见
[`guide.md`](guide.md) 第 10 节。

### 2.7 强异常安全测试（m5_strong_safety）

`m5_strong_safety` 验证 TASKS.md 4.1 的强异常安全要求。它在测试程序里替换了
全局 `operator new[]` / `delete[]`，在指定的第 k 次数组分配时抛出 `std::bad_alloc`，
然后检查：

- 操作要么完整成功，要么对象的内容 / 长度 / 容量完全不变
  （`push_back`、`insert`、复制赋值、`operator+`、构造失败路径）；
- 失败路径不泄漏内存（用分配/释放计数平衡判断，ASan 的 LeakSanitizer 也会兜底）；
- 失败之后对象仍然可以正常使用。

它只替换 `new[]`，不影响 `iostream` 等内部使用的标量 `new`；在 ASan/UBSan 构建与
普通构建下都能运行。**前提是实现用 `new char[]` 分配缓冲区**：若改用
`malloc` / `std::allocator`，本测试注入不到（作业要求用 `new[]` / `delete[]`，
见 `../TASKS.md` 4.4）。

```bash
ctest --test-dir build -R m5_strong_safety --output-on-failure
```

---

## 3. AddressSanitizer + UndefinedBehaviorSanitizer

### 3.1 默认开启与关闭方法

**ASan + UBSan 默认开启**，所以第 2 节的 `build/` 就是内存检查版本：

```bash
cmake -S . -B build              # 默认即带 ASan+UBSan
cmake --build build -j
ctest --test-dir build --output-on-failure
```

显式写出开关（与默认行为等价，便于在命令里看清意图）或使用独立目录：

```bash
cmake -S . -B build-asan -DENABLE_SANITIZERS=ON
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

确实需要不带 sanitizer 的普通构建时：

```bash
cmake -S . -B build-plain -DENABLE_SANITIZERS=OFF
cmake --build build-plain -j
ctest --test-dir build-plain --output-on-failure
```

开启后工程会为**编译与链接**都加上：

| 标志 | 作用 |
| --- | --- |
| `-fsanitize=address` | 检查越界读写、use-after-free、double free、内存泄漏等 |
| `-fsanitize=undefined` | 检查有符号溢出、空指针解引用、越界移位、错误的类型转换等 UB |
| `-fno-omit-frame-pointer` | 保留帧指针，让报告能打印完整调用栈 |
| `-fno-sanitize-recover=all` | UB 一旦发生立即终止进程，而不是“打印后继续” |

配置阶段会先用 `check_cxx_source_compiles` 做一次**真实的编译 + 链接**探测（只检查
编译标志会因链接时缺少 `-fsanitize` 而误判，因此不能简单地用
`check_cxx_compiler_flag`）：

- 编译器支持 → 正常生成；
- 编译器不支持（或使用 MSVC）→ CMake **直接报错**并提示改用 GCC/Clang 或加
  `-DENABLE_SANITIZERS=OFF`，不会静默退化成普通构建，避免“以为开了 sanitizer
  其实没开”。

### 3.2 手动指定标志（备用方案）

不想用（或改不了）上面的选项时，可以绕过 CMake 选项手动指定（记得同时用
`-DENABLE_SANITIZERS=OFF` 关掉默认的探测与报错）：

```bash
cmake -S . -B build-asan -DENABLE_SANITIZERS=OFF \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

如果工具链对 UBSan 支持不完整，可以退化为只开 ASan：

```bash
cmake -S . -B build-asan -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer"
```

### 3.3 运行与报告解读

**内存泄漏（Linux）**：GCC/Clang 的 ASan 在 Linux 上默认开启 LeakSanitizer；
也可显式打开：

```bash
ASAN_OPTIONS=detect_leaks=1 ./build-asan/m1_basics
```

出现泄漏时报告形如：

```text
==12345==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 17 byte(s) in 1 object(s) allocated from:
    #0 0x... in operator new[](unsigned long)
    #1 0x... in String::String(char const*) src/my_string.cpp:57
```

含义：某次 `new[]` 分配的内存没有任何 `delete[]` 释放。常见原因：

- 析构函数漏写 `delete[]`；
- 扩容时直接覆盖了旧指针，未先释放旧缓冲区；
- 复制赋值走“先释放、再分配”，中途原指针丢失；
- 移动后源对象未置空，其析构函数又释放了一次（通常表现为 double free 或 UAF）。

**常见 ASan 报告关键词**：

| 关键词 | 含义 |
| --- | --- |
| `heap-buffer-overflow` | 读写了缓冲区边界之外（常见于手写循环的 `<=` / 差一错误） |
| `heap-use-after-free` | 使用了已释放的内存（常见于自赋值/自插入/移动后继续使用） |
| `attempting double-free` | 同一块内存释放两次（常与移动后未置空有关） |
| `LeakSanitizer: detected memory leaks` | 有分配未释放 |

**UBSan 报告**形如 `runtime error: signed integer overflow`、
`runtime error: null pointer passed as argument` 等；由于我们加了
`-fno-sanitize-recover=all`，出现后会立即中止，先修复第一条报错再看后续。

**macOS 说明**：AppleClang 的 ASan/UBSan 可用于检测越界、UAF、double free 与 UB，
但 **LeakSanitizer 在 macOS 上支持有限**，ASan 通常不会报告内存泄漏。
因此 macOS 同学请把重点放在“无 ASan/UBSan 报错”上；如确实需要查泄漏，
可以用 Xcode 的 Instruments（Leaks）工具，或到 Linux/WSL2 下跑一遍上面的泄漏检测。

**MSVC（仅作参考）**：本课程不要求 Windows 原生环境，建议使用 WSL2。
若必须在 MSVC 下检查，只能用 `/fsanitize=address`（没有 UBSan），
把 `-DCMAKE_CXX_FLAGS="/fsanitize=address"` 传给 CMake 即可，此时不要使用
`-DENABLE_SANITIZERS=ON`（该选项只支持 GCC/Clang）。

---

## 4. 常见问题（FAQ）

**Q1. 构建一直报 `undefined reference to 'String::...'`？**
说明对应函数还没实现（或实现文件没有加进 `STRING_SOURCES`）。
这是起始框架的正常状态，逐个把 `include/my_string.h` 里的声明实现完即可。

**Q2. `ctest --test-dir` 报 `unrecognized option`？**
CMake 版本低于 3.20，改用 `cd build && ctest --output-on-failure`。

**Q3. 配置时报错说编译器不支持 `-fsanitize=address,undefined`？**
ASan/UBSan 默认开启，因此配置阶段会做真实探测，不支持就会直接报错。可以：
换用 `-DCMAKE_CXX_COMPILER=clang++`；升级 GCC；或改用 WSL2 / Docker。
如果只想开 ASan，按 3.2 节手动指定 `-fsanitize=address`（同时加
`-DENABLE_SANITIZERS=OFF`）；确实不需要 sanitizer 时加 `-DENABLE_SANITIZERS=OFF`
做普通构建（但不满足作业的内存检查要求）。

**Q4. ASan 报错里没有函数名和行号？**
确认构建类型是 `Debug`（默认即是，带 `-g`），并保留 `-fno-omit-frame-pointer`。
另外要保证报错位置和你的源码行号对得上——修改代码后记得重新编译。

**Q5. 被移动后的对象该怎么处理？**
只保证“可析构、可重新赋值”，不要读取它的内容或 `size()`。
随附测试就是这么用的。建议移动后把源对象的指针置空、长度容量清零，
析构时用 `delete[] nullptr` 兜底。

**Q6. 为什么头文件叫 `my_string.h` 而不是 `string.h`？**
`include/` 会通过 `-Iinclude` 进入头文件搜索路径。若把文件命名为 `string.h`，
在某些工具链（尤其 macOS）上，标准库内部的 `#include <string.h>` 会先找到你的
文件而不是 C 标准库头文件，导致编译失败。改名可以彻底避开这个冲突。

**Q7. 对编译警告有什么要求？**
不要求 `-Werror`，但验收标准是“无警告编译”。完成前请把 `-Wall -Wextra -Wpedantic`
下的警告清零（常见如未使用参数、有符号/无符号比较等）。

**Q8. 可以自己加测试文件吗？**
可以，也非常鼓励；但请**新建自己的文件**，不要修改 `tests/` 下的任何文件。
随附测试是你验收的主要依据。

**Q9. 编译报 `has a different exception specifier`？**
说明该函数的声明里有 `noexcept`，但 `.cpp` 的定义里漏写了。把 `noexcept`
原样补上即可（移动构造、移动赋值、`operator[]`、`size`、`capacity`、`c_str`、
转换、`swap` 都带 `noexcept`）。详见 [`guide.md`](guide.md) 第 4 节。

**Q10. 为什么 ctest 没有跑流运算符（`<<` / `>>`）的测试？**
流操作是选做 bonus，默认不构建。需要加 `-DENABLE_BONUS_TESTS=ON` 配置，
构建后会多出 `bonus_stream` 并自动注册到 CTest（见 2.6 节）。

---

## 5. 完成前自检清单

- [ ] 默认（ASan + UBSan）构建：`cmake --build build -j` 无警告通过；
- [ ] `ctest --test-dir build --output-on-failure` 五个里程碑全部 Passed，
      每个测试程序单独运行时最后输出 `ALL TESTS PASSED`
      （m1 242 项 / m2 79 项 / m3 20 项 / m4 76 项 / m5 56 项检查）；
- [ ] `-DENABLE_SANITIZERS=OFF` 的普通构建同样全部通过、无警告；
- [ ] sanitizer 构建下无 ASan/UBSan 报错；
- [ ] `m5_strong_safety` 全过：注入 `std::bad_alloc` 时原对象内容 / 容量不变、不泄漏；
- [ ] （选做）`-DENABLE_BONUS_TESTS=ON` 后 `bonus_stream`（11 项）也全过；
- [ ] 空串（默认构造容量 ≥ 16）、长串、多次扩容、自赋值、自移动、自插入、
      自交换、非法位置异常都想过一遍；
- [ ] 没有使用 `std::string` / `std::string_view` / 任何 STL 容器；
- [ ] 没有修改 `tests/`；公开接口签名未被改动（测试里的 `static_assert` 会强制检查）；
