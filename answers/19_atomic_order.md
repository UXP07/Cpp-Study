# 练习 19 标准答案 —— atomic 与内存序（知识点 #12）

## 参考实现

```cpp
// 练习 19 —— atomic 与内存序：release/acquire 配对
#include <atomic>

// Q1：单生产者-单消费者的最小同步——载荷先落、旗子后竖
class Channel {
public:
    void publish(int v) {
        data_ = v;                                        // 载荷先行
        ready_.store(true, std::memory_order_release);    // 闸门：此前的写全部对读者可见
    }

    int consume() {
        while (!ready_.load(std::memory_order_acquire)) { // 配对闸门：过门后写端的一切可见
            // 自旋：等待是短暂时最便宜的方式
        }
        return data_;
    }

private:
    int data_ = 0;
    std::atomic<bool> ready_{false};
};

// Q2：独立计数——只求原子性、不携带依赖数据，relaxed 足够
class RelaxCounter {
public:
    void bump() { v_.fetch_add(1, std::memory_order_relaxed); }
    long get() const { return v_.load(std::memory_order_relaxed); }

private:
    std::atomic<long> v_{0};
};

int main() {
    return 0;
}
```

## 判题驱动（判题端使用，放 build/tmp，判完删除；timeout 判死刑）

```cpp
// drive19.cpp
#include <cstdio>
#include <thread>
#include <vector>
#define main user_main
#include "../../exercises/19_atomic_order/main.cpp"
#undef main

static int fails = 0;
static int total = 0;
#define EXPECT(cond) do { ++total; if (!(cond)) { ++fails; \
    std::printf("[FAIL] line %d: %s\n", __LINE__, #cond); } } while (0)

int main() {
    // ===== Channel：1000 轮发布-消费，值必须逐轮配对 =====
    {
        int mismatch = 0;
        for (int round = 0; round < 1000; ++round) {
            Channel ch;
            int payload = round * 7 + 1;
            std::thread reader([&ch, payload, &mismatch] {
                if (ch.consume() != payload) ++mismatch;
            });
            ch.publish(payload);
            reader.join();
        }
        EXPECT(mismatch == 0);
    }

    // ===== RelaxCounter：8 线程 × 10000 =====
    {
        RelaxCounter c;
        std::vector<std::thread> ts;
        for (int t = 0; t < 8; ++t) {
            ts.emplace_back([&c] {
                for (int i = 0; i < 10000; ++i) c.bump();
            });
        }
        for (auto& t : ts) t.join();
        EXPECT(c.get() == 80000);
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

`[PASS] 2 checks passed`，退出码 0，多轮稳定；`timeout 10` 运行。
注意：内存序错误**不保证**在压测里现形（低概率 UB）——判题以代码审查
为主、压测为辅。

## 设计意图

- **publish 的两条语句顺序是全部考点**：data_ 先写、release 后竖——
  顺序换掉（或全 relaxed）在弱内存序机器上就是"旗子立了货没到"。
  x86 强内存序掩盖这个问题，但代码要面向标准写而不是面向 x86 写
- **1000 轮新实例**：排除复用残留，每轮都是干净的生产-消费对——
  压测有牙齿，但牙齿有限（见上），审查是主力
- **RelaxCounter 用 atomic<long>**：17 号 Counter 的免锁版对照——
  同一个"多线程计数"需求，锁和原子的取舍第一次摆上桌
- **自旋等待**：consume 忙等的正当性 = 等待极短（对端马上 publish）；
  长等待该用 18 号的条件变量——两种等待各自的适用面

## 评分要点

- publish：载荷先写 + release 后竖，顺序不可换（审查）
- consume：acquire 配对 + 自旋；返回 data_（不是重新读旗子）
- RelaxCounter：显式 relaxed 的 fetch_add/load
- 压测全绿稳定；零输出零警告；接口逐字符（第七查）
- SR 三问要点齐全（六种内存序表是验收硬指标）

## 简答题要点（判题对照）

- SR1：**relaxed**=只保证这一条是原子的，不管别的任何顺序；
  **acquire**=读闸门（配对读到后，写端此前的一切写可见）；
  **release**=写闸门（这条写之前的一切写，对配对读者可见）；
  **acq_rel**=同一操作既是读闸又是写闸（fetch_add 常用）；
  **seq_cst**=全体 seq_cst 操作共享一条全世界公认的时间线（默认档，
  最安全最贵）；**consume**=数据依赖版 acquire，实践中当 acquire 用。
  默认 seq_cst——编译器不替你挑更快的，因为挑错 = 低概率数据损坏，
  安全的默认值比聪明的默认值重要
- SR2：不能换。换成"先竖旗后写货"：读者 acquire 过门后看到的可能是
  **旧 data_**（旗立了货没到）。全 relaxed：原子性还在（旗不会半红半绿）
  但顺序保证全无——读者过门时货可能还没落到它能看见的位置，
  低概率拿到旧值。这对配对是"单生产-单消费最小同步"：一个 atomic bool
  + 一条数据，就把"数据完整发布"做成了无锁的
- SR3：relaxed 够用的判据——这个计数**不携带其他数据的发布**，纯独立
  统计（打点、指标、采样数）。工程纪律：默认 seq_cst 上线，profile
  证明某条是瓶颈且语义允许时再降级——因为内存序 bug 复现率极低
  （千分之一甚至更低、和负载/核数/编译器版本相关），**测试防不住，
  只能靠模式正确**：配对关系写进代码评审清单

## 常见错误预测（判题重点检查）

1. publish 顺序写反（先 store 后写 data_）——x86 上测不出来，审查抓
2. 全部用默认档/relaxed 偷懒（违反"显式内存序"约束）
3. consume 返回前重新 load 旗子当数据返回（返回的是 bool 不是 data_）
4. RelaxCounter 用锁实现（违反禁 mutex）或用 fetch_add 默认档（未显式）
5. ready_ 初始化漏 {false}（NSDMI 老//#账）
6. 自旋里没有退出条件写反（!ready_ 写成 ready_ → 直接跳过等待读旧值）
7. 接口拼写第七查

## 模式点拨（判题时向用户点明）

- **发布模式**：data 写好 → flag release 竖起；读者 acquire 等旗——
  SPSC 无锁队列（波次 9 的 #55）的婴儿形态
- **内存序选型表**：独立计数 relaxed；数据发布 release/acquire；
  拿不准 seq_cst——"从最贵开始，用证据降级"
- **测试防不住内存序 bug**：低概率 UB 的防线是模式与评审，不是压测——
  这是并发和单线程工程方法论的分水岭
- **atomic vs mutex 取舍**：单变量原子操作用 atomic（指令级）；
  复合不变式（跨多变量）用锁——17 号和 19 号合起来就是完整答案
