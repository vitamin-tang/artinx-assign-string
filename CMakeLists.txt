cmake_minimum_required(VERSION 3.14)

project(assignment2_string LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 导出 compile_commands.json（放在 build/ 下）：clangd 依赖它做补全与跳转，
# VS Code 配置见 .vscode/ 与 docs/vscode.md。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 未指定构建类型时默认 Debug（保留调试信息，且不会定义 NDEBUG）
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# 打开常用告警，要求最终代码无警告
if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# AddressSanitizer + UndefinedBehaviorSanitizer（默认开启）
#
#   默认构建即带 ASan+UBSan（环境支持时）：
#     cmake -S . -B build && cmake --build build -j
#   如需关闭，做一次不带 sanitizer 的普通构建：
#     cmake -S . -B build-plain -DENABLE_SANITIZERS=OFF
#
# 仅支持 GCC / Clang（Linux、macOS）：开启时会做一次真实的编译 + 链接探测，
# 不支持（或使用 MSVC）则直接报错，并提示用 -DENABLE_SANITIZERS=OFF 关闭，
# 避免“以为开了 sanitizer 其实没开”。手动指定标志的方法见 docs/build-and-test.md。
# ---------------------------------------------------------------------------
option(ENABLE_SANITIZERS "Build with AddressSanitizer + UndefinedBehaviorSanitizer" ON)

if(ENABLE_SANITIZERS)
  if(MSVC)
    message(FATAL_ERROR
      "ENABLE_SANITIZERS 默认开启，但仅支持 GCC/Clang。\n"
      "MSVC 请改用 /fsanitize=address（详见 docs/build-and-test.md）；\n"
      "若只需不带 sanitizer 的普通构建，请加 -DENABLE_SANITIZERS=OFF。")
  endif()

  # 必须做一次真实的编译 + 链接：只检查编译标志（check_cxx_compiler_flag）会因为
  # 链接时缺少 -fsanitize 而误判 GCC/Clang 不支持。
  include(CheckCXXSourceCompiles)
  set(CMAKE_REQUIRED_FLAGS "-fsanitize=address,undefined")
  check_cxx_source_compiles("int main() { return 0; }" COMPILER_SUPPORTS_ASAN_UBSAN)
  unset(CMAKE_REQUIRED_FLAGS)
  if(NOT COMPILER_SUPPORTS_ASAN_UBSAN)
    message(FATAL_ERROR
      "当前编译器不支持 -fsanitize=address,undefined，无法开启默认启用的 ENABLE_SANITIZERS。\n"
      "请改用 GCC/Clang（例如 -DCMAKE_CXX_COMPILER=clang++），或到 docs/build-and-test.md\n"
      "查看手动指定 sanitizer 标志的方法；\n"
      "若只需不带 sanitizer 的普通构建，请加 -DENABLE_SANITIZERS=OFF。")
  endif()

  add_compile_options(
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
    -fno-sanitize-recover=all   # UBSan 报错立即终止，便于定位
  )
  add_link_options(-fsanitize=address,undefined)
endif()

include(CTest)  # 提供 enable_testing()

# ---------------------------------------------------------------------------
# 构建目标
#
# 如果你的实现拆分成多个 .cpp 文件，把新文件追加到 STRING_SOURCES 即可。
# ---------------------------------------------------------------------------
set(STRING_SOURCES
  src/my_string.cpp
  # 在这里追加你自己的 .cpp 文件
)

add_library(string_lib STATIC ${STRING_SOURCES})
target_include_directories(string_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ---------------------------------------------------------------------------
# 里程碑测试（M1/M2 必做，M3/M4/M5 进阶）
#
# 每个里程碑一个独立的可执行文件。测试只引用"本里程碑及更早里程碑"的函数，
# 所以只实现了 M1 时，m1_basics 也能链接并运行——先看到 M1 变绿，再往下做。
# 第一次构建时的链接错误（undefined reference）就是该里程碑还没实现的函数清单。
#
#   cmake --build build --target all_tests   # 只构建全部里程碑测试
#   cmake --build build --target check       # 构建并运行全部测试
# ---------------------------------------------------------------------------
set(MILESTONE_TESTS
  m1_basics
  m2_value_semantics
  m3_move
  m4_edge_cases
  m5_strong_safety
)

foreach(test_name IN LISTS MILESTONE_TESTS)
  add_executable(${test_name} tests/${test_name}.cpp)
  target_link_libraries(${test_name} PRIVATE string_lib)
  target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  if(BUILD_TESTING)
    add_test(NAME ${test_name} COMMAND ${test_name})
  endif()
endforeach()

# 聚合目标：一次构建全部里程碑测试
add_custom_target(all_tests DEPENDS ${MILESTONE_TESTS})

# 便捷目标：构建并运行全部测试
add_custom_target(check
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  DEPENDS ${MILESTONE_TESTS}
  USES_TERMINAL
)

# ---------------------------------------------------------------------------
# 选做（bonus）：流运算符 << / >> 的测试
#
#   cmake -S . -B build -DENABLE_BONUS_TESTS=ON
#   cmake --build build -j
#   ctest --test-dir build --output-on-failure
#
# 默认不构建：未实现流操作时，里程碑测试（m1_basics ~ m4_edge_cases）
# 仍能正常构建与通过。
# ---------------------------------------------------------------------------
option(ENABLE_BONUS_TESTS "Build the optional stream-operator tests" OFF)

if(ENABLE_BONUS_TESTS)
  add_executable(bonus_stream tests/stream_tests.cpp)
  target_link_libraries(bonus_stream PRIVATE string_lib)
  target_include_directories(bonus_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)

  if(BUILD_TESTING)
    add_test(NAME bonus_stream COMMAND bonus_stream)
  endif()

  # 开启 bonus 时，all_tests / check 也覆盖 bonus
  add_dependencies(all_tests bonus_stream)
  add_dependencies(check bonus_stream)
endif()
