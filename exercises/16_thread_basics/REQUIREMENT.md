// 练习 16 —— std::thread 基础：启动、汇合与传参（知识点 #9；波次 2 开局）

## 背景

你正式进入并发世界。第一课不是锁、不是原子——是**把线程正确地开起来、
安全地等回来**。开得不对程序当场 `std::terminate`，传参不对错误静默。

四个术语先翻成白话：

- **join**：等它干完再走（汇合点——join 之前主线程停着等）
- **detach**：放它单飞（从此不管死活，thread 对象不再代表那个线程）
- **thread_local**：每个线程一份的私有全局变量——互不可见，改的是各自副本
- **参数陷阱**：thread 构造函数**先把实参拷一份**存进线程内部再转交给函数
  ——想传引用必须 `std::ref`，忘写不报错，改的是拷贝

场景：行情批处理的分片并行求和（低延迟系统的 map-reduce 雏形）。

**零输出要求**照旧；注释不要求；main 留空；判题端驱动验证。
工具链已预检：裸 g++ 线程直接可用，无需额外链接参数。

## 需求

**Q1. 区间求和 worker**
`sum_slice(const std::vector<int>& v, std::size_t begin, std::size_t end)`
→ 返回 `[begin, end)` 区间和（long long）。纯函数，不含线程知识。

**Q2. 分片并行求和（本题主菜）**
`parallel_sum(const std::vector<int>& v, unsigned nthreads)` → long long：
- 把数组切成 nthreads 片（**必须正确处理不整除**，如 10 个元素切 4 片）
- 每个线程算自己那片的和，**写进自己的专属槽位**（如
  `std::vector<long long> partials(nthreads)` 的第 k 格）——
  **零共享可变状态**，不许所有线程往同一个变量上 +=
- join 全部线程后汇总返回
- 结果与线程数无关：1、2、4 线程答案一致

**Q3. 参数传递——std::ref 陷阱**
worker：`add_many(int& v, int times)`（把 v 自增 times 次）。
函数 `bump_via_thread(int& x)`：起线程执行 `add_many(x, 1000)`，
join 后 x 恰好 +1000。**thread 构造对实参是先拷贝**——传 x 的引用
必须用 `std::ref`（想想：忘写会怎样？答案在 SR2）。

**Q4. thread_local 私有副本**
名字空间级定义 `thread_local int tl_ticks = 0;`
+ `tick_worker(int n)`（把 tl_ticks 加 n）
+ `my_ticks()`（返回当前线程的 tl_ticks）。
主线程的副本和子线程的副本互不可见——判题端会验证隔离性。

## 验收标准（判题端驱动逐条核对）

- `sum_slice`：区间边界正确（前缀/中间/全区间）
- `parallel_sum`：1/2/4 线程结果全等于单线程期望；**不整除切分正确**
  （10 元素切 4 片、切 3 片）；全程无数据竞争（每线程独立槽位）
- `bump_via_thread`：调用后 x 恰好 +1000（std::ref 正确使用）
- `thread_local` 隔离：主线程 `tick_worker(5)` 后 `my_ticks()==5`；
  两个子线程各自加 1000/2000 并 join 后，**主线程的 my_ticks() 仍是 5**
- 线程全部 join，无泄漏无 terminate；零输出零警告

## 简答题（答案写在 main.cpp 顶部注释块，白话作答）

SR1. `std::thread t(f);` 之后既不 join 也不 detach 就让 t 析构——会发生什么
     （精确到程序的行为）？join 和 detach 的本质区别？detach 出去的线程
     还引用着函数里的局部变量，函数返回后会怎样（这正是 RAII 视角下
     detach 的什么问题）？
SR2. 传给 thread 构造的实参经历了什么（两步：先拷进 thread 内部、
     运行期再转给函数）？忘写 std::ref 编译能过吗、现象是什么——
     为什么说它比编译错误更危险？
SR3. thread_local 变量有几个副本、归谁、什么时候生什么时候死？
     多个线程同时写各自的 thread_local 副本算数据竞争吗？
     什么场景用它比"全局变量 + 锁"更合适？
SR4. parallel_sum 为什么让每个线程写自己的槽位而不是共享一个总和变量
     （共享 += 是什么性质的错误；锁能救但代价是什么）？
     "分片-汇总"这个模式在低延迟世界叫什么名字、你能在哪些地方再用它？

## 约束

- **禁用** std::mutex / std::atomic / std::async——同步原语是 #10/#12 的课，
  本题只许"零共享可变状态"的裸 thread
- 不允许任何共享可变状态（本题不教锁，也不许埋雷）
- 零输出；C++17；接口名逐字符照需求抄（第六查）；改完自己编译
- 构建：`cmake --build --preset default --target 16_thread_basics`
