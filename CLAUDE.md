# project01 —— C++ 学习工作区

用户按知识点清单逐步学习 C++ 与系统编程。工作模式：**需求式出题 → 用户从零实现 → 判题循环整改 → 总结沉淀**。

**学习目标（判题从严标准）**：自如运用 C++，针对项目写出简洁高效、科学合理的代码。

## 核心规则

- **知识点清单（唯一事实来源）：`log/knowledge.md`** —— 唯一序号、学习内容、验收标准、状态
- **出题（需求式）→ `make-exercise` skill**。触发语："按 N 号出 M 道题""出复习题"
- **判题（循环整改）→ `judge-exercise` skill**。触发语："判题""检查""验收""我做完了"
- 每题两份产出：`exercises/NN_topic/REQUIREMENT.md`（需求文档，用户唯一看的）；`.claude/answers/NN_topic.md`（标准答案，判题参照，**用户不提前查看**）
- 用户自己写 `main.cpp`（无骨架、无签名提示）；简答题答案写在 main.cpp 顶部注释块
- 出题可参考 opencli（smart-search）互联网检索，或知识库直接命题
- **不自动 git commit**，提交时机由用户决定

## 目录职责（用户指定的规则）

- `doc/` —— **用户个人随笔，Claude 不在此目录写文件**
- `log/` —— 学习记录：学习计划、knowledge.md（知识点清单）、progress.md（进度表）、运行日志
- `exercises/` —— 练习题，一题一目录：`NN_topic/`（NN 为全局递增练习号，目录名即 target 名）；`test/` 为用户草稿区（随手试验，不参与验收）
- `include/` —— 跨练习公共工具（tester.h）
- `.claude/skills/` —— 工作流 skill（make-exercise 出题 / judge-exercise 判题）
- `.claude/answers/` —— 标准答案库（判题参照）

## 构建

- 日常：`cmake --preset default && cmake --build --preset default`
- 单题：`cmake --build --preset default --target NN_topic`
- Release 对比（-O2）：`cmake --preset release && cmake --build --preset release`，产物在 `build/bin-release/`

## 环境备忘

- **代码终端输出一律使用英文**（Windows 控制台 GBK 代码页下中文易乱码）；**注释一律使用中文**
- MinGW-W64 g++ 8.1 (x86_64-posix-seh)：C++17 完整可用
- C++20 concepts/ranges 支持有限，届时建议升级 MSYS2 GCC 13+
- CMake 4.1，generator 为 MinGW Makefiles
- knowledge.md 中 `[Linux]` 知识点动手需 WSL2/虚拟机；`[GPU]` 需 NVIDIA GPU；`[Python]` 需 Python 环境
