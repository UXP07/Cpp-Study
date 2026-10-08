#include <cstddef>
#include <cstring>
#include <memory>
#include <utility>

/**
SR1. 所有权判据表默写（unique / shared / weak / 裸指针引用四行 + 函数参数惯例），
     并说明本题 Part B 三个构造签名分别对应哪条判据、为什么按值接收。
答：判据表（这份资源该由谁管）：
    1、unique_ptr —— 这份资源从头到尾只有一个主人（默认首选，零开销）
    2、shared_ptr —— 这份资源需要好几个主人一起养，最后一个走的负责关灯
       （代价：控制块 + 计数）
    3、weak_ptr —— 只想看看它、不想养它：不增加计数，能打听死活（lock）
    4、裸指针/引用 —— 纯粹"借用"，不管理生死。最常用在函数参数：
       函数只是用一下、不打算拥有，就传 const T& 或裸指针/引用——
       看签名就知道它不会抢你的东西；要收下所有权才收智能指针。
    Part B 对应：Owner 收 unique_ptr（按值）＝"我要独吞"——按值传是所有权
    转移的唯一写法，按引用传就可能是假转移；Subscriber 收 shared_ptr
    （按值）＝"我要入伙"——拷进来计数 +1；Observer 收 weak_ptr（按值）＝
    "我只看看"——拿到的是观察牌不是持有权。签名本身就是合同。
SR2. "先放旧再共享"的死因是什么（默写具体场景）？你的 `reset()` 实现里，
     这条纪律体现在哪一行？`reset()` 与析构函数是什么关系？
答：1、死因走一遍（a、b 共享同一份消息，use_count=2，执行 a = b）：
    （1）先接管：a 的 ptr_/count_ 被改成 b 的那份——a 原来旧份额的指针
         在这一步被覆盖，从此再也没人能释放它（旧份额是独占资源 → 泄漏）；
    （2）后 release：此刻减的是"刚接上的"那份计数：2→1，看似没事——
         但真实持有者是 2 个，计数却只剩 1，计数和人数脱钩；
    （3）若两个对象恰好独占同一份（a=b 自赋值且没 guard）：1→0 →
         把刚接上的资源 delete → 手里的指针当场悬垂。
    正确顺序下：release 减的是自己的旧份额（别人还拿着，安全），
    然后才接新的——任何时刻都不存在"刚接上的东西被自己亲手删掉"。
    2、reset() 的体现：函数体只有一行 release()——"先放旧"就是它的全部
    动作：先处置旧我，不碰任何新东西。
    3、reset 与析构走同一条释放路径：reset = 对象还活着，提前手动放份额
    （回到空）；析构 = 对象死亡时自动做同一件事。共用一份代码 =
    "每个资源恰好死一次"由结构保证，不靠每个调用点记得。
SR3. 一个句柄类的"空状态"要维持哪些性质？列出你能想到的全部"从空出发/
     到达空"的操作，并说明空状态不变式为什么是"漏判空"类 bug 的总疫苗。
答：术语先翻译："从空出发"＝对象已经是空的时候，还能对它做什么；
    "到达空"＝哪些操作会把对象变成空。
    1、空状态的性质：ptr_ 和 count_ 必须同时为空、绑定成一个单位——
       判空只看一处（本类看 count_ 即可）；此时 use_count()==0、
       operator bool 为假、get()==nullptr。
    2、从空出发：默认构造、拷贝一个空对象、移动一个空对象、把空对象
       移动赋给别人、在空对象上 reset（连调两次也安全）、对空对象判 bool。
       到达空：被别人移动、reset、释放到最后一份。
    3、总疫苗机理：只要"空 ⟺ 两个字段全空"永远成立，每个函数入口只需
       问一次"我空不空"（本类就是 release 开头那个 if），答案唯一；
       反过来，如果空有两种表示（如 ptr_ 空但 count_ 悬着），每个函数
       都要各自判断、各有遗漏——07 的 string(nullptr)、10 的拷贝空对象
       崩溃，都是某条路径没维持这个等价。不变式成立 = 判空从
       "N 个函数各记一次"变成"结构上只有一种空"。
 */


class Msg{
private:
    unsigned char* buf_;
    std::size_t len_;
    int id_;
    inline static int live_ = 0;
public:
    Msg(const unsigned char * data, std::size_t len, int id) : 
    buf_(new unsigned char[len]), len_(len), id_(id) {
        memcpy(buf_, data, len_);
        live_++;
    }

    Msg(const Msg&) = delete;
    Msg& operator=(const Msg&) = delete;

    ~Msg() {
        delete[] buf_;
        live_--;
    }

    std::size_t size() const { return len_; }
    const unsigned char* data() const { return buf_; }
    static int live() { return live_; }
    int id() const { return id_; }
};

template <class T>
class Ref{
private:
    T* ptr_ = nullptr;
    long* count_ = nullptr;
    void release() {
        if(count_ == nullptr) return ;
        if(--*count_ == 0) {
            delete ptr_;
            delete count_;
        }
        ptr_ = nullptr;
        count_ = nullptr;
    }
public:
    Ref() = default;

    explicit Ref(T* p) : ptr_(p), count_(ptr_!=nullptr ? new long(1) : nullptr) {}

    Ref(const Ref& other) : ptr_(other.ptr_), count_(other.count_){
        if(other.count_ != nullptr) ++*count_;
    }

    Ref& operator=(const Ref& other) {
        if(this != &other){
            release();
            ptr_ = other.ptr_;
            count_ = other.count_;
            if(count_ != nullptr) ++*count_;
        }
        return *this;
    }

    Ref(Ref&& other) noexcept : ptr_(other.ptr_), count_(other.count_) {
        other.ptr_ = nullptr;
        other.count_ = nullptr;
    }

    Ref& operator=(Ref&& other) noexcept {
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

    ~Ref() { release(); }

    void reset() {
        release();
    }

    explicit operator bool() const {
        return ptr_ != nullptr;
    }

    T* get() const { return ptr_; }

    long use_count() const { return count_ != nullptr ? *count_ : 0; }

    T& operator*() const { return *ptr_; }

    T* operator->() const { return ptr_; }

};

class Owner{
private:
    std::unique_ptr<Msg> msg_;
public:
    explicit Owner(std::unique_ptr<Msg> m) : msg_(std::move(m)) {}
    std::unique_ptr<Msg>& msg() { return msg_; }
};

class Subscriber{
private:
    std::shared_ptr<Msg> msg_;
public:
    explicit Subscriber(std::shared_ptr<Msg> s) : msg_(std::move(s)) {}
    std::shared_ptr<Msg>& msg() { return msg_; }
};

class Observer{
private:
    std::weak_ptr<Msg> msg_;
public:
    explicit Observer(std::weak_ptr<Msg> w) : msg_(std::move(w)) {}
    std::shared_ptr<Msg> peek() const { return msg_.lock(); }
};



int main()
{
    return 0;
}