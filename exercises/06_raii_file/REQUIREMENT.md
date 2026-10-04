// 练习 06 —— RAII 文件句柄（LogFile 与五法则）

> 知识点 #1 RAII 与资源管理（构造/析构与资源生命周期、栈展开、异常安全）；
> 综合运用练习 01 的初始化列表、练习 04 的深拷贝，并首次落地移动语义（五法则）。

## 背景

行情录制组件中，每个 Recorder 独占一个日志文件。文件句柄（`FILE*`）是典型的裸资源：
忘记 close 就泄漏，close 两次程序直接崩，直接拷贝指针会造成 double close。
你要手写一个 RAII 类 `LogFile`，把句柄的生命周期焊死在对象生命周期上，
并完整实现五法则——析构、拷贝构造、拷贝赋值、移动构造、移动赋值，一个不少。

## 需求

### 类：`LogFile`（直接写在 main.cpp 即可）

**Q1. 构造与初始化列表**
构造函数接收文件名与打开模式（`"w"` / `"a"` / `"r"`），用 `<cstdio>` 的 `fopen` 打开。
所有成员必须经**初始化列表**初始化（句柄成员允许列表内置空后体内打开，
也允许直接在列表里调用 `fopen`，失败检查放构造体内）。
打开失败抛 `std::runtime_error`，异常消息需含文件名；构造失败时不得泄漏任何资源。

**Q2. 析构**
持有句柄则 `fclose` 并打印 `[LogFile] dtor: <name> (closed)`；
句柄为空则打印 `[LogFile] dtor: (empty)`。不得对空指针 `fclose`。

**Q3. 写入与统计**
提供写入操作 `write`（整行字符串，如 `"alpha\n"`），每次写入后 `fflush` 立即落盘；
提供三个查询：文件名、是否持有句柄、累计写入字节数。
计数语义：**本对象经当前句柄实际写入的字节数**（拷贝后从 0 重计，移动则随句柄一并移交）。

**Q4. 拷贝语义 = 独立句柄（深拷贝资源，不是拷指针）**
拷贝构造 / 拷贝赋值必须为新对象重新 `fopen` 同一文件，且用**追加模式 `"a"`**
（想一想：用原模式 `"w"` 重开会发生什么？）。
拷贝赋值还须满足：① **自赋值安全**——状态与计数不变；② **先获取、后释放**——
新句柄打开失败时抛异常，且原句柄保持可用（强异常保证）。

**Q5. 移动语义 = 所有权移交**
移动构造 / 移动赋值接管句柄与累计字节数，源对象置为"空"
（句柄空、计数归零、可安全析构）。两个移动操作必须标 `noexcept`。
移动赋值须正确释放自身旧句柄（判空）。

**Q6. trace 输出**（判题机器逐行核对，格式必须完全一致，全部英文）：

```
[LogFile] ctor: <name>
[LogFile] copy-ctor: <name>
[LogFile] copy-assign: <old> -> <new>          （自赋值两名字相同，可加后缀 (self)）
[LogFile] move-ctor: <name>
[LogFile] move-assign: <old> -> <new>
[LogFile] dtor: <name> (closed)
[LogFile] dtor: (empty)
```

### 演示程序 main（按顺序执行）

**Q7.** 状态打印格式 `[stat] <标签>: <name>, bytes=<n>, handle=<yes/no>`
（标签用下述变量名）。依次演示：

1. 内层作用域以 `"w"` 打开 `log.txt`（对象 a），写 `alpha\n`、`bravo\n`，打印 a 状态；
   离开作用域自动关闭（trace 可见）。
2. 以 `"a"` 再开 `log.txt`（对象 b），写 `charlie\n`，打印 b 状态。
3. `LogFile c = b;`（拷贝构造），c 写 `delta\n`，打印 c、b 状态。
4. 另开 `other.txt`（对象 d，`"w"`），写 `echo\n`；执行 `d = b;`（拷贝赋值）后打印 d；
   d 写 `foxtrot\n`；再通过引用自赋值（`LogFile& r = d; d = r;`），打印 d 验证无变化。
5. `LogFile e = std::move(b);`（移动构造），打印 e、b 状态；e 写 `golf\n`，再打印 e。
6. 另开 `tmp.txt`（对象 f，`"w"`）；`f = std::move(e);`（移动赋值），打印 f、e 状态；
   f 写 `hotel\n`，再打印 f。
7. 用 `"r"` 模式构造 `missing.txt` → 捕获异常打印 `[main] caught: <what()>`；
   确认**没有**这个对象的 dtor trace（它从未存在）。
8. 写一个函数：内部以 `"w"` 打开 `unwind.log`、写 `before-throw\n` 后抛
   `std::runtime_error("simulated failure")`；main 捕获并打印 `[main] caught: <what()>`。
   trace 顺序必须证明：**dtor 在 catch 打印之前执行**（栈展开）。
9. 程序末尾用裸 `fopen/fgets/fclose` 逐行读回 `log.txt`，每行打印 `[file] <行内容>`。

## 验收标准（判题逐条核对）

- 程序正常退出（退出码 0，无崩溃、无 CRT 报错）
- trace 序列与上述操作一一对应；每个句柄恰好关闭一次，无 double-close
- 字节计数链正确：
  a=12；b=8；c=6；d 拷贝赋值后 0 → foxtrot 后 8 → 自赋值后仍 8；
  e 移动构造后 8 → golf 后 13；f 移动赋值后 13 → hotel 后 19
- 被移动对象：bytes=0、handle=no（name 允许打印为空）
- `log.txt` 最终 7 行：alpha, bravo, charlie, delta, foxtrot, golf, hotel
  （判题以行数为准；Windows 文本模式 `\n` 落盘为 `\r\n`，物理字节数会偏大，属正常现象）
- `other.txt` 仅一行 echo；`tmp.txt` 存在且为 0 字节；`unwind.log` 含 before-throw
- 构造失败：无该对象 dtor trace；异常捕获后程序继续执行
- 栈展开：`unwind.log` 的 dtor trace 出现在 `[main] caught:` 之前
- main 结束时 f, e, d, c, b 逆序析构：f/d/c 打印 (closed)，e/b 打印 (empty)
- 五个特殊成员函数全部显式手写；移动操作 `noexcept`；成员用初始化列表

## 简答题（答案写在 main.cpp 顶部注释块）

SR1. 构造函数抛异常时，析构函数为什么不会被调用？已构造完成的成员会发生什么？
     这如何保证"构造失败必不泄漏"？若构造函数要先后获取两份资源，第二份失败怎么办？
SR2. `std::move` 本身移动了任何东西吗？它到底做了什么？
     为什么移动构造/赋值必须把源对象句柄置空，不置空会发生什么？
SR3. 移动构造/移动赋值为什么要标 `noexcept`？提示：`std::vector<LogFile>` 扩容时，
     元素搬迁在"移动构造有无 noexcept"下分别走哪条路径，为什么？
SR4. 什么是栈展开，发生在什么时机？结合本次运行，指出哪两行输出的先后顺序
     证明了栈展开发生；从 throw 到进入 catch 之间发生了什么？

## 约束

- 资源必须是 `<cstdio>` 裸句柄（`fopen`/`fclose`）；**禁止** `std::fstream`、
  禁止智能指针托管句柄、禁止提供公开的 `close()`（释放只经析构与赋值运算符）
- 五件套全部手写，不得使用 `= default` / `= delete`
- 移动构造/移动赋值必须 `noexcept`
- C++17（g++ 8.1）；终端输出全英文，注释全中文
- 文件创建在运行时工作目录（判题在仓库根目录运行，产物为 log.txt 等 4 个文件）
- 构建：`cmake --build --preset default --target 06_raii_file`，运行 `./build/bin/06_raii_file.exe`
- （可选）数值断言可用 `include/tester.h` 的 `CHECK_EQ` 自检，不计入必需验收
