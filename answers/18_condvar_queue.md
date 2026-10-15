# 练习 18 标准答案 —— 条件变量（知识点 #11）

## 参考实现

```cpp
// 练习 18 —— 条件变量：谓词等待与优雅关停
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

template <typename T>
class TsQueue {
public:
    void push(T v) {
        {
            std::lock_guard<std::mutex> g(m_);
            if (closed_) {
                throw std::runtime_error("push on closed queue");
            }
            q_.push_back(std::move(v));
        }
        cv_.notify_one();       // 通知放锁外：被唤醒者别一睁眼就撞墙
    }

    // 阻塞版：谓词 = 有货 或 关门——防虚假唤醒/丢失唤醒的同一副药
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [this] { return !q_.empty() || closed_; });
        if (q_.empty()) {
            return std::nullopt;            // 走到这说明 closed_ 为真且无货
        }
        T v = std::move(q_.front());
        q_.pop_front();
        return v;
    }

    bool try_pop(T& out) {
        std::lock_guard<std::mutex> g(m_);
        if (q_.empty()) {
            return false;
        }
        out = std::move(q_.front());
        q_.pop_front();
        return true;
    }

    void close() {
        {
            std::lock_guard<std::mutex> g(m_);
            closed_ = true;
        }
        cv_.notify_all();       // 关停必须叫醒所有睡着的消费者
    }

    bool closed() const {
        std::lock_guard<std::mutex> g(m_);
        return closed_;
    }
    std::size_t size() const {
        std::lock_guard<std::mutex> g(m_);
        return q_.size();
    }

private:
    mutable std::mutex m_;
    std::condition_variable cv_;
    std::deque<T> q_;
    bool closed_ = false;
};

int main() {
    return 0;
}
```

## 判题驱动（判题端使用，放 build/tmp，判完删除；timeout 判死刑）

```cpp
// drive18.cpp
#include <atomic>
#include <cstdio>
#include <optional>
#include <thread>
#include <vector>
#define main user_main
#include "../../exercises/18_condvar_queue/main.cpp"
#undef main

static int fails = 0;
static int total = 0;
#define EXPECT(cond) do { ++total; if (!(cond)) { ++fails; \
    std::printf("[FAIL] line %d: %s\n", __LINE__, #cond); } } while (0)

int main() {
    // ===== try_pop 空队列：立即 false =====
    {
        TsQueue<int> q;
        int out = -1;
        EXPECT(!q.try_pop(out) && out == -1);
    }

    // ===== push after close 抛异常 =====
    {
        TsQueue<int> q;
        q.close();
        bool threw = false;
        try { q.push(1); } catch (const std::runtime_error&) { threw = true; }
        EXPECT(threw);
    }

    // ===== 多生产多消费：4 生产 × 1000，2 消费 =====
    {
        TsQueue<long long> q;
        std::vector<std::thread> ts;
        for (int p = 0; p < 4; ++p) {
            ts.emplace_back([&, p] {
                for (int i = 1; i <= 1000; ++i) {
                    q.push((long long)(p + 1) * 1000000 + i);   // 编码可解码
                }
            });
        }
        std::atomic<long long> sum{0};
        std::atomic<int> items{0};
        for (int c = 0; c < 2; ++c) {
            ts.emplace_back([&] {
                while (auto v = q.pop()) {
                    sum += *v % 1000000;        // 解码 i 部分
                    items += 1;
                }
            });
        }
        for (int p = 0; p < 4; ++p) ts[p].join();   // 生产者收工
        q.close();                                   // 关门
        for (unsigned i = 4; i < ts.size(); ++i) ts[i].join();

        long long expect_sum = 0;
        for (int i = 1; i <= 1000; ++i) expect_sum += i;
        EXPECT(items == 4000);                       // 无丢失无重复
        EXPECT(sum == 4 * expect_sum);               // 总和精确
        EXPECT(q.size() == 0);
        long long out;
        EXPECT(!q.try_pop(out));
    }

    if (fails == 0) {
        std::printf("[PASS] %d checks passed\n", total);
    } else {
        std::printf("[FAIL] %d/%d failed\n", fails, total);
    }
    return fails == 0 ? 0 : 1;
}
```

## 预期结果

`[PASS] 6 checks passed`，退出码 0，多轮稳定；`timeout 10` 运行，
挂起 = 裸 wait 或漏 notify，判 FAIL。

## 设计意图

- **谓词是唯一的解药**：虚假唤醒（OS 允许无通知而醒）和丢失唤醒
  （通知先于等待）症状不同、药方相同——醒来重查条件，条件在锁内检查
- **值编码解码**：producer 把身份编进数值（p*1e6+i），消费者解码统计——
  不但验总和，还验**条数**（无丢失无重复），压测有牙齿
- **close 的语义三角**：push 抛、pop 排干后给 nullopt、try_pop 照常——
  关停不是一刀切，各操作有各自的体面
- **notify_one vs notify_all 的分界**：入队叫一个（多叫浪费），
  关门叫全体（漏叫 = 永久睡眠）——判题代码审查
- **原子 sum/items 在驱动里**：判题驱动自己也要正确的并发统计——
  用 19 号的工具（此时用户还没学，驱动先用了，讲评时点破）

## 评分要点

- pop 谓词 wait（裸 wait 直接 FAIL）；谓词 = 非空或已关
- 关停三语义全对：push 抛 / pop 排干后 nullopt / close 唤醒全体
- try_pop 非阻塞；多生产多消费 4000 条无丢失、总和精确、多轮稳定
- push 的检查与入队在同一锁内（否则 close 竞态：检查未关、入队前被关）
- SR 三问要点齐全；零输出零警告；接口逐字符

## 简答题要点（判题对照）

- SR1：虚假唤醒 = 条件变量允许在没有任何 notify 的情况下醒来——
  POSIX 接口的历史包袱，标准干脆把它合法化。裸 wait 醒来必须
  while 重查（醒 ≠ 条件成立）；谓词版 wait 就是标准库替你写的这个
  while 循环——醒来自动重查谓词，假醒直接睡回去
- SR2：丢失唤醒：T1 检查条件不满足 →（还没睡）→ T2 改条件并 notify
  （没人听）→ T1 睡过去——通知落空，永睡。裸版会永睡；谓词版醒来先
  查条件，条件已成立直接走。必须配 mutex：条件本身是共享状态，检查
  要持锁；wait 的"原子放锁 + 睡"消灭了"检查完、还没睡"的缝隙——
  不原子的话通知恰好落在缝隙里 = 白发
- SR3：只等"非空"：close 后队列已空、消费者还在睡，shutdown 永远
  等不到它们。只等"关停"：余货被丢（违约）。所以谓词是"非空或已关"，
  且醒来先处理货、货空且关才收工。close 必须 notify_all：消费者
  有多个，notify_one 每次只叫一个，剩下的留在梦里。优雅关停适合
  "收工前提是活干完"（交易收尾、日志 flush）；立即关停适合超时熔断、
  取消计算——两种都是工程常态，本题实现前者

## 常见错误预测（判题重点检查）

1. 裸 `cv_.wait(lk)`（无谓词）——偶发挂死，审查直接判 FAIL
2. push 的 closed 检查在锁外（竞态窗口）或检查后忘抛
3. close 忘 notify_all（漏一个消费者 → 挂起，timeout 抓）
4. pop 返回谓词写 `!q_.empty()`（少了 || closed_）——关停语义错
5. wait 用 lock_guard 而非 unique_lock（wait 要求可解锁——编不过，
   编译器现场教）
6. try_pop 拿错了锁的形状（比如也 wait 了）
7. push 后忘 notify（消费者永睡——timeout 抓）
8. 接口拼写第七查

## 模式点拨（判题时向用户点明）

- **谓词等待三连**：条件变量 + mutex + 谓词——少一样都是错的；
  记公式 `cv.wait(lk, pred)` = `while (!pred()) cv.wait(lk);`
- **优雅关停模式**：关门标志 + notify_all + "排干后退出"——
  20 号线程池的关停就是本类原样放大
- **notify_one/all 分界**：数据可用叫一个，状态变更叫全体
- **队列 = 锁 + 谓词 + 容器**：这个三件套是并发世界的乐高积木——
  线程池、消息总线、后台 flush 全是它换皮
