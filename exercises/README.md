# exercises — 练习题

一题一目录，目录名即可执行文件名（两位编号 + 主题，如 `01_raii`）。

## test/ —— 学习草稿区

`test/` 不是正式练习：学习时随手试验代码的地方，**不参与验收、不记录进度**。
约定只保留一个 `main.cpp`（当前试验），有价值的试验建议自行 commit 或整理进笔记。

## 约定
- 目录内所有 `.cpp` 编入该练习的可执行文件（多文件练习随意加，头文件也放本目录）
- 题目写在 `main.cpp` 顶部注释：背景、任务清单（TODO）、提示
- 验收标准写成 `include/tester.h` 的 `CHECK` / `CHECK_EQ` 断言
- 跨练习的公共代码放根目录 `include/`
- `00_selfcheck` 是 tester.h 的自检示例，可当作练习目录的模板

## 构建与运行（单题）
```
cmake --preset default
cmake --build --preset default --target 01_raii
./build/bin/01_raii.exe
```

新建子目录即自动纳入构建（CMakeLists.txt 自动发现），无需改任何配置。
