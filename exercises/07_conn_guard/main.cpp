#include <string>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <cstring>

/**
SR1. Conn 保留拷贝、ScopedLock 删除拷贝——各自的判据是什么？
     给出"一个资源类该不该禁拷贝"的通用判断准则。
答：1、判据 = 问"拷贝意味着什么"。拷贝 Conn = 向服务器再要一条连接
       （重新获取一份资源，语义成立 → 深拷贝，计数 +1）；
       拷贝 ScopedLock = 两个对象都认为该由自己 unlock 同一把锁
       （"持有状态"不可共享，语义荒谬 → =delete）。
    2、准则：说得通就拷，荒谬就删；性能敏感时即使说得通，也可禁拷贝只留移动。
SR2. 统一赋值 `operator=(Conn)` 按值收参，为什么能同时覆盖拷贝赋值和移动赋值？
     重载决议发生在哪一步？它为什么天然不需要自赋值特判，代价是什么？
答：1、按值形参的初始化就是重载决议现场：实参是左值 → 形参被拷贝构造；
       实参是右值 → 形参被移动构造。之后函数体只有一份，两种赋值共用。
    2、自赋值天然正确：操作对象是实参的副本，和自己的副本 swap，内容不变。
    3、代价：自赋值时白付一次完整深拷贝——用多一次拷贝换"永远正确 + 强异常保证"。
SR3. copy-and-swap 的强异常保证来自哪里？
     把"可能抛异常的操作"和"修改自身的操作"的先后关系说清楚。
答：1、所有可能抛异常的操作（形参的构造：new + 容量检查）都发生在进入函数体
       之前的调用点；函数体内只剩 swap——交换指针和整数，绝不抛。
    2、因此抛异常时自身一字未动（强保证）；进了函数体就不可能再抛。
       保证来自顺序：会抛的全在改自己之前，改自己的操作（swap）不会抛。
SR4. 为什么移动构造在容量满时也必须成功、且 live 计数不变？
     这与移动操作标 noexcept 有什么内在联系？
答：1、移动 = 所有权过户，不向服务器要新东西，与容量无关，所以计数不变。
    2、它不分配、不锁定、不查容量——只交换已有指针。没有获取就没有失败，
       没有失败才有资格承诺 noexcept。noexcept 是"无获取"的收据。
 */


class Conn{
private:
    char* ctx_;
    size_t size_;
    inline static int live_ = 0;
    inline static int cap_ = 1 << 30;
public:

    Conn(const std::string& endpoint) : ctx_(nullptr), size_(0) {
        if(live_ >= cap_)
        {
            throw std::runtime_error("capcity has been exceeded!");
        }
        size_ = endpoint.size();
        ctx_ = new char[size_];
        std::memcpy(ctx_, endpoint.c_str(), size_);
        live_++;
    }

    Conn(const Conn& other) : ctx_(nullptr), size_(0) {
        if(live_ >= cap_)
        {
            throw std::runtime_error("capcity has been exceeded in copy!");
        }
        size_ = other.size_;
        ctx_ = new char[size_];
        std::memcpy(ctx_, other.ctx_, size_);
        live_++;
    }

    Conn(Conn&& other) noexcept : ctx_(other.ctx_), size_(other.size_) {
        other.ctx_ = nullptr;
        other.size_ = 0;
    }

    Conn& operator=(Conn other) noexcept
    {
        swap(other);
        return *this;
    }

    void swap(Conn& other) noexcept
    {
        std::swap(ctx_, other.ctx_);
        std::swap(size_, other.size_);
    }



    static int live() { return live_; }
    static void set_capacity(int n) { cap_ = n; }
    std::string endpoint() const { return std::string(ctx_, size_); }

    ~Conn()
    {
        if(ctx_ != nullptr)
        {
            delete[] ctx_;
            live_--;
        }
    }

};

class ScopedLock{
private:
    std::mutex* m_;
    bool locked_;
public:
    explicit ScopedLock(std::mutex& lock) : m_(&lock), locked_(true) { m_->lock(); }
    
    ScopedLock(const ScopedLock& other) = delete;
    ScopedLock& operator=(const ScopedLock& other) = delete;

    ScopedLock(ScopedLock&& other) noexcept : m_(other.m_), locked_(other.locked_) {
        other.m_ = nullptr;
        other.locked_ = false;
    }

    ScopedLock& operator=(ScopedLock&& other) noexcept
    {
        if(this != &other)
        {
            release();
            m_ = other.m_;
            locked_ = other.locked_;
            other.m_ = nullptr;
            other.locked_ = false;
        }
        return *this;
    }

    bool holds() const { return m_ != nullptr && locked_; }

    ~ScopedLock()
    {
        release();
    }

private:
    void release() {
        if(m_ != nullptr && locked_)
        {
            m_->unlock();
        }
        m_ = nullptr;
        locked_ = false;
    }


};







int main()
{
    return 0;
}