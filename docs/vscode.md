# VS Code 使用指南（clangd + CodeLLDB）

仓库里预置了 `.vscode/` 配置，克隆后开箱即用：**clangd** 负责补全、跳转与诊断，
**CodeLLDB** 负责断点调试。本文说明首次配置、常用操作与排错。

其他构建、测试、ASan/UBSan 的细节见 [`build-and-test.md`](build-and-test.md)。

## 0. 前置

- Linux / macOS，或 Windows + WSL2（Windows 原生请改用 WSL2，配置里的命令按 POSIX 写）；
- 已按 [`build-and-test.md`](build-and-test.md) 第 1 节准备好 GCC/Clang 与 CMake；
- Windows 用户：在 WSL 里装好 VS Code Server（在 WSL 终端里执行 `code .` 即可），
  扩展与调试器都装在 WSL 一侧。

## 1. 安装扩展

打开仓库后 VS Code 会提示安装推荐扩展（见 `.vscode/extensions.json`），点“安装”即可；
也可以在终端里执行：

```bash
code --install-extension llvm-vs-code-extensions.vscode-clangd
code --install-extension vadimcn.vscode-lldb
```

> 如果你之前装了微软的 C/C++ 扩展，`.vscode/settings.json` 已经关掉了它的 IntelliSense，
> 避免和 clangd 抢诊断；也可以直接卸载它。

## 2. 首次配置（只需一次）

`Ctrl+Shift+P` → `Tasks: Run Task` → **cmake: configure**（等价于
`cmake -S . -B build`）。它会生成：

- `build/`：构建目录；
- `build/compile_commands.json`：**clangd 依赖它**来获知头文件路径与编译选项。

配置完成后打开 `src/my_string.cpp`，应该立刻能补全 `include/my_string.h` 里的接口、
用 `F12` / `Ctrl+点击` 跳转定义。**没跑这一步的话编辑器里会满屏红波浪线。**

## 3. 常用操作

| 操作 | 方式 |
| --- | --- |
| 构建 | `Ctrl+Shift+B`（任务 `cmake: build`） |
| 运行全部测试 | `Ctrl+Shift+P` → `Tasks: Run Task` → `ctest: 运行全部测试` |
| 只跑一个里程碑 | 任务 `ctest: 只跑一个里程碑`，弹出列表里选 m1~m5 / bonus |
| 清理重建 | 任务 `cmake: 清理并重新配置`（需要 bash） |
| 调试 | `Ctrl+Shift+D` 打开 Run and Debug，选 `调试 M1 · 基础接口 (m1_basics)` 等，按 `F5` |

调试配置会先自动构建（`preLaunchTask`），所以按 `F5` 就是“编译 + 启动调试”。
在行号左侧点一下打断点，可以单步进入自己的 `String` 函数，查看 `data_` / `size_` /
`capacity_` 的变化——排查 `insert` 自插入、扩容、悬空指针时比打印日志高效得多。

调试 bonus 之前，先运行一次任务 **cmake: configure (bonus)**（它会加上
`-DENABLE_BONUS_TESTS=ON`），否则 `build/bonus_stream` 不存在。

## 4. clangd 常见问题

| 症状 | 处理 |
| --- | --- |
| 满屏红波浪线、找不到 `<cstddef>` | 先跑 `cmake: configure`（缺 `compile_commands.json`）；或你用的是别的构建目录，见下一条 |
| 用 `build-plain/` 等其它目录构建 | `.vscode/settings.json` 里 `--compile-commands-dir` 固定指向 `build/`，改这一行即可 |
| 想看 clangd 到底用了什么命令 | 输出面板（`Ctrl+Shift+U`）切到 `clangd` |
| 索引坏了 / 很慢 | 删除 `.cache/clangd/`（已 gitignore）后重启窗口，会重新索引 |
| 连系统头文件都找不到 | 在 `clangd.arguments` 里加 `--query-driver=/usr/bin/c++,/usr/bin/g++,/usr/bin/clang++`（按本机编译器路径调整） |

clangd 的告警是本机静态分析，和评分无关；**验收以 `ctest` 与 ASan/UBSan 为准**。

## 5. CodeLLDB 常见问题

| 症状 | 处理 |
| --- | --- |
| 启动时报找不到 `program` | 先构建（`Ctrl+Shift+B`）；bonus 配置还需要 `cmake: configure (bonus)` |
| Windows 上路径对不上 | 本配置按 Linux/macOS/WSL 写；Windows 请用 WSL2 |
| 想抓内存错误现场 | ASan/UBSan 报错时程序会 abort，调试器会停在信号处，直接看调用栈即可定位到行号 |
| 想看内存泄漏 | macOS 的 LeakSanitizer 支持有限，请在 Linux/WSL 下跑（见 `build-and-test.md` 第 3.3 节） |
| 断点先停在测试文件里 | 正常：`F11`（Step Into）进入你自己的 `String` 实现 |

## 6. 提交前

`.vscode/` 是仓库的一部分（方便所有人），如果你本地改过里面的路径（例如换成
`build-plain`），提交前建议还原，避免把个人配置提交上去：

```bash
git checkout -- .vscode
```
