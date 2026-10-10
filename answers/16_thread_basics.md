# 练习 16 标准答案 —— std::thread 基础（知识点 #9）

## 参考实现

```cpp
// 练习 16 —— std::thread 基础：启动、汇合与传参
#include <cstddef>
#include <thread>
#include <utility>
#include <vector>

// ===== Q1：区间求和 worker（纯函数） =====
inline long long sum_slice(const std::vector<int>& v,
                           std::size_t begin, std::size_t end) {
    long long s = 0;
    for (std::size_t i = begin; i < end; ++i) {
        s += v[i];
    }
    return s;
}

// ===== Q2：分片并行求和——每线程写自己的槽位，join 后汇总 =====
inline long long parallel_sum(const std::vector<int>& v, unsigned nthreads) {
    std::vector<long long> partials(nthreads, 0);
    std::vector<std::thread> ts;
    ts.reserve(nthreads);

    const std::size_t n = v.size();
    const std::size_t base = n / nthreads;      // 每片基础长度
    const std::size_t extra = n % nthreads;     // 不整除：前 extra 片每片多一个
    std::size_t begin = 0;
    for (unsigned k = 0; k < nthreads; ++k) {
        const std::size_t len = base + (k < extra ? 1 : 0);
        ts.emplace_back([&v, &partials, k, begin, len] {
            partials[k] = sum_slice(v, begin, begin + len);   // 只写自己的格子
        });
        begin += len;
    }
    for (auto& t : ts) {
        t.join();                                // 汇合点：全部算完才汇总
    }

    long long total = 0;
    for (long long p : partials) {
        total += p;
    }
    return total;
}

// ===== Q3：参数传递——thread 构造先拷贝实参，传引用必须 std::ref =====
inline void add_many(int& v, int times) {
    for (int i = 0; i < times; ++i) {
        ++v;
    }
}

inline void bump_via_thread(int& x) {
    std::thread t(add_many, std::ref(x), 1000);  // 忘 std::ref = 改的是内部拷贝
    t.join();
}

// ===== Q4：thread_local——每个线程一份的私有副本 =====
thread_local int tl_ticks = 0;

inline void tick_worker(int n) {
    tl_ticks += n;          // 改的是"本线程的"副本
}

inline int my_ticks() {
    return tl_ticks;
}

int main() {
    return 0;  // 验证由判题端驱动完成
}
```

## 判题驱动（判题端使用，放 build/tmp，判完删除）

```cpp
// drive16.cpp
#include <cstdio>
#include <numeric>
#include <thread>
#include <vector>
#define main user_main
#include "../../exercises/16_thread_basics/main.cpp"
#undef main

static int fails = 0;
static int total = 0;
#define EXPECT(cond) do { ++total; if (!(cond)) { ++fails; \
    std::printf("[FAIL] line %d: %s\n", __LINE__, #cond); } } while (0)

int main() {
    // ===== Q1：区间求和 =====
    std::vector<int> v(1000);
    for (int i = 0; i < 1000; ++i) {
        v[i] = i + 1;
    }
    const long long expect = 500500;             // 1+2+...+1000
    EXPECT(sum_slice(v, 0, 4) == 10);            // 前缀
    EXPECT(sum_slice(v, 3, 8) == 4 + 5 + 6 + 7 + 8);
    EXPECT(sum_slice(v, 0, v.size()) == expect); // 全区间

    // ===== Q2：分片并行（结果与线程数无关；不整除切分） =====
    EXPECT(parallel_sum(v, 1) == expect);
    EXPECT(parallel_sum(v, 2) == expect);
    EXPECT(parallel_sum(v, 4) == expect);
    const std::vector<int> ten{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    EXPECT(parallel_sum(ten, 4) == 55);          // 10/4 不整除
    EXPECT(parallel_sum(ten, 3) == 55);          // 10/3 不整除

    // ===== Q3：std::ref =====
    int x = 0;
    bump_via_thread(x);
    EXPECT(x == 1000);

    // ===== Q4：thread_local 隔离 =====
    tick_worker(5);
    EXPECT(my_ticks() == 5);
    {
        std::thread t1(tick_worker, 1000);
        std::thread t2(tick_worker, 2000);
        t1.join();
        t2.join();
    }
    EXPECT(my_ticks() == 5);                     // 子线程的副本与 main 无关

    if (fails == 0) {
        std::printf("[PASS] %d checks passed\n", total);
    } else {
        std::printf("[FAIL] %d/%d failed\n", fails, total);
    }
    return fails == 0 ? 0 : 1;
}
```

## 预期结果

`[PASS] 11 checks passed`，退出码 0（多次运行结果稳定一致）。

## 设计意图

- **parallel_sum 是主菜**：分片-汇总是并发的"hello world 之后第一课"，
  零共享可变状态（每线程一格槽位）让你在没学锁之前就能写出**正确的**
  多线程程序——正确性来自结构，不来自同步工具
- **切分不整除**是必然撞上的边界（10/4）：逼你想清楚边界算术，
  也是日后 sharding 的基本功
- **bump_via_thread 的 std::ref**：thread 构造的 decay-copy 语义是
  静默错误的重灾区——本题要求写对，SR2 要求讲清"错起来什么样"
- **thread_local 用隔离性验证**：子线程各加 1000/2000，main 的 5 纹丝不动
  ——"每线程一份副本"不是背的，是断言出来的
- **join/detach/terminate 全部 SR 化**：未 join 析构 = terminate，
  现场演示会杀死判题驱动，概念考核放简答正合适（验收"能解释"）

## 评分要点

- sum_slice 边界（半开区间、long long 累加）
- parallel_sum：1/2/4 线程一致、不整除切分正确、**零共享可变状态**
  （共享总和变量直接判 FAIL——代码审查）、全部 join
- bump_via_thread：std::ref 正确（忘写则 x==0，驱动当场抓）
- thread_local：隔离性断言过；tl_ticks 定义在名字空间级
- SR 四问要点齐全（见下）；零输出零警告；接口名逐字符（第六查）

## 简答题要点（判题对照）

- SR1：**未 join 未 detach 就析构 → std::terminate，程序当场死**（不是泄漏、
  不是警告，是死）。join = 汇合：主线程停下等它干完，线程资源在 join 处
  回收；detach = 放单飞：线程对象与底层线程脱钩，对象析构无害，但从此
  无法汇合。detach 的线程引用函数局部 = 函数返回后悬垂（RAII 视角：
  线程的生命周期超过了它所用资源的生命周期——反过来违反 RAII）
- SR2：两步——构造时把实参**拷贝**一份存进 thread 内部（decay-copy），
  运行期把拷贝转交给函数。忘写 std::ref：**编译能过**（拷贝是合法的），
  现象是改的是内部拷贝、原变量纹丝不动——比编译错误危险正在于
  **静默**：程序看起来全对，结果不对，没有任何报警
- SR3：副本数 = 线程数，每个线程一份，随线程生（首次使用时初始化）
  随线程死（线程退出时析构）。各写各的副本**不是数据竞争**
  （竞争的定义是同一对象至少一个写）。适合场景：每线程的私有上下文
  （计数器、缓冲、随机数种子、缓存的句柄）——不需要共享就不用付锁的价
- SR4：共享一个总和变量 += 是**数据竞争（UB）**——读改写三步会被打断，
  丢更新。锁能救但每次 += 都要拿放锁，把并行的好处吃光。分片-汇总
  在低延迟世界叫 sharding / map-reduce 雏形：**能不共享就不共享，
  共享是最后手段**——线程池的任务分派、SPSC 队列（波次 9）、
  分片行情处理全是这个模式

## 常见错误预测（判题重点检查）

1. parallel_sum 所有线程往同一个 total 上 +=（数据竞争 UB——违反
   "零共享可变状态"，驱动多次运行可能侥幸过，代码审查 + 约束双重抓）
2. 切分用 n/nthreads 均分丢余数（10/4 只算前 8 个 → 结果错）
3. 忘 join（线程对象析构 → terminate，驱动当场崩）
4. bump_via_thread 忘 std::ref（x==0，当场抓）
5. lambda 捕获 [&, k, begin, len] 里引用捕获 + 值捕获混写出错
   （begin/len 必须值捕获或保证不变——引用捕获循环变量是经典坑：
   emplace_back 后 begin 已变）
6. tl_ticks 写成普通全局 int（隔离性断言失败 + 两个子线程写它 = 数据竞争）
7. thread 对象存不下/复用同一个变量（必须 vector<thread> 或顺序 join）
8. 接口拼写第六查：sum_slice/parallel_sum/add_many/bump_via_thread/
   tl_ticks/tick_worker/my_ticks 逐字符对照

## 模式点拨（判题时向用户点明）

- **零共享分片**：并发的第一正确姿势——正确性来自结构（每线程一格），
  不来自同步工具（锁/原子）。锁是 #10 的课，但"能不用锁"是终身纪律
- **join 是 RAII 的镜像**：获取线程资源于构造、归还于 join；
  未归还就析构 = terminate——这就是 std::jthread（C++20）要在析构里
  自动 join 的原因
- **std::ref 是引用的语言级显式化**：thread/async/bind 这类"延迟调用"
  的设施都 decay-copy——想让引用穿过它们必须显式打包。
  和 07 号"签名即合同"一脉：类型不撒谎
- **thread_local = 线程的私有命名空间**：高并发组件的标配
  （每线程缓存、每线程分配器 arena——波次 9 的内存池就这么用）
