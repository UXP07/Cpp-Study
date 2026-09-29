# project01 —— C++ 学习工作区

用户按知识点清单逐步学习 C++ 与系统编程。工作模式：按 N 号请 Claude 出题 → 用户独立编程 → Claude 验收 → 用户确认后标记已学习。

## 核心规则

- **知识点清单（唯一事实来源）：`log/knowledge.md`** —— 唯一序号、学习内容、验收标准、状态
- **出题 / 验收 / 标记已学习 / 查进度 → 使用 `make-exercise` skill**（`.claude/skills/make-exercise/SKILL.md`）。触发语："按 N 号出 M 道题""验收""把 N 号标为已学习"
- 出题可参考 opencli（smart-search）互联网检索，或知识库直接命题
- **不自动 git commit**，提交时机由用户决定

## 目录职责（用户指定的规则）

- `doc/` —— **用户个人随笔，Claude 不在此目录写文件**
- `log/` —— 学习记录：学习计划、knowledge.md（知识点清单）、progress.md（进度表）、运行日志
- `exercises/` —— 练习题，一题一目录：`NN_topic/`（NN 为全局递增练习号，取自 progress.md，目录名即 target 名）
- `include/` —— 跨练习公共工具（tester.h）
- `.claude/skills/` —— 工作流 skill

## 构建

- 日常：`cmake --preset default && cmake --build --preset default`
- 单题：`cmake --build --preset default --target NN_topic`
- Release 对比（-O2）：`cmake --preset release && cmake --build --preset release`，产物在 `build/bin-release/`

## 环境备忘

- MinGW-W64 g++ 8.1 (x86_64-posix-seh)：C++17 完整可用
- C++20 concepts/ranges 支持有限，届时建议升级 MSYS2 GCC 13+
- CMake 4.1，generator 为 MinGW Makefiles
- knowledge.md 中 `[Linux]` 知识点动手需 WSL2/虚拟机；`[GPU]` 需 NVIDIA GPU；`[Python]` 需 Python 环境
