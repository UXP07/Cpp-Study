// 练习 17 —— mutex 与锁：RAII 守卫、死锁与一次初始化（知识点 #10）

## 背景

16 号你学会了"零共享"——但有些状态天生要共享（账户余额、队列）。
现在上锁。核心纪律只有两条：**锁的生命周期交给 RAII**（你 07 号手写的
ScopedLock 的标准库正版）、**多把锁要么统一顺序要么一次全拿**。

三个术语白话：

- **lock_guard**：最轻的锁守卫——构造时锁、析构时开，中途不能动
- **unique_lock**：重量级守卫——支持延迟锁/中途手动开/可移动/限时尝试，
  代价是维护"当前是否持有"的状态
- **scoped_lock（C++17）**：一次拿任意多把锁，内部用"试探-退让"算法，
  任意顺序调用都不死锁

**零输出**照旧；注释不要求；main 留空；判题端驱动验证（死锁用外部超时判死刑）。

## 需求

**Q1. class Counter（线程安全计数器）**
`inc()` / `get()`（const，注意锁成员需 mutable）——mutex + lock_guard。
16 号那个"共享 += 是数据竞争"的问题，现在用锁正面解决。

**Q2. class Account（无锁账户 + 外部加锁）**
`adjust(long)`（直接加减，**类本身不管线程安全**）、`balance()`、
`mutex()`（暴露锁的访问器——转账要同时锁两把，这是常见妥协）。

**Q3. 转账的两版（死锁构造与修复——本题主菜）**
- `transfer_deadlock_prone(Account& from, Account& to, long amt)`：
  两个 lock_guard **逐个**加锁（先 from 后 to）。写出这个版本并保证编译
  通过即可——**判题驱动不会调用它**（调用即挂）。SR2 要解释它何时死锁
- `transfer_safe(...)`：**`std::scoped_lock` 同时锁两把**（或 unique_lock
  的 defer_lock + std::lock，二选一）——判题端会开 8 线程双向高压转账

**Q4. call_once 一次初始化**
函数 `init_once()`：内部静态 `std::once_flag`，首次调用执行初始化
（初始化体内给一个全局计数 `init_runs` +1）。多线程同时首调，
初始化体**恰好执行一次**。

## 验收标准（判题端驱动核对；死锁由外部 timeout 判死刑）

- Counter：8 线程 × 25000 次 inc == 200000，任意次运行稳定
- transfer_safe 高压：A=100000、B=0，8 线程各 5000 次双向转账
  （A→B 与 B→A 各半），结束后 **A+B 守恒 == 100000**，读总数也要加锁
- init_once：8 线程并发首调，init_runs == 1
- transfer_deadlock_prone 仅要求存在且正确（逐个加锁的形状），不运行
- 零输出零警告；全程无挂起

## 简答题（答案写在 main.cpp 顶部注释块，白话作答）

SR1. lock_guard 和 unique_lock 各自的定位？默认该用谁？unique_lock 多出来
     的能力有哪些（至少四个：延迟、中途开、可移动、限时/尝试），
     本题 transfer_safe 的 defer_lock 版本里 defer 是什么意思？
SR2. 死锁的四个必要条件（白话各一句）？最容易破坏的是哪一个、怎么破坏？
     transfer_deadlock_prone 在什么并发场景下挂（具体到两个线程各自
     拿着什么等什么）？std::scoped_lock 靠什么不死锁？
SR3. Counter 的 inc() 从进函数到退出，锁持有了多长？把"锁的粒度"说成
     一条纪律；单条 `v_++` 若只求原子不维护复合不变式，还有什么更轻的
     工具（预告下一题）？

## 约束

- 禁 atomic（本题全部用锁；更轻的工具是 19 号的课）
- transfer_deadlock_prone 不得被任何会执行的代码路径调用
- 零输出；C++17；接口名逐字符照抄（第七查）；改完自己编译
- 构建：`cmake --build --preset default --target 17_mutex_locks`
