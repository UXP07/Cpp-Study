#include <cstring>
#include <utility>

/**
SR1. 推断题：`less_than(3, 7)` 的 T 是什么？`less_than(3, 7.0)` 为什么编译失败、
     有哪两种修法？`less_than("ab", "cd")` 的 T 是什么——字符数组去哪了？
答：1、T是int。
    2、两个实参推出两个不同的 T：3 推出 int、7.0 推出 double——模板推断
       不做隐式转换调和（这是特性不是缺陷：防的是悄悄的精度转换）→ 编译失败。
    3、修法一（最常用）：显式写模板实参 less_than<double>(3, 7.0)；
       修法二：让实参类型一致（3->3.0 或 7.0->7）。
    4、T 是 const char*。"数去哪了"是字面意思："ab" 本是 char[3] 数组，
       按值传参时数组退化成首元素的指针——数组本身没被传，传的是首地址，
       所以 T 推成了 const char*。（进阶：可变 char buf[6] 退化成 char*，
       连 const char* 特化都接不住——标准库字符串比较全收 const char*
       就有这层原因。）
SR2. 全特化和偏特化各自匹配什么（用自己的话说清楚区别）？
     函数模板为什么没有偏特化、实际用什么代替？
     你的 `Holder<T*>` 偏特化换掉的只是"实现"还是"契约"？说明理由。
答：1、全特化：指名道姓给某一个具体类型换整套实现（如 const char*）；
       偏特化：给"某一类形状"换实现（如所有指针 T*），它仍是模板，T 没定死。
    2、语言干脆不允许函数模板偏特化（会和重载决议纠缠），实际用重载代替：
       多个函数模板或非模板函数一起参与重载决议。
    3、换的是契约，不只是实现。主模板是值语义：可拷贝、拷出的互不影响、
       不管资源生死；指针版是所有权语义：禁拷贝、移动转移、析构负责 delete、
       release 交责。调用方对"拷一个 Holder 会发生什么"的预期完全不同——
       权利义务说明书整个换了，不只是内部算法换了（vector<bool> 偏特化
       换掉整个使用姿势是同款故事）。
SR3. 代码膨胀：`Holder<int>` 和 `Holder<double>` 会生成几份类？
     为什么模板通常整个写在头文件里，不能像普通函数那样
     "声明放头文件、实现放 cpp"？（提示：编译器生成代码时需要看到什么）
答：1、两份——每种 T 独立实例化一份完整类（代码膨胀的由来）。
    2、实例化发生在用到它的那个编译单元里，编译器生成 Holder<int> 的代码
       时必须看到类的完整定义；实现放别的 cpp，使用处看不到 → 没人生成过
       这份代码 → 链接错误（undefined reference）。普通函数有链接器兜底，
       模板没人预先生成"通用版"，所以模板是头文件居民。
 */


template <typename T>
bool less_than(T a, T b) 
{
    return a < b;
}

template <>
bool less_than<const char *>(const char* a, const char* b){
    return std::strcmp(a, b) < 0;
}

class Token{
private:
    inline static int live_ = 0;
public:
    Token() { live_++; }
    ~Token() { live_--; }
    Token(const Token&) = delete;
    Token& operator=(const Token&) = delete;
    static int live() { return live_; }
};

template <class T>
class Holder{
private:
    T hold_{};
public:
    Holder() = default;
    explicit Holder(const T& h) : hold_(h) {}
    ~Holder() {}

    Holder(const Holder& other) : hold_(other.hold_) {}
    Holder& operator=(const Holder& other) {
        if(this != &other)
        {
            hold_ = other.hold_;
        }
        return *this;
    }

    Holder(Holder&& other) noexcept : hold_(other) {}
    Holder& operator=(Holder&& other) noexcept {
        if(this != &other) {
            hold_ = std::move(other.hold_);
        }
        return *this;
    }

    const T& get() const { return hold_; }

};

template <class T>
class Holder<T*>{
private:
    T* ptr_ = nullptr;
public:
    explicit Holder(T* const p) : ptr_(p) {}

    Holder(const Holder& ) = delete;
    Holder& operator=(const Holder& ) = delete;

    Holder(Holder&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
    Holder& operator=(Holder&& other) noexcept {
        if(this != &other)
        {
            delete ptr_;
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    ~Holder() {
        delete ptr_;
    }

    T* get() const { return ptr_; }
    T* release() {
        T* p = ptr_;
        ptr_ = nullptr;
        return p;
    }

    explicit operator bool() const {
        return ptr_ != nullptr;
    }

};



int main()
{


    return 0;
}