# log — 运行日志

存放程序运行时产生的日志文件。

当前示例程序暂未写入日志；需要时在代码中将日志输出路径指向本目录即可。


  最终结构

  project01/
  ├── CMakeLists.txt          # 构建脚本（C++17，-Wall -Wextra）
  ├── CMakePresets.json        # MinGW Makefiles + binaryDir=build/obj + Debug
  ├── include/greeter.h        # 头文件（声明）
  ├── src/main.cpp             # 源文件（入口）
  ├── src/greeter.cpp          # 源文件（实现）
  ├── doc/README.md            # 文档与心得
  ├── log/README.md            # 运行日志
  └── build/
      ├── bin/project01.exe    # ← 仅 exe（已运行，输出 Hello, project01!，退出码 0）
      └── obj/                 # ← 仅中间文件（CMakeCache.txt、CMakeFiles/、Makefile、.obj）

  验证结果

  - cmake --preset default → 配置到 build/obj，生成 MinGW Makefiles
  - cmake --build --preset default → 编译链接成功，exe 落在 build/bin
  - build/bin/project01.exe → 输出 Hello, project01!，退出码 0

  日常使用

  cmake --preset default          # 配置（仅需一次，改 CMakeLists 后自动重新配置）
  cmake --build --preset default  # 编译
  ./build/bin/project01.exe       # 运行

  新增源文件时：.cpp 放 src/ 并加入 CMakeLists.txt 的 add_executable，.h 放 include/（无需改配置，include 路径已生效）。彻底清理只需删除 build/ 目录。