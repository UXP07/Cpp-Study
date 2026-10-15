#include <mutex>

/*
SR1. lock_guard 和 unique_lock 各自的定位？默认该用谁？unique_lock 多出来
     的能力有哪些（至少四个：延迟、中途开、可移动、限时/尝试），
     本题 transfer_safe 的 defer_lock 版本里 defer 是什么意思？
    1、lock_guard是最简单的RAII锁，只能构造上锁、析构解锁。unique_lock可以
       中途解锁、搭配条件变量、可移动、限时/尝试取锁——有这些需求时使用。
    2、默认使用lock_guard。
    3、多出的能力四件：延迟加锁、中途解锁/再锁、可移动、限时/尝试
      （try_lock_for）。
    4、defer_lock = "先建守卫不锁门"：构造时只占位不加锁，稍后手动 lock()
       或交给 std::lock 统一编排——给"一次拿多把"留出两把都未锁的起点，
       scoped_lock 就是它的糖衣。
SR2. 死锁的四个必要条件（白话各一句）？最容易破坏的是哪一个、怎么破坏？
     transfer_deadlock_prone 在什么并发场景下挂（具体到两个线程各自
     拿着什么等什么）？std::scoped_lock 靠什么不死锁？
    1、互斥=一次一人用；持有并等待=拿着旧的等新的；不可剥夺=抢不走；
       循环等待=等成一圈。
    2、最容易破坏循环等待：全程序统一锁序（如按地址）加锁，无环即无死锁。
    3、T1线程持有A的锁等待B的锁，而B的锁被T2持有、T2又在等A——互等成环。
    4、不是"原子地同时锁两把"（CPU没有这种指令）：scoped_lock 内部是
       std::lock 的试探-退让算法——逐个尝试加锁，任何一个失败就把已拿的
       全放掉、稍后重来；退让制造"把手里的锁还回去"的空隙，
       "持有并等待"被打破，死锁四条件缺一即死。
SR3. Counter 的 inc() 从进函数到退出，锁持有了多长？把"锁的粒度"说成
     一条纪律；单条 `v_++` 若只求原子不维护复合不变式，还有什么更轻的
     工具（预告下一题）？
    1、inc中lock_guard初始化上锁，inc结束解锁——持锁恰好覆盖一次++，最小。
    2、锁粒度=临界区的范围，两个维度：宽度（罩住多少共享状态——太少不够
       维持不变式，太多并发度归零）× 长度（持有多久——只做必须原子的，
       计算挪到锁外、IO/执行任务绝不在锁内）。纪律：宽度罩住不变式的
       最小范围，长度只做必须原子的。
    3、原子变量（std::atomic 的 fetch_add）——单条++只求原子不维护
       复合不变式时，免锁，指令级原子。
*/

class Counter {
private:
    mutable std::mutex m_;
    long v_ = 0;
public:
    void inc() {
        std::lock_guard<std::mutex> lk(m_);
        v_++;
    }

    long get() const
    {
        std::lock_guard<std::mutex> lk(m_);
        return v_;
    }

};

class Account{
private:
    std::mutex m_;
    long bal_  = 0;
public:
    void adjust(long ad) {
        bal_ += ad;
    }

    long balance() const {
        return bal_;
    }

    std::mutex& mutex() {
        return m_;
    }
};

void transfer_deadlock_prone(Account& from, Account& to, long amt)
{
    std::lock_guard<std::mutex> lk1(from.mutex());
    std::lock_guard<std::mutex> lk2(to.mutex());
    from.adjust(-amt);
    to.adjust(amt);
}

void transfer_safe(Account& from, Account& to, long amt)
{
    std::scoped_lock lk(from.mutex(), to.mutex());
    from.adjust(-amt);
    to.adjust(amt);
}

long total_of(Account& a, Account& b) {
    std::scoped_lock both(a.mutex(), b.mutex());
    return a.balance() + b.balance();
}

long init_runs = 0;
void init_once() {
    static std::once_flag flag;
    std::call_once(flag, [] { init_runs++; });
}


int main()
{
    return 0;
}