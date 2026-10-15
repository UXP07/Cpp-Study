#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

/*
SR1. 虚假唤醒是什么（谁允许的、为什么标准拿它没办法）？
     为什么裸 wait（不带谓词）醒来必须 while 重查、而谓词版不用你写循环
     ——谓词版帮你做了什么？
    1、条件变量的操作系统底层实现允许"没人 notify 也会醒"——标准干脆把
       虚假唤醒合法化，因此用法上必须防御它。
    2、醒来 ≠ 条件成立：while 重查，不成立接着睡——逻辑上免疫假醒。
    3、谓词版 wait 就是标准库替我写的这个 while 循环：醒来自动重查谓词，
       假醒自动睡回去。
SR2. 丢失唤醒：notify 发生在对方进入 wait **之前**会怎样（裸版 vs 谓词版
     各走一遍）？为什么条件变量必须配 mutex——wait 的"原子放锁+睡"解决
     了什么（如果不原子，检查条件和睡觉之间的缝隙里来了通知会怎样）？
    1、没有沉睡等待的 wait，提醒凭空消失：裸版随后进入 wait 永睡；
       谓词版进来先查条件，已满足直接走——所以谓词版免疫丢失唤醒。
    2、和 SR1 不重复：SR1 治"假醒"（没人叫却醒了），这里治"缝里漏"
      （叫了但没接到）。如果"检查条件"和"睡下"是两步，中间的缝里来了
       notify 就是白发：T1 检查完条件放锁、还没睡下的瞬间，T2 的 notify
       打到空气上，T1 随后睡死。wait 的"原子放锁+睡"把两步焊成一步：
       锁放开的同一瞬间人已睡下，无缝可漏——这就是条件变量必须配
       mutex 的完整答案：mutex 不只是保护队列，它是唤醒不丢的结构保证。
SR3. 关停语义：消费者的等待谓词为什么必须是"非空 **或** 已关闭"？
     close 时为什么必须 notify_all——只 notify_one 会把哪些线程留在梦里？
     优雅关停（收完再关）和立即关停（丢弃余货）各适合什么场景？
    1、只等"非空"：关门时队列已空、消费者在睡，永远等不到唤醒（挂死）；
       只等"已关闭"：余货被丢。所以谓词是"非空或已关"：非空醒来干活，
       空且关才收工。
    2、notify_one 每次只叫醒一个：多个消费者在睡时，其余的留在梦里
       永远等不到下一次 notify——所以关停（状态变更）必须 notify_all
      （数据可用才叫一个）。
    3、优雅关停（收完再关）：正常收尾——交易收尾、日志 flush；
       立即关停（丢弃余货）：紧急状态——超时熔断、取消计算。
*/

template <class T>
class TsQueue{
private:
    std::deque<T> que_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    bool closed_ = false;
     
public:
    void push(const T& v) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if(closed_) {
                throw std::runtime_error("queue has closed.");
            }
            que_.push_back(std::move(v));
        }
        cv_.notify_one();
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lk(mtx_);
        cv_.wait(lk, [this]{ return !this->que_.empty() || this->closed_;});
        if(que_.empty()) {
            return std::nullopt;
        }
        T v = std::move(que_.front());
        que_.pop_front();
        return v;
    }

    bool try_pop(T& v) {
        std::lock_guard<std::mutex> lk(mtx_);
        if(que_.empty()) {
            return false;
        }
        v = std::move(que_.front());
        que_.pop_front();
        return true;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            closed_ = true;
        }
        cv_.notify_all();

    }

    bool closed() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return closed_;
    }
    
    std::size_t size() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return que_.size();
    }

};

int main()
{
    return 0;
}