#include <algorithm>
#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>

/**
SR1. 按值和按引用怎么选？工厂函数返回的 lambda 为什么**绝不能**捕获
     函数内局部变量的引用——"悬垂"具体指什么（谁先死、地址留着指谁）？
     "捕获即成员"怎么把你 06-12 练的所有权直觉接到 lambda 上？
答: 1、考虑是否要改变原有的变量，使用的次数，对象的内存大小。
       总纲：按引用的前提只有一个——被引用对象的生命周期必须盖过闭包。
    2、当工厂函数返回后，内部的局部变量被回收，但是地址还存在返回的lambda对象里，此时不能再操作这个内存。
    3、相当于构造函数中传入的是值还是引用。
SR2. mutable 是干什么的？为什么 counter 拷贝之后两边各数各的？
     这说明闭包的本质是什么（用"类、对象、成员"的语言回答）？
答: 1、让const函数中也可以改动变量。
    2、拷贝相当于创建副本，肯定是独立计数。
    3、本质是初始化了一个匿名的对象，传入的捕获是初始化成员变量，后续的函数是类中的成员函数。
SR3. `[p = std::move(p)]` 这个写法叫什么、解决什么问题？
为什么这个闭包不可拷贝——把"成员决定类的拷贝性"这句话接到本题。
答: 1、init捕获，解决了std::unique_ptr指针只能指向一个对象的问题——[p] 要求
       拷贝（编不过），[p = std::move(p)] 是在闭包构造时刻移动初始化这个成员，
       转移所有权进闭包。
    2、成员没有拷贝构造没有拷贝赋值。
SR4. `[this]` 捕获的到底是什么（指针还是对象副本）？什么条件下 this
     闭包会悬垂？C++17 里 `[=]` 会不会悄悄把 this 带上、坑在哪？
答: 1、指针
    2、this所指的对象销毁在闭包之前。
    3、如果用到的话，会直接拷贝this指针，但是指向的并非是对象的拷贝，而是对象本身。
 */



auto make_multiplier(int k) {
    return [k](int x)->int { return x*k; };
}

auto make_counter() {
    return [n=0]() mutable { return ++n;};
}

auto make_probe(int& cell) {
    return [&cell]() { return ++cell;};
}

auto make_owner(std::unique_ptr<int> p){
    return [p = std::move(p)]() { return *p; };
}

class Accumulator {
private:
    long total_ = 0;
public:

    auto adder()
    {
        return [this](int k)->long {
            this->total_ = this->total_+k;
            return this->total_;
        };
    }

    long total() const { return total_; }
};

std::vector<int> sort_by_abs(const std::vector<int>& v) {
    std::vector<int> nums = v;
    std::sort(nums.begin(), nums.end(), 
    [](int a, int b){
        return std::abs(a) < std::abs(b);
    });
    return nums;
}

std::size_t count_over(const std::vector<int>& v, int threshold) {
    return static_cast<std::size_t>(
        std::count_if(v.begin(), v.end(),
        [threshold](int x){
            return x > threshold;
        }));
}

std::vector<int> squares(const std::vector<int>& v) {
    std::vector<int> out;
    out.reserve(v.size());
    std::transform(v.begin(), v.end(), std::back_inserter(out), 
                    [](int x) { return x*x; });

    return out;
}

int main()
{

    return 0;
}