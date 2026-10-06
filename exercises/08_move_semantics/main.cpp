#include <iostream>
#include <cstddef>

/**
SR1. `std::move` 到底做了什么、移动了什么？`Tracer c = std::move(a);` 之后 a 还能用吗，为什么？
答：1、std::move 只是 static_cast<T&&>（编译期类型转换），什么都不移动；
       它唯一的作用是把表达式变成右值身份，让重载决议选中移动版本。
       真正的搬运发生在被选中的移动构造/赋值的函数体里。
    2、能用。std::move 没对 a 做任何事；a 变不变取决于移动构造掏不掏它——
       Tracer 的移动构造只 ++moves 不掏资源，a 状态完好；
       有资源的类（如 Blob）会被掏空成空壳，但对象本身仍合法（可析构、可再赋值）。
SR2. `Tracer d = Tracer();` 为什么是 0 拷贝 0 移动？C++17 在这里保证了什么？
     同理 `f(Tracer{})` 按值传参也是 0，但 `f(std::move(d))` 是 1 移动——分界线在哪？
答：1、Tracer() 是纯右值——不是对象，是"初始化图纸"。C++17 保证纯右值直接在
       最终地址物化（构造函数的 this 就是目标的地址，只构造一次），不产生任何
       临时对象，所以 0 拷贝 0 移动（guaranteed copy elision）。
    2、分界线在实参的值类别：f(Tracer{}) 传纯右值——图纸直接在形参位置兑现，0 次；
       f(std::move(d)) 传的是已存在的对象（xvalue）——必须付一次移动构造把内容
       搬进形参，1 次。图纸直接在目标地址盖房；盖好的房子要整只搬进去。
SR3. **预测并给出依据**：Tracer（移动构造非 noexcept）从空 vector push 4 个右值，
     最终 copies 和 moves 各是多少？Blob（noexcept 移动、无拷贝）同场景 moves 是多少？
     vector 扩容搬迁走拷贝还是移动，由什么规则决定（提示：move_if_noexcept 与强异常安全）？
答：1、push_back 内部分两笔账：落位（实参 → 末尾槽位，每次 push 必有 1 次移动构造）
       与搬迁（容量满时旧元素逐一搬去新内存，4 次 push 共搬迁 1+2=3 次）。
       预测：Tracer moves=4（落位全走移动）、copies=3（搬迁全走拷贝）；
       Blob moves=7（4 落位 + 3 搬迁全走移动）、copies 不存在（拷贝已删）。
    2、规则 move_if_noexcept：移动构造 noexcept 或拷贝不可用 → 搬移动；否则搬拷贝。
       原因：搬一半抛异常时旧元素已被掏空、无法回滚；拷一半抛了可以析构新内存、
       原地复原——vector 为保强异常安全只能退回拷贝。
    3、推论：v.reserve(4) 先预留 → 搬迁消失，只剩 4 次落位。
SR4. Blob 被移动后处于什么状态？哪些操作允许（析构/移动赋值/……），
     哪些属于使用错误？"valid but unspecified"和本题的约定是什么关系？
答：1、moved-from 状态：data 空指针、size_ 0、checksum 0——空壳但对象合法。
    2、允许：析构、被移动赋值（复活）、查询接口（返回零值不崩）；
       使用错误：把它当有效快照读数据、调用需要有效资源的操作。
    3、"valid but unspecified" 是标准的底线承诺：被移动对象可析构、可赋值，
       值是什么标准不说；本题把这层"未指定"具体化为三零约定——
       资源类的作者必须给出明确约定，而不是留给人猜。
 */


class Blob {
private:
    unsigned char * data;
    std::size_t size_;
    inline static std::size_t live_ = 0;
    inline static std::size_t moves_ = 0;

public:
    explicit Blob(std::size_t n, int seed) : data(new unsigned char[n]), size_(n) {
        for(std::size_t i=0; i<size_; i++)
        {
            data[i] = static_cast<unsigned char>(seed + i);
        }
        live_++;
    }

    Blob(const Blob& other) = delete;
    Blob& operator=(const Blob& other) = delete;

    Blob(Blob&& other) noexcept : data(other.data), size_(other.size_) {
        other.data = nullptr;
        other.size_ = 0;
        moves_++;
    }

    Blob& operator=(Blob&& other) noexcept {
        if(this != &other)
        {
            if(data != nullptr){
                delete[] data;
                live_--;
            }

            data = other.data;
            other.data = nullptr;

            size_ = other.size_;
            other.size_ = 0;

            moves_++;
        }

        return *this;
    }

    static std::size_t live() { return live_; }
    static std::size_t moves() { return moves_; }
    bool valid() const { return data != nullptr; }
    std::size_t size() const { return size_; }
    unsigned long long checksum() const
    {
        unsigned long long sum = 0;
        for(std::size_t i=0; i < size_; i++)
        {
            sum += data[i];
        }

        return sum;
    }



    ~Blob()
    {
        if(data != nullptr)
        {
            delete[] data;
            data = nullptr;
            live_--;
        }
    }

};

class Tracer{
private:
    inline static std::size_t copies_ = 0;
    inline static std::size_t moves_ = 0;
public:

    Tracer() = default;

    static std::size_t copies() { return copies_; }
    static std::size_t moves() { return moves_; }

    Tracer(const Tracer&) { copies_++; }
    Tracer(Tracer&&) { moves_++; }

    static void reset() {
        copies_ = 0;
        moves_ = 0;
    }
};

int main()
{
    return 0;
}