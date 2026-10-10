# 练习 13 标准答案 —— Lambda 与闭包（知识点 #5）

## 参考实现

```cpp
// 练习 13 —— Lambda 与闭包：捕获的四种姿势
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>

// ========= Part A：四个工厂 =========

// 按值捕获：k 复印进闭包，工厂栈帧死了也不影响
inline auto make_multiplier(int k) {
    return [k](int x) { return x * k; };
}

// init 捕获 + mutable：闭包自带状态；拷贝即分叉（每个闭包对象有自己的 n）
inline auto make_counter() {
    return [n = 0]() mutable { return ++n; };
}

// 引用捕获：留的是 cell 的地址，调用改的是外面的原变量
inline auto make_probe(int& cell) {
    return [&cell]() { ++cell; };
}

// init 捕获 + move：把 unique_ptr 塞进闭包——成员不可拷贝 => 闭包不可拷贝
inline auto make_owner(std::unique_ptr<int> p) {
    return [p = std::move(p)]() { return *p; };
}

// ========= Part B：捕获 this =========

class Accumulator {
public:
    auto adder() {
        // 捕获的是 this 指针：total_ 的生死跟着对象走，闭包只是攥着地址
        return [this](int x) {
            total_ += x;
            return total_;
        };
    }
    long total() const { return total_; }

private:
    long total_ = 0;                       // NSDMI：出生即零
};

// ========= Part C：lambda 定制 STL 算法 =========

inline std::vector<int> sort_by_abs(const std::vector<int>& v) {
    std::vector<int> out = v;
    std::sort(out.begin(), out.end(),
              [](int a, int b) { return std::abs(a) < std::abs(b); });  // 严格弱序
    return out;
}

inline std::size_t count_over(const std::vector<int>& v, int threshold) {
    return static_cast<std::size_t>(
        std::count_if(v.begin(), v.end(),
                      [threshold](int x) { return x > threshold; }));
}

inline std::vector<int> squares(const std::vector<int>& v) {
    std::vector<int> out;
    out.reserve(v.size());
    std::transform(v.begin(), v.end(), std::back_inserter(out),
                   [](int x) { return x * x; });
    return out;
}

int main() {
    return 0;  // 验证由判题端驱动完成
}
```

## 判题驱动（判题端使用，放 build/tmp，判完删除）

```cpp
// drive13.cpp
#include <cstdio>
#include <memory>
#include <type_traits>
#include <vector>
#define main user_main
#include "../../exercises/13_lambdas/main.cpp"
#undef main

static int fails = 0;
static int total = 0;
#define EXPECT(cond) do { ++total; if (!(cond)) { ++fails; \
    std::printf("[FAIL] line %d: %s\n", __LINE__, #cond); } } while (0)

int main() {
    // ===== A1. 按值捕获：工厂返回后依然安全 =====
    auto m3 = make_multiplier(3);
    EXPECT(m3(5) == 15 && m3(-2) == -6);
    EXPECT(make_multiplier(7)(0) == 0);
    static_assert(std::is_copy_constructible_v<decltype(m3)>,
                  "value-capture closure copyable");

    // ===== A2. mutable 计数器：闭包状态 + 拷贝分叉 =====
    auto c = make_counter();
    EXPECT(c() == 1 && c() == 2 && c() == 3);
    auto c2 = c;                           // 拷贝：状态一起复印，此后各数各的
    EXPECT(c2() == 4 && c2() == 5);
    EXPECT(c() == 4);                      // 分叉证据：c 不受 c2 影响

    // ===== A3. 引用捕获：改的是原变量 =====
    int cell = 1;
    auto probe = make_probe(cell);
    probe();
    probe();
    EXPECT(cell == 3);

    // ===== A4. init 捕获 + move：unique_ptr 进闭包 =====
    auto owner = make_owner(std::unique_ptr<int>(new int(42)));
    EXPECT(owner() == 42);
    static_assert(!std::is_copy_constructible_v<decltype(owner)>,
                  "owning closure must not be copyable");

    // ===== B. 捕获 this：同一对象，共享 total =====
    Accumulator acc;
    auto add = acc.adder();
    EXPECT(add(5) == 5 && add(3) == 8);
    EXPECT(acc.total() == 8);
    auto add2 = acc.adder();
    EXPECT(add2(1) == 9);                  // 两个闭包攥着同一个 this

    // ===== C. lambda 定制 STL =====
    EXPECT((sort_by_abs({-5, 2, -3, 1}) == std::vector<int>{1, 2, -3, -5}));
    EXPECT(count_over({1, 5, 9, 2}, 4) == 2);
    EXPECT((squares({1, 2, 3}) == std::vector<int>{1, 4, 9}));

    if (fails == 0) {
        std::printf("[PASS] %d checks passed\n", total);
    } else {
        std::printf("[FAIL] %d/%d failed\n", fails, total);
    }
    return fails == 0 ? 0 : 1;
}
```

## 预期结果

`[PASS] 13 checks passed`，退出码 0；2 条编译期 static_assert（正/负各一）成立。

## 设计意图

- **"闭包是编译器写的类"**：四个工厂对应四种捕获姿势，验收全用闭包的
  可观察行为（分叉、改原变量、不可拷贝）——把 06-12 的类直觉整体迁移
- **counter 拷贝分叉**是本题最有价值的锚点：证明闭包 = 有成员的对象，
  拷贝 = 复印状态，之后互不相干——很多人用了一年 lambda 也没看清这件事
- **owner 闭包不可拷贝**：unique_ptr 成员 → 闭包不可拷贝，
  "成员决定类的拷贝性"在 lambda 身上的复现（负向 static_assert 抓）
- **禁止默认捕获 [=]/[&]**：每个捕获显式列出是工业规范（避免悄悄
  延长生命周期或悄悄悬垂），也逼着写题时逐份思考
- **sort 比较器严格弱序**：`<=` 会让 std::sort 越界/UB——数据集没造等值
  绝对值，靠代码审查抓这个经典坑

## 评分要点

- 四工厂行为全对：值捕获独立于工厂栈帧、counter 分叉、probe 改原变量、
  owner 不可拷贝（编译期）
- Accumulator：this 捕获改成员、两个 adder 共享同一 total、NSDMI
- STL 三连结果正确；比较器严格弱序；lambda 定制（不许手写循环糊弄）
- 捕获全部显式（无 [=]/[&]）；无 std::function；auto 返回闭包本体
- 简答题四问要点齐全（见下）；零输出零警告

## 简答题要点（判题对照）

- SR1：选法——只用一次/需要副本语义 → 按值；需要改原变量/对象很大 →
  按引用，但**引用的生命周期必须盖过闭包**。悬垂：工厂的局部变量死在
  栈上，闭包留着的地址指向已回收的栈帧——调用即 UB。捕获即成员：
  [x] = 类里有个 x 成员（构造时复印），[&x] = 类里有个 x 的地址成员——
  06-12 的"成员是什么语义，类就是什么语义"原样适用于这个编译器生成的类
- SR2：mutable 解除按值捕获成员的 const（默认 operator() 是 const 的，
  改副本要 mutable）。分叉：拷贝闭包 = 复印全部成员（n 被复印），
  两个对象各自的 n 各自 ++。本质：闭包是编译器生成的匿名类的一个实例，
  捕获列表是它的成员初始化列表，调用是 operator()
- SR3：init 捕获（广义捕获，C++14）。解决：①给捕获成员起名并就地
  构造 ②能塞 move-only 对象（[p] 对 unique_ptr 编不过——那是要求拷贝）。
  不可拷贝：unique_ptr 成员不可拷贝 → 编译器为闭包生成的拷贝构造被删——
  "成员决定类的拷贝性"（shared_ptr 成员闭包就可拷贝、Blob 成员闭包就只移动）
- SR4：捕获的是 **this 指针**（不是对象副本）。悬垂条件：对象先死、
  闭包后用（比 lambda 存活更久的对象才安全——成员函数返回的 lambda
  危险系数最高）。C++17 的 [=] 会**隐式按值捕获 this 指针**（看起来
  拷了对象，实际只拷了指针）——对象死了照样悬垂；C++20 才有 [*this]
  拷贝对象本体。坑：[=] 给了"值捕获很安全"的错觉

## 常见错误预测（判题重点检查）

1. counter 忘 mutable（operator() 默认 const，++ 编不过或改不动）
2. make_probe 写成 [cell]（按值：probe 后原变量不变，EXPECT 抓）
3. make_owner 用 [p]（要求拷贝 unique_ptr，编译失败）或忘 std::move
4. adder 写 [total_]（成员名不能直接捕获——只能 [this] 或 [&]；
   经典编译错误）
5. counter 语义写成共享（static n / 引用静态——分叉验收直接抓）
6. 用 [=]/[&] 默认捕获（约束违反，代码审查抓）
7. sort 比较器写成 <=（严格弱序违规，UB——审查抓，数据集故意不含等值绝对值）
8. 用 std::function 包返回（约束违反 + 白丢内联）
9. 手写 for 循环实现 Part C（"用 lambda 定制算法"是考点本体）

## 模式点拨（判题时向用户点明）

- **闭包 = 编译器写的类**：捕获列表 = 成员初始化列表，mutable = 去 const，
  拷贝闭包 = 拷成员——五法则知识原样迁移（这是 06-12 与 13 的桥）
- **工厂返回 lambda 的安全清单**：按值 = 复印安全；按引用 = 只准引用
  参数传入的、生命周期由调用方保证的对象；[this] = 对象必须活得比闭包久
- **init 捕获 + move = 把所有权塞进闭包**：回调持资源的标准姿势
  （异步任务的 per-task 状态、线程池 task 捕获 move-only payload 全是它）
- **比较器三戒**：严格弱序（< 不是 <=）、两侧对称、不改状态——
  sort/set/map 的万能坑，现在记住以后少debug 几小时
