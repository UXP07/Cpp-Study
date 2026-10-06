#include <memory>
#include <utility>
/**
SR1. 选用判据：什么时候用 unique_ptr / shared_ptr / weak_ptr / 裸指针+引用？
     给一张"所有权判据表"（含：函数参数只想用不想拥有时选什么）。
答：1、unique_ptr —— 所有权唯一、无人共享（默认首选，零开销）
    2、shared_ptr —— 生命周期需要多个持有者共享（有代价：控制块 + 计数）
    3、weak_ptr   —— 只观察不持有：打破循环引用、可探测对方死活（lock）
    4、裸指针/引用 —— 不管理生命周期的"借用"。最高频场景是函数参数
       "只用不拥有"：传 const T& 或裸指针/引用，签名不产生所有权暗示；
       要吞下所有权才收智能指针（unique_ptr 按值 = 独吞，shared_ptr = 入伙）
SR2. 计数为什么不能是 Shared<T> 的普通成员？它必须满足"被所有副本共享"，
     这和练习 03 浅拷贝观察到的现象是同一个原理吗？拷贝和移动对计数各做什么，
     为什么？`make_shared` 比先 new 再构造 shared_ptr 好在哪？
答：1、普通成员会被每个副本各拷一份——每个副本都有自己的计数，各自从自己
       那份算起，release 时各删各的 → double free。计数必须放堆上：
       生命周期独立于任何副本，被所有副本共享同一个。
    2、是同一个原理：练习 03 浅拷贝 = 指针被共享但没人协调（无意的共享，
       事故）；Shared = 有意共享资源 + 用共享计数协调释放（受控的设计）。
       事故与设计的分界在"有没有协调机制"。
    3、计数衡量的是"持有者人数"：拷贝 = 多了一个持有者 → +1；
       移动 = 持有者换人、人数不变 → 不动。
    4、make_shared 把对象和控制块合并成一次堆分配（省一次分配、
       同生同死缓存局部性好）；shared_ptr<T>(new T) 是两次分配。
SR3. 循环引用为什么让 shared_ptr 的计数永远到不了 0？weak_ptr 是怎么拆掉环的？
     为什么用之前必须 lock()，lock 返回什么？
答：1、不是死锁（没人阻塞），是计数互锁泄漏：环上每条边都是 shared，
       每个节点的计数都被对方 +1——外部引用全部撤走后计数仍互锁在 1，
       谁也到不了 0，内存泄漏。
    2、weak 不参与 shared 计数，把环上的"持有边"减到不构成回路。
       weak 有自己的计数，两层死法：shared 计数归 0 → 对象先亡；
       weak 计数归 0 → 控制块才亡。
    3、所指对象可能已死，直接用会悬垂。lock() 原子地换一个临时 shared：
       活着 → 拿到引用且期间计数 +1（使用窗口内对象不许死）；
       已死 → 空 shared_ptr。lock 不阻塞，只是试探。
SR4. 拷贝赋值里"先释放自己旧份额、再共享对方"的顺序能不能反？
     自赋值走不走这条路径，为什么安全？
答：1、不能反。死因：先接管、后放旧——当新旧份额指向同一资源（或旧份额
       恰是最后一份）时，release 旧份额会把刚接上的资源 delete，
       新指针瞬间悬垂；先放旧再共享，则任何时刻至多持有一份合法资源。
    2、自赋值不走这条路径：this != &other 直接跳过，计数不动——
       这个 guard 就是自赋值安全的全部。

 */

class Widget{
private:
    int v_;
    inline static int live_ = 0;
public:
    explicit Widget(int v) : v_(v) {live_++;}
    ~Widget() { live_--; }
    Widget(const Widget& other) = delete;
    Widget& operator=(const Widget& other) = delete;
    int value() const { return v_; }
    static int live() { return live_; }
};

class Node{
private:
    std::shared_ptr<Node> next_;
    std::weak_ptr<Node> prev_;
    inline static int live_ = 0;
public:
    Node() { live_++; }
    ~Node() { live_--; }
    static int live() { return live_; }

    void set_next(std::shared_ptr<Node> n) { next_ = std::move(n); }
    void set_prev(std::weak_ptr<Node> p) { prev_ = std::move(p); }

    std::shared_ptr<Node> next() const { return next_; }
    std::shared_ptr<Node> prev() const { return prev_.lock(); }
};

template <class T>
class Shared{
public:
    Shared() = default;
    explicit Shared(T* p) : ptr_(p), count_(p!=nullptr ? new long(1) : nullptr) {}
    Shared(const Shared& other) : ptr_(other.ptr_), count_(other.count_) {
        if(count_ != nullptr)
            ++*count_;
    }

    Shared(Shared&& other) noexcept : ptr_(other.ptr_), count_(other.count_) {
        other.ptr_ = nullptr;
        other.count_ = nullptr;
    }

    Shared& operator=(const Shared& other) {
        if(this != &other)
        {
            release();
            ptr_ = other.ptr_;
            count_ = other.count_;
            if(count_ != nullptr) ++*count_;
        }
        return *this;
    }

    Shared& operator=(Shared&& other) noexcept {
        if(this != &other)
        {
            release();
            ptr_ = other.ptr_;
            count_ = other.count_;

            other.ptr_ = nullptr;
            other.count_ = nullptr;

        }
        return *this;
    }

    ~Shared() { release(); }

    T* get() const { return ptr_; }
    long use_count() const { return count_ == nullptr ? 0 : *count_; }
    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }


private:
    T* ptr_ = nullptr;
    long* count_ = nullptr;
    
    void release(){
        if(count_ == nullptr) return;
        if(--*count_ == 0)
        {
            delete count_;
            delete ptr_;
        }
        ptr_ = nullptr;
        count_ = nullptr;
    }
};


int main()
{
    return 0;
}