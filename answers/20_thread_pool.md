# 练习 20 标准答案 —— 线程池（知识点 #13；波次 2 毕业设计）

## 参考实现

```cpp
// 练习 20 —— 线程池：任务队列 + 条件变量 + 优雅关停
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(unsigned n) {
        workers_.reserve(n);
        for (unsigned i = 0; i < n; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void submit(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> g(m_);
            if (closed_) {
                throw std::runtime_error("submit on closed pool");
            }
            tasks_.push_back(std::move(task));
        }
        cv_.notify_one();
    }

    // 优雅关停：置标志、叫醒全体、join 全体；队列余货被 worker 排干
    void shutdown() {
        bool expected = false;
        if (!closing_.compare_exchange_strong(expected, true)) {
            return;                             // 幂等：第二个调用者直接走
        }
        {
            std::lock_guard<std::mutex> g(m_);
            closed_ = true;
        }
        cv_.notify_all();
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
    }

    ~ThreadPool() { shutdown(); }               // RAII：忘了手动关也不漏线程

private:
    void worker_loop() {
        std::unique_lock<std::mutex> lk(m_);
        for (;;) {
            cv_.wait(lk, [this] { return !tasks_.empty() || closed_; });  // 非空或关停
            if (tasks_.empty()) {
                return;                         // 关停且排干：收工
            }
            std::function<void()> task = std::move(tasks_.front());
            tasks_.pop_front();
            lk.unlock();                        // 执行任务不持锁（锁粒度）
            task();
            lk.lock();
        }
    }

    std::mutex m_;
    std::condition_variable cv_;
    std::deque<std::function<void()>> tasks_;
    bool closed_ = false;
    std::atomic<bool> closing_{false};          // shutdown 幂等闸
    std::vector<std::thread> workers_;
};

int main() {
    return 0;
}
```

## 判题驱动（判题端使用，放 build/tmp，判完删除；timeout 判死刑）

```cpp
// drive20.cpp
#include <atomic>
#include <cstdio>
#include <stdexcept>
#include <thread>
#define main user_main
#include "../../exercises/20_thread_pool/main.cpp"
#undef main

static int fails = 0;
static int total = 0;
#define EXPECT(cond) do { ++total; if (!(cond)) { ++fails; \
    std::printf("[FAIL] line %d: %s\n", __LINE__, #cond); } } while (0)

int main() {
    // ===== 基础压测：4 工人 × 10000 任务，无丢失 =====
    {
        std::atomic<long> done{0};
        {
            ThreadPool pool(4);
            for (int i = 0; i < 10000; ++i) {
                pool.submit([&done] { done.fetch_add(1, std::memory_order_relaxed); });
            }
            pool.shutdown();
        }
        EXPECT(done == 10000);
    }

    // ===== 带余货关停：submit 500 后立刻 shutdown，全部执行完 =====
    {
        std::atomic<long> done{0};
        ThreadPool pool(4);
        for (int i = 0; i < 500; ++i) {
            pool.submit([&done] { done.fetch_add(1, std::memory_order_relaxed); });
        }
        pool.shutdown();                        // 队列可能还有大量余货
        EXPECT(done == 500);
    }

    // ===== 关停后 submit 抛 =====
    {
        ThreadPool pool(2);
        pool.submit([] {});
        pool.shutdown();
        bool threw = false;
        try { pool.submit([] {}); } catch (const std::runtime_error&) { threw = true; }
        EXPECT(threw);
        pool.shutdown();                        // 幂等：再关一次不炸
    }

    // ===== 析构自动关停 =====
    {
        std::atomic<long> done{0};
        {
            ThreadPool pool(2);
            for (int i = 0; i < 100; ++i) {
                pool.submit([&done] { done.fetch_add(1, std::memory_order_relaxed); });
            }
        }                                       // 出作用域：析构 shutdown
        EXPECT(done == 100);
    }

    if (fails == 0) {
        std::printf("[PASS] %d checks passed\n", total);
    } else {
        std::printf("[FAIL] %d/%d failed\n", fails, total);
    }
    return fails == 0 ? 0 : 1;
}
```

（挂起防护：判题端以 `timeout 10` 外部运行驱动，挂起即死锁/漏唤醒，
直接 FAIL。）

## 预期结果

`[PASS] 4 checks passed`，退出码 0，多轮稳定；`timeout 10` 运行。

## 设计意图

- **worker 循环是 18 号 TsQueue 的灵魂内嵌版**：谓词"非空或关停"、
  先清货再收工、关停 notify_all——把上一题的手艺原样搬进池
- **锁粒度（17 号纪律）**：取任务持锁、执行任务放锁——否则一个慢任务
  把 submit 全堵死
- **shutdown 幂等 + RAII**：closing_ 原子闸防两个线程同时 shutdown
  重复 join；析构自动关——06 号以来的"释放恰好一次"在关停语义上的复现
- **std::function 特批**：任务类型擦除的代价（一次堆分配 + 间接调用）
  是本设计的已知成本——低延迟池会用模板/move-only 任务（波次 9 优化点）
- **压测三面**：数量守恒（无丢失）、余货排干（优雅关停核心）、
  异常语义（关停后 submit）——池的正确性三角

## 评分要点

- worker 谓词等待"非空或关停"；取任务持锁/执行放锁（审查）
- 基础压测 10000 无丢失；余货 500 全执行；关停后 submit 抛；重复
  shutdown 不炸；析构自动关停
- shutdown 的标志置位与 notify_all 齐全；join 全体
- SR 四问要点齐全；零输出零警告；接口逐字符（第七查）

## 简答题要点（判题对照）

- SR1：只等"非空"——shutdown 置标志时队列已空、工人全在睡，
  notify_all 没人应答也得有人**重新查谓词**才能醒——等"非空"的工人
  醒来条件永假，join 永等（外部看 = 挂死）。只等"关停"——余货直接
  丢（500 个任务没跑）。谓词必须"非空或关停"，醒来先干活：队列非空
  就取任务执行，队列空且关停才 return——**先清货再收工**，优雅关停
  的定义就是这个顺序
- SR2：持锁执行任务 = 任务跑多久，submit/其他工人取任务就被堵多久
  （锁内做昂贵操作）。这叫锁粒度过粗/持锁跨越昂贵操作。正确姿势：
  锁内只做"改队列"这一步，执行挪到锁外——临界区缩到最小
- SR3：计数只求原子、不携带发布——relaxed 够（19 号判据）。
  submit 高频锁竞争：所有生产者抢一把队列锁——低延迟的解法方向：
  无锁队列（#55）、每工人独立队列（work stealing，#65 事件驱动架构
  的标配）、或分片多队列
- SR4：省了每任务的线程创建/销毁（每次 ~10µs 级 + 栈内存 8MB 预留）
  且并发上限可控（12 核起 12 个工人而不是 10000 个线程）。池不够好：
  任务极短时 function 擦除 + 锁竞争占大头（每任务开销 > 任务本身）——
  波段 9 的无锁化就是从这里出发的

## 常见错误预测（判题重点检查）

1. worker 裸 wait（无谓词）——挂死，timeout 抓
2. 谓词只写 `closed_` 或只写 `!tasks_.empty()`——余货丢或关停挂死
3. 持锁执行 task（submit 被慢任务堵死——压测里可能侥幸过，审查抓）
4. shutdown 不 notify_all（有工人在睡 → join 挂，timeout 抓）
5. shutdown 二次调用重复 join（std::terminate——驱动的幂等检查抓）
6. 析构不关停（线程还在跑、对象没了——UB/terminate，析构路径检查抓）
7. submit 的 closed 检查在锁外（竞态）或忘了抛
8. worker 取任务忘了 pop（同一任务被跑两遍——数量守恒抓）
9. 接口拼写第七查

## 模式点拨（判题时向用户点明）

- **任务队列 + 谓词等待 + 关停标志**：并发组件的标准三件套——
  线程池、消息总线、异步日志器（flush on shutdown）全是同一骨架
- **优雅关停模式**：关门 → 排干 → join——和生产系统"停止接收订单、
  处理存量、下线"是同一个仪式，波次 9 的网关重连直接复用
- **锁粒度纪律**：锁内改共享、锁外做工作——线程池性能的第一分水岭
- **毕业设计意味着什么**：16-20 五道题拼成一个可运行的整体——
  这就是波次 9 事件驱动引擎（#65）的雏形，你已经在造它了
