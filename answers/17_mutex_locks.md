# 练习 17 标准答案 —— mutex 与锁（知识点 #10）

## 参考实现

```cpp
// 练习 17 —— mutex 与锁：RAII 守卫、死锁与一次初始化
#include <mutex>

// Q1：线程安全计数器——mutex + lock_guard 正面解决"共享 +="
class Counter {
public:
    void inc() {
        std::lock_guard<std::mutex> g(m_);
        ++v_;
    }
    long get() const {
        std::lock_guard<std::mutex> g(m_);   // get 是 const，锁成员必须 mutable
        return v_;
    }
private:
    mutable std::mutex m_;
    long v_ = 0;
};

// Q2：无锁账户——线程安全由调用方（转账函数）负责
class Account {
public:
    void adjust(long amt) { bal_ += amt; }
    long balance() const { return bal_; }
    std::mutex& mutex() { return m_; }       // 转账要同时锁两把：暴露锁是常见妥协
private:
    std::mutex m_;
    long bal_ = 0;
};

// Q3a：死锁构造版——逐个加锁（判题端不调用：两向并发即互锁）
void transfer_deadlock_prone(Account& from, Account& to, long amt) {
    std::lock_guard<std::mutex> g1(from.mutex());
    std::lock_guard<std::mutex> g2(to.mutex());  // T1 持 from 等 to，T2 持 to 等 from
    from.adjust(-amt);
    to.adjust(amt);
}

// Q3b：修复版——scoped_lock 一次拿两把（内部试探-退让，任意顺序不死锁）
void transfer_safe(Account& from, Account& to, long amt) {
    std::scoped_lock both(from.mutex(), to.mutex());
    from.adjust(-amt);
    to.adjust(amt);
}

// 读总数也要在锁内看两把账本，否则转账中途读到撕裂的中间态
long total_of(Account& a, Account& b) {
    std::scoped_lock both(a.mutex(), b.mutex());
    return a.balance() + b.balance();
}

// Q4：call_once——多线程并发首调，初始化体恰好执行一次
inline long init_runs = 0;
inline void init_once() {
    static std::once_flag flag;
    std::call_once(flag, [] { ++init_runs; });
}

int main() {
    return 0;
}
```

## 判题驱动（判题端使用，放 build/tmp，判完删除；挂起由 timeout 判死刑）

```cpp
// drive17.cpp
#include <cstdio>
#include <thread>
#include <vector>
#define main user_main
#include "../../exercises/17_mutex_locks/main.cpp"
#undef main

static int fails = 0;
static int total = 0;
#define EXPECT(cond) do { ++total; if (!(cond)) { ++fails; \
    std::printf("[FAIL] line %d: %s\n", __LINE__, #cond); } } while (0)

int main() {
    // ===== Counter：8 线程 × 25000 =====
    {
        Counter c;
        std::vector<std::thread> ts;
        for (int t = 0; t < 8; ++t) {
            ts.emplace_back([&c] {
                for (int i = 0; i < 25000; ++i) c.inc();
            });
        }
        for (auto& t : ts) t.join();
        EXPECT(c.get() == 200000);
    }

    // ===== transfer_safe：8 线程双向高压，守恒 =====
    {
        Account a, b;
        a.adjust(100000);
        std::vector<std::thread> ts;
        for (int t = 0; t < 8; ++t) {
            ts.emplace_back([&, t] {
                for (int i = 0; i < 5000; ++i) {
                    if (t % 2 == 0) transfer_safe(a, b, 100);
                    else            transfer_safe(b, a, 100);
                }
            });
        }
        for (auto& t : ts) t.join();
        EXPECT(total_of(a, b) == 100000);   // 守恒（读也在锁内）
    }

    // ===== init_once：8 线程并发首调 =====
    {
        std::vector<std::thread> ts;
        for (int t = 0; t < 8; ++t) ts.emplace_back(init_once);
        for (auto& t : ts) t.join();
        EXPECT(init_runs == 1);
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

`[PASS] 3 checks passed`，退出码 0，多轮稳定；判题端以
`timeout 10` 运行，挂起 = 死锁/漏 notify，直接判 FAIL。

## 设计意图

- **Counter 是 16 号 SR4 的闭环**：同一道"共享 +="题，16 号定性为 UB、
  17 号用锁正面解掉——一题两知识点接力
- **Account 无锁 + 外部加锁**：转账的原子性横跨两个对象，内部加锁的
  Account 每个方法各自加锁救不了"转账中间态"——锁的边界必须覆盖
  **不变式的范围**，而不是对象的边界
- **死锁构造版不运行**：死锁的现场教学放进 SR（谁持什么等什么），
  代码审查看形状（两个逐次的 lock_guard）
- **total_of 也在锁内**：读一致性同样要锁——"只读不用锁"是另一个经典误区
- **inline 变量**（C++17）放全局 init_runs：14 号的知识顺手复用

## 评分要点

- Counter 精确 200000 稳定；get() 的 const + mutable 锁成员
- transfer_safe：守恒精确；scoped_lock（或 defer+std::lock）形状正确
- transfer_deadlock_prone 存在且逐个加锁、无调用路径
- init_once 恰好一次；total_of 类读法一致
- SR 三问要点齐全；零输出零警告；接口逐字符（第七查）

## 简答题要点（判题对照）

- SR1：默认 lock_guard（零状态、不可错用）；unique_lock 多出：延迟加锁
  （defer_lock——构造不锁，稍后 lock()）、中途 unlock()/再 lock()、
  可移动（转移所有权）、try_lock/try_lock_for（限时尝试）——代价是
  维护持有状态、体积更大。defer = "先建守卫不锁门，钥匙稍后用"
- SR2：四条件——互斥（一次一人用）、持有并等待（拿着旧的等新的）、
  不可剥夺（抢不走）、循环等待（形成环）。最易破坏：**循环等待**——
  全程序统一锁序（如按地址排序）就无环。场景：T1 transfer(a,b) 持 a 等 b，
  T2 transfer(b,a) 持 b 等 a——互等成环。scoped_lock 靠"试探-退让"
  （std::lock 算法：逐个 try，失败全放重来）打破占有并保持
- SR3：持锁长度 = 从进 inc 到 return 的整个临界区（这里恰好等于整个
  函数）。纪律：**锁粒度覆盖不变式、不覆盖昂贵操作**——锁内只做必须
  原子的事。单条 ++ 只要原子性：`std::atomic<long>` 的 fetch_add——
  一条指令级原子，免锁（19 号正课）

## 常见错误预测（判题重点检查）

1. Counter 忘 mutable 锁成员（get() const 编不过 → 去掉 const 糊弄——
   审查抓）
2. transfer_safe 里先 lock_guard(from) 再 lock_guard(to)（那就是死锁版
   换了个名——守恒压测当场挂给看：timeout 判死刑）
3. scoped_lock 传错（传了 Account 而不是 mutex()）
4. 读总数不加锁（守恒断言可能撞上转账中间态——偶发 FAIL）
5. init_once 的 once_flag 放成函数外全局共享（功能对但形状差）或
   每次调用新建（call_once 失效 → init_runs == 8）
6. lambda 捕获 t 引用捕获（16 号刚练过的坑，复检）
7. 接口拼写第七查

## 模式点拨（判题时向用户点明）

- **RAII 守卫三件套**：lock_guard（默认）/ unique_lock（要动锁时）/
  scoped_lock（要一次拿多把时）——选型表
- **锁的边界 = 不变式的范围**：转账跨对象，锁就得跨对象——
  "给每个类塞一把锁"治不了跨对象不变式
- **统一锁序 / 一次全拿**：多锁的两条活路，scoped_lock 是 C++17 的
  正解；Go 的按地址排序是同一思想
- **call_once / magic static**：C++11 起静态局部变量初始化本身线程安全
  （编译器帮你 call_once）——两者选一，别双保险
