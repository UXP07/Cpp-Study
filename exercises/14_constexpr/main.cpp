#include <type_traits>
#include <utility>

/*
SR1. constexpr 是给函数的什么承诺？拿到证书的函数"双重生命"指什么？
     constexpr 函数里能不能有循环、分支？（哪一年起就可以）
    1、可以在编译期被执行，也可以在运行时执行。
    2、编译期当常量算，运行期当普通函数调（双重生命：一份代码两头用）
    3、可以——C++14 起循环、分支、局部变量都允许（C++11 只许单 return）。
SR2. static_assert 和运行期断言（比如你写过的 EXPECT）有什么本质区别？
     "能当数组维度/模板实参"为什么是比"static_assert 能算"更硬的
     constexpr 证明？（提示：这些位置只认什么）
    1、static_assert是编译时检查，断言是运行时记录。
    2、是，数组维度/模板实参只认编译期常量。更硬的那半：这个值直接参与
       构成类型本身（std::array<int, 6> 的 6 是类型的一部分）——static_assert
       是"检查它"，数组维度是"拿它当积木盖进类型里"，能当积木的常量
       没有任何造假空间。
SR3. if constexpr 和普通 if 的本质区别？普通 if 为什么编不过 Q3
     （两个分支都要实例化——错的那支编不过就全盘编不过）？
     这个特性在模板里的经典用途是什么？
    1、编译时会保留符合条件的代码，不符合的直接不编译。
    2、普通 if 是运行期决定走哪条，两条分支都必须是合法代码（都要实例化）：
       给 vector 实例化 v*2 那支时，vector*2 这个运算不存在 → 整个函数编不过。
       if constexpr 在编译期就裁掉错分支——那段代码从未存在过，自然不用编。
    3、对模板接收的类型进行分类执行（C++17 之前要靠 SFINAE/标签分发做的事）。
SR4. constexpr lambda 是什么（C++17）？它和你 #5 写的闭包是什么关系——
     捕获列表里的东西想进 constexpr 世界有什么天然限制？
    1、闭包的 operator() 是带 constexpr的函数。
    2、隐式不写consrexpr的闭包可以是编译期执行，也可以运行时执行。显示指定的闭包要符合constexpr闭包的要求，不能传引用值等等。
    3、必须能在编译期求得。
*/



constexpr int round_to_tick(int price, int tick)
{
    return (price+tick/2) / tick * tick;
}

constexpr long long ticks_between(int p1, int p2, int tick) {
    return (p1 > p2 ? static_cast<long long>(p1-p2) : static_cast<long long>(p2-p1))/tick;
}

constexpr int gcd(int a, int b) {
    while(b != 0) {
        int t = a%b;
        a = b;
        b = t;
    }
    return a;
}


class Price{
private:
    int cents_;
public:
    constexpr Price(int cents) : cents_(cents) {}
    constexpr int dollars() const { return cents_/100; }
    constexpr int rem() const {return cents_%100;}
};

template <typename T>
constexpr auto scale(const T& v) {
    if constexpr (std::is_integral_v<T>) {
        return v*2;
    }
    else
    {
        return v.size();
    }
}

constexpr auto square = [](int x) { return x*x; };



int main()
{
    return 0;
}