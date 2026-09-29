# project01 —— C++ 学习工作区

用户在此按学习计划逐步学习 C++ 与系统编程。工作模式：学完知识点 → 请 Claude 出题 → 用户独立编程 → Claude 验收。

## 目录职责（用户指定的规则）
- `doc/` —— **用户个人随笔，Claude 不在此目录写文件**
- `log/` —— 学习记录：学习计划、`progress.md` 进度表、程序运行日志
- `exercises/` —— 练习题，一题一目录
- `include/` —— 跨练习公共工具（`tester.h` 等）

## 出题规范（用户请求出题时）
1. 新建 `exercises/NN_主题/`，NN 两位编号递增（查 `log/progress.md` 取下一个编号）
2. `main.cpp` 顶部注释写：题目背景、任务清单（TODO）、提示；内容对应刚学的知识点
3. 提供函数签名骨架 + `#include "tester.h"` 验收断言，**不给实现**
4. 出题后构建验证骨架本身可编译（骨架先返回占位值，断言允许 FAIL）

## 验收规范（用户说"做完了"时）
1. `cmake --build --preset default --target NN_主题` 编译，零 warning
2. `./build/bin/NN_主题.exe` 运行，全部 `[PASS]` 才算通过
3. review 代码：指出 bug、未定义行为、风格问题、更优写法；除非用户要求，不直接改代码
4. 通过后在 `log/progress.md` 添一行（编号、主题、日期、一句话点评）

## 构建
- 日常：`cmake --preset default && cmake --build --preset default`
- 单题：`cmake --build --preset default --target NN_主题`
- Release 对比（-O2）：`cmake --preset release && cmake --build --preset release`，产物在 `build/bin-release/`

## 环境备忘
- MinGW-W64 g++ 8.1 (x86_64-posix-seh)：C++17 完整可用
- C++20 concepts/ranges 支持有限，届时建议升级 MSYS2 GCC 13+
- CMake 4.1，generator 为 MinGW Makefiles
