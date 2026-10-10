# 练习 14 标准答案 —— constexpr：把计算搬到编译期（知识点 #6）

## 参考实现

```cpp
// 练习 14 —— constexpr：编译期换算与类型剪枝
#include <type_traits>
#include <utility>

// ===== Q1：constexpr 换算函数 =====

// 四舍五入到最近的 tick：先加半格，再整除取整、乘回
constexpr int round_to_tick(int price, int tick) {
    return (price + tick / 2) / tick * tick;
}

// 间隔 tick 数：与顺序无关
constexpr long long ticks_between(int p1, int p2, int tick) {
    return (p1 > p2 ? static_cast<long long>(p1) - p2
                    : static_cast<long long>(p2) - p1) / tick;
}

// 循环版辗转相除：constexpr 里可以有循环（C++14 起）
constexpr int gcd(int a, int b) {
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a;
}

// ===== Q2：constexpr 字面类型 =====

class Price {
public:
    constexpr Price(int cents) : cents_(cents) {}
    constexpr int dollars() const { return cents_ / 100; }
    constexpr int rem() const { return cents_ % 100; }

private:
    int cents_;
};

// ===== Q3：if constexpr 按类型剪枝 =====

// 整数翻倍；有 size() 的容器给尺寸——错分支被丢弃，连编译都不参与
template <typename T>
constexpr auto scale(const T& v) {
    if constexpr (std::is_integral_v<T>) {
        return v * 2;
    } else {
        return v.size();
    }
}

// ===== Q4：constexpr lambda =====

constexpr auto square = [](int x) { return x * x; };

int main() {
    return 0;  // 验证由判题端驱动完成
}
```

## 判题驱动（判题端使用，放 build/tmp，判完删除）

```cpp
// drive14.cpp
#include <array>
#include <cstdio>
#include <vector>
#define main user_main
#include "../../exercises/14_constexpr/main.cpp"
#undef main

static int fails = 0;
static int total = 0;
#define EXPECT(cond) do { ++total; if (!(cond)) { ++fails; \
    std::printf("[FAIL] line %d: %s\n", __LINE__, #cond); } } while (0)

// ===== 编译期通道：static_assert 全家桶（伪 constexpr 在这里当场击毙） =====
static_assert(round_to_tick(103, 10) == 100);
static_assert(round_to_tick(105, 10) == 110);
static_assert(round_to_tick(104, 10) == 100);
static_assert(round_to_tick(0, 8) == 0);
static_assert(ticks_between(100, 130, 10) == 3);
static_assert(ticks_between(130, 100, 10) == 3);
static_assert(ticks_between(-50, 50, 10) == 10);
static_assert(gcd(12, 18) == 6);
static_assert(gcd(7, 13) == 1);
static_assert(gcd(0, 9) == 9);

constexpr Price unit{12345};
static_assert(unit.dollars() == 123 && unit.rem() == 45);

static_assert(scale(21) == 42);
static_assert(square(9) == 81);

// 硬证据：数组维度只认编译期常量
std::array<int, gcd(12, 18)> dim_proof;
static_assert(dim_proof.size() == 6);
static_assert(sizeof(std::array<unsigned char, square(8)>) == 64);

int main() {
    // ===== 运行期通道：双重生命（同一批函数，运行期变量喂进去） =====
    int price = 103, tick = 10;
    EXPECT(round_to_tick(price, tick) == 100);
    int a = 12, b = 18;
    EXPECT(gcd(a, b) == 6);
    int cents = 12345;
    Price rt{cents};
    EXPECT(rt.dollars() == 123 && rt.rem() == 45);
    EXPECT(scale(5) == 10);                          // 整数路径
    EXPECT(scale(std::vector<int>{1, 2, 3}) == 3);   // 容器路径
    int x = 12;
    EXPECT(square(x) == 144);

    if (fails == 0) {
        std::printf("[PASS] %d checks passed\n", total);
    } else {
        std::printf("[FAIL] %d/%d failed\n", fails, total);
    }
    return fails == 0 ? 0 : 1;
}
```

## 预期结果

17 条 static_assert 全部成立（编译过 = 全对）；运行期通道
`[PASS] 6 checks passed`，退出码 0。

## 设计意图

- **双通道验证是本题核心**：同一个函数，static_assert 里喂字面量（编译期）、
  main 里喂运行期变量——"双重生命"不是背的，是被两头各算一遍的
- **数组维度是硬证据**：`std::array<int, gcd(12,18)>` 编译过的唯一可能
  是 gcd(12,18) 在编译期算完——比任何 static_assert 都硬
  （static_assert 本身也是编译期，但维度是"不得不"）
- **scale 是 if constexpr 的最小完整教学**：普通 if 下 `v * 2` 对
  vector 也必须实例化 → 编译失败——这个"写错就编不过"的痛感就是考点本体
- **Price 用"分"存**：整数域避开浮点 constexpr 的比较陷阱，
  也是行情系统的真实做法（定点数）
- **constexpr lambda**：把 #5 的闭包接进编译期世界——
  无捕获闭包天生 literal type，加个 constexpr 就两头能用

## 评分要点

- 三函数全部真 constexpr（17 条 static_assert 一票否决）
- round_to_tick 的先加半格再整除方向正确；ticks_between 与顺序无关、
  负数域正确；gcd 循环版（非递归）
- Price：constexpr 构造 + 两个 constexpr 成员（漏 const 或 constexpr 都会在
  static_assert 处爆）
- scale：必须 if constexpr（普通 if 编不过容器路径）；is_integral_v 判定
- square：constexpr 闭包
- 运行期通道全对（双重生命）
- 零输出零警告；接口名逐字符正确

## 简答题要点（判题对照）

- SR1：承诺是"只要实参是编译期常量，我就能在编译期算出结果"——注意它
  不是"只能在编译期"：编译期算过，运行期照样当普通函数调（双重生命）。
  循环、分支、局部变量：C++14 起都可以（C++11 只允许单 return）
- SR2：static_assert 编译期检查——条件不成立直接编译失败，错误根本
  活不到运行；EXPECT 是运行期检查，程序跑起来才知道。数组维度/模板实参
  是更硬的证明：这些位置**只认编译期常量**（int a[N] 的 N 必须编期定），
  能塞进去说明这个值真的是编译期算出来的，别无可能
- SR3：普通 if 两条分支都要**实例化并编过**（运行期才知道走哪条，
  所以两条都得是合法代码）；if constexpr 的条件在**编译期**判定，
  不成立的分支被**丢弃**（不实例化、不参与编译）。经典用途：
  模板里按类型剪枝——同一个模板给 int 和容器不同的函数体，
  这是 C++17 之前要靠 SFINAE/标签分发才能做到的事
- SR4：constexpr lambda 是"闭包类型的 operator() 带 constexpr"——
  无捕获（或捕获全是编译期常量）的闭包天生是字面类型，能进 static_assert。
  和 #5 的关系：闭包是编译器生成的类，constexpr lambda 就是给这个类的
  operator() 发证书。天然限制：**按引用捕获的变量不是编译期常量**，
  有引用捕获的闭包进不了 constexpr 世界（捕获的东西必须在编译期定死）

## 常见错误预测（判题重点检查）

1. 函数忘标 constexpr 或只在声明标（static_assert 处当场爆）
2. round_to_tick 除法方向错（先除后加）或忘乘回 tick
3. gcd 用递归版（编译器递归深度限制，大数报错）或 `std::gcd` 违禁
4. Price 成员函数忘 const → constexpr 成员自动 const（无此问题），
   忘 constexpr → static_assert 爆
5. scale 写成普通 if——容器路径直接编不过（这是设计好的教学点，
   编译器会现场教）
6. square 忘 constexpr（static_assert(square(9)==81) 爆）
7. ticks_between 忘处理顺序（返回负数或用了 abs 但域没盖住负数）
8. 接口拼写（老规矩第三查：逐字符照需求抄）

## 模式点拨（判题时向用户点明）

- **查表前移**：任何"输入有限、输出确定"的计算（tick 取整、费率换算、
  CRC 表）都能 constexpr 化——启动前算完躺进 .rodata，运行期零成本
- **双重生命**：constexpr 函数不用写两份（一份编译期一份运行期）——
  一份代码两头用，这就是它比宏和模板元编程优雅的地方
- **if constexpr = 模板的 if**：按类型剪枝的正规姿势（C++17 后 SFINAE
  的大半场景被它取代）——#8 的 concepts 是它的进化版
- **static_assert 是免费的单测**：写在头文件里的编译期断言让错误
  在构建阶段就死，永远到不了用户手里
