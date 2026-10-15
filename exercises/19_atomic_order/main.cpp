#include <atomic>

/*
SR1. 六种内存序各自一句话（白话表：relaxed=只原子不管顺序；
     acquire=读闸门；release=写闸门；acq_rel=读写双向闸门；
     seq_cst=全局统一时间线+默认；consume 实际当 acquire 用）。
     默认是哪档、为什么编译器不帮你挑更快的？
    1、六种内存序白话表：
       relaxed——只保证这一条是原子的，不管任何顺序；它不管同步，
                 所以得自己确保不需要同步（独立计数才敢用）。
       acquire——读闸门：本线程之后的读写不能提前到它前面；
                 配对读到后，写端 release 之前的一切写对它可见（跨线程可见性）。
       release——写闸门：本线程之前的读写不能挪到它后面；
                 与 acquire 配对完成"数据发布"。
       acq_rel——读改写操作的合体闸（fetch_add 常用）：读的半边是
                 acquire、写的半边是 release。
       seq_cst——所有 seq_cst 操作存在全程序公认的一条总顺序：
                 不光两两配对，全世界共用一条时间线。
       consume——数据依赖版 acquire，实践当 acquire 用。
    2、默认 seq_cst，保证安全——编译器不替你挑更快的：挑错 = 低概率
       数据损坏，安全的默认 > 聪明的默认。
SR2. release/acquire 配对：写端 publish 里两条语句的顺序能换吗——换了
     会出什么事故（消费者看到旗子却读到旧 data）？两边都用 relaxed
     会怎样？为什么说这对配对是"单生产者-单消费者的最小同步"？
    1、不能，release只保证上方的操作顺序，如果换了可能会产生读旧值。
    2、原子性还在（旗子不会半真半假），但顺序和可见性保证全无——
       消费者过门时货可能还没落到它能看见的位置，低概率拿到旧值。
    3、因为不牵扯别的同步设施，只要 release 上方和 acquire 下方这一对，
       一个 atomic bool + 一条数据就完成了完整的"数据发布"——
       单生产者-单消费者的最小同步。
SR3. 什么时候 relaxed 就够（独立计数、统计打点）？为什么说"默认
     seq_cst、profile 说话之前不降级"是工程纪律？内存序 bug 的
     复现率有什么特点（低概率、难复现——所以靠什么防：靠模式而不是
     靠测试）？
    1、计数或什么不牵扯同步的事情（统计打点、指标采样）。
    2、降级省的是纳秒、赌上的是低概率数据损坏——没有 profile 证据
       就不值得冒这个险，所以默认 seq_cst 上线、证明是瓶颈再降。
    3、复现率极低：千分之一甚至更低、随负载/核数/编译器版本漂移
      （重排确实发生在 CPU 流水线/缓存层——方向没错）。工程结论：
       测试防不住，防线是模式正确 + 代码评审。
*/

class Channel{
private:
    int data_;
    std::atomic<bool> ready_ = false;
public:
    void publish(int v) {
        data_ = v;
        ready_.store(true, std::memory_order_release);
    }

    int consume() {
        while (!ready_.load(std::memory_order_acquire))
        {
        }
        return data_;
    }
};

class RelaxCounter{
private:
    std::atomic<int> counter_ = 0;
public:
    void bump() {
        counter_.fetch_add(1, std::memory_order_relaxed);
    }

    int get() const {
        return counter_.load(std::memory_order_relaxed);
    }
};


int main(){
    return 0;
}