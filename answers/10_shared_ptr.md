# 练习 10 标准答案 —— 智能指针：所有权建模与引用计数（知识点 #3）

## 参考实现

```cpp
// 练习 10 —— 所有权建模（Node 循环拆弹）+ 手写引用计数 Shared<T>
#include <memory>
#include <utility>

// 公共载荷：被拥有者，活计数观测
class Widget {
public:
    explicit Widget(int v) : v_(v) { ++live_; }
    ~Widget() { --live_; }
    Widget(const Widget&) = delete;             // 载荷不可拷贝：逼出指针语义
    Widget& operator=(const Widget&) = delete;
    int value() const { return v_; }
    static int live() { return live_; }
private:
    int v_;
    inline static int live_ = 0;
};

// Part A：双向链节点——一个方向"持有"，一个方向"观察"，环才拆得开
class Node {
public:
    Node() { ++live_; }
    ~Node() { --live_; }
    static int live() { return live_; }

    void set_next(std::shared_ptr<Node> n) { next_ = std::move(n); }  // 持有方向：shared
    void set_prev(std::weak_ptr<Node> p)   { prev_ = std::move(p); }  // 观察方向：weak

    std::shared_ptr<Node> next() const { return next_; }
    std::shared_ptr<Node> prev() const { return prev_.lock(); }       // 用前 lock：可能已死

private:
    std::shared_ptr<Node> next_;
    std::weak_ptr<Node>   prev_;
    inline static int live_ = 0;
};

// Part B：手写简化版引用计数智能指针（单线程版）
template <typename T>
class Shared {
public:
    Shared() = default;

    // 接管裸指针：资源与"迷你控制块"（堆上的共享计数）一起立起来
    explicit Shared(T* p) : ptr_(p), count_(p != nullptr ? new long(1) : nullptr) {}

    // 拷贝 = 共享：同一资源、同一计数，+1
    Shared(const Shared& other) : ptr_(other.ptr_), count_(other.count_) {
        if (count_ != nullptr) {
            ++*count_;
        }
    }

    // 移动 = 过户：持有者人数没变，计数不动；源拆雷
    Shared(Shared&& other) noexcept : ptr_(other.ptr_), count_(other.count_) {
        other.ptr_ = nullptr;
        other.count_ = nullptr;
    }

    Shared& operator=(const Shared& other) {
        if (this != &other) {
            release();                          // 先放掉自己的旧份额
            ptr_ = other.ptr_;
            count_ = other.count_;
            if (count_ != nullptr) {
                ++*count_;                      // 再加入对方的共享
            }
        }
        return *this;
    }

    Shared& operator=(Shared&& other) noexcept {
        if (this != &other) {
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
    long use_count() const { return count_ != nullptr ? *count_ : 0; }
    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }

private:
    // 释放份额：最后一人离开时，资源与控制块同亡
    void release() {
        if (count_ == nullptr) {
            return;
        }
        if (--*count_ == 0) {
            delete ptr_;
            delete count_;
        }
        ptr_ = nullptr;
        count_ = nullptr;
    }

    T*    ptr_ = nullptr;
    long* count_ = nullptr;                     // 迷你控制块：必须堆上、被所有副本共享
};

int main() {
    return 0;  // 验证由判题端驱动完成
}
```

## 判题驱动（判题端使用，放 build/tmp，判完删除）

```cpp
// drive10.cpp
#include <cstdio>
#include <memory>
#include <type_traits>
#include <utility>
#define main user_main
#include "../../exercises/10_shared_ptr/main.cpp"
#undef main

static int fails = 0;
static int total = 0;
#define EXPECT(cond) do { ++total; if (!(cond)) { ++fails; \
    std::printf("[FAIL] line %d: %s\n", __LINE__, #cond); } } while (0)

// 编译期特征
static_assert(std::is_default_constructible_v<Shared<Widget>>, "default ctor");
static_assert(std::is_copy_constructible_v<Shared<Widget>>, "copyable");
static_assert(std::is_copy_assignable_v<Shared<Widget>>, "copy-assignable");
static_assert(std::is_nothrow_move_constructible_v<Shared<Widget>>, "move ctor noexcept");
static_assert(std::is_nothrow_move_assignable_v<Shared<Widget>>, "move assign noexcept");

int main() {
    // ===== A. std::shared_ptr 基线：拷贝共享不复制资源 =====
    EXPECT(Widget::live() == 0);
    {
        std::shared_ptr<Widget> a = std::make_shared<Widget>(1);
        EXPECT(a.use_count() == 1 && Widget::live() == 1);
        {
            std::shared_ptr<Widget> b = a;                    // 拷贝：use+1，live 不变
            EXPECT(a.use_count() == 2 && Widget::live() == 1);
        }
        EXPECT(a.use_count() == 1 && Widget::live() == 1);
    }
    EXPECT(Widget::live() == 0);

    // ===== B. 循环引用拆弹：双向链接后整体释放，live 必须归零 =====
    {
        auto a = std::make_shared<Node>();
        auto b = std::make_shared<Node>();
        a->set_next(b);
        b->set_prev(a);
        EXPECT(a->next() != nullptr && b->prev() == a);       // 双向可达
    }
    EXPECT(Node::live() == 0);   // 若 prev 也是 shared：环成，live 卡在 2

    // ===== C. Shared<Widget> 生命周期与计数锚点 =====
    EXPECT(Widget::live() == 0);
    {
        Shared<Widget> a(new Widget(1));                      // 接管 =1
        EXPECT(a.use_count() == 1 && Widget::live() == 1);
        Shared<Widget> b = a;                                 // 拷贝 =2
        EXPECT(a.use_count() == 2 && b.use_count() == 2 && Widget::live() == 1);
        {
            Shared<Widget> c = b;                             // 3
            EXPECT(c.use_count() == 3);
        }
        EXPECT(b.use_count() == 2);                           // 回到 2

        Shared<Widget> m = std::move(a);                      // 移动：计数不变
        EXPECT(m.use_count() == 2 && Widget::live() == 1);
        EXPECT(a.use_count() == 0 && a.get() == nullptr);
        EXPECT((*m).value() == 1 && m->value() == 1);

        Shared<Widget> d(new Widget(2));                      // live=2
        EXPECT(Widget::live() == 2);
        d = b;                                                // 拷贝赋值：放旧 + 共享
        EXPECT(Widget::live() == 1 && d.use_count() == 3);
        Shared<Widget>& rb = b;
        b = rb;                                               // 自赋值
        EXPECT(b.use_count() == 3 && Widget::live() == 1);

        Shared<Widget> e;                                     // 空对象
        EXPECT(e.use_count() == 0 && e.get() == nullptr);
        e = std::move(d);                                     // 复活
        EXPECT(e.use_count() == 3 && d.use_count() == 0);
    }
    EXPECT(Widget::live() == 0);                              // 全释放，资源归零

    if (fails == 0) {
        std::printf("[PASS] %d checks passed\n", total);
    } else {
        std::printf("[FAIL] %d/%d failed\n", fails, total);
    }
    return fails == 0 ? 0 : 1;
}
```

## 预期结果

`[PASS] 21 checks passed`，退出码 0；5 条编译期 static_assert 全部成立。

## 设计意图

- **计数器三部曲收官**：live（资源活计数，06 起）→ moves（搬运计数，08）→
  use_count（持有者计数，10）——所有权语义层层递进，验证手段一脉相承
- **控制块 = 堆上的 `long`**：std::shared_ptr 的控制块（计数 + 弱计数 + 删除器）
  的最小内核；它必须在堆上，因为生命周期要独立于任何一个副本
- **Node 一持一观**：next shared（方向"拥有"）、prev weak（方向"观察"）——
  环上有向的"持有边"不构成回路，引用计数才清得零
- **拷贝赋值先 release 后共享**：与 07 的 copy-and-swap 同族——所有会改自己
  状态的操作，都把"放旧"放在"拿新"之后才安全
- **Widget 删拷贝**：载荷不可复制，shared 的"共享不复制"语义才被逼出来
  （否则拷贝 Widget 也能活，考点失焦）

## 评分要点

- Node 双向链接后 live 归零（核心）；next()/prev() 双向可达，prev 经 lock()
- Shared：接管=1 / 拷贝+1 / **移动不变** / 最后一份释放时资源与控制块同亡
- 计数在堆上被共享（普通成员=每个副本各一份，计数永远到不了真实值）
- 拷贝赋值先放旧后共享、自赋值安全；空对象全程安全、可被移动赋值复活
- use_count() 空对象返回 0；operator*/operator-> 正确
- 移动 noexcept；编译期特征五连过；零输出；注释中文
- 简答题四问要点齐全（见下）

## 简答题要点（判题对照）

- SR1：**唯一拥有 → unique_ptr；共享生命周期 → shared_ptr；只观察不拥有 →
  weak_ptr（可判死活）或裸指针/引用（不管理）**。参数惯例：函数只是"用一下"
  就传 `const T&` 或裸指针/引用，不传智能指针——不转移所有权就不该让签名
  产生所有权暗示；要吞下所有权才收 `unique_ptr<T>`（按值）或 `shared_ptr<T>`
- SR2：普通成员会被每个副本各拷一份——每个副本都有自己的计数=1，release 时
  各删各的 → double free。这**正是练习 03 浅拷贝的同一现象**：指针成员浅拷贝
  后副本共享对象却各自为政；解法从"深拷贝资源"升级成"共享计数"。
  拷贝 = 计数 +1（多了一个持有者）；移动 = 不动（持有者换人没换人数）。
  `make_shared` 把对象和控制块合并成一次堆分配（省一次分配 + 更好的缓存局部性）；
  `shared_ptr<T>(new T)` 是两次分配
- SR3：环上每条边都是 shared = 每个节点的计数都至少被对方 +1，外部引用全部
  撤走后计数仍互锁在 1，谁也到不了 0——内存泄漏。weak 不增加计数（"观察不
  持有"），把环上的"持有边"减到不构成回路。lock() 尝试换取一个临时 shared：
  对象还活着 → 拿到引用且期间计数 +1（防在使用中途死亡）；已死 → 空 shared_ptr
- SR4：不能反——先共享对方再放自己：释放旧份额时若它是最后一份会 delete 资源，
  而此刻你刚接上的正是同一份资源的另一份额（当 this 与 other 共享同一资源时，
  旧份额的 release 会把资源删掉，新接的指针瞬间悬垂）。自赋值走不到这条路径：
  `this != &other` 直接跳过，计数不动——这就是自赋值安全

## 常见错误预测（判题重点检查）

1. 计数写成普通成员（`long count_`）——每个副本私有计数，use_count 永远 1，
   析构 double free（03 浅拷贝的现代版重演，判题重点）
2. 移动构造把计数 +1 或 -1（持有者人数没变，动它就是错——08 "move≠acquire"
   的计数版）
3. 拷贝赋值顺序反了：先接管后 release 自己 → 自共享/同资源赋值场景悬垂
4. Node 两个方向都选 shared：循环引用成环，live 归零 FAIL（本题 Part A 的
   核心考点）
5. release 在计数归零时忘 delete 控制块（内存泄漏）或空指针不判（崩溃）
6. moved-from 对象析构二次 release（源没置空——06 起的老毛病第 N 次查）
7. 拷贝构造忘 ++ 计数 / 忘判 count_ 为空（拷贝空对象崩溃）
8. `operator->` 写成返回 T&（返回指针才对）；use_count 对空对象不判空

## 模式点拨（判题时向用户点明）

- **所有权三件套判据**：unique（唯一）/ shared（共享）/ weak（观察）——
  画所有权图，边标类型，环上不能全是"持有"边
- **控制块**：跨对象共享的状态必须放堆上、生命周期独立于任何副本——
  这个模式以后到处都是（控制块 = 引用计数 + 删除器 + 弱计数的完整版）
- **计数器三部曲**（live → moves → use_count）：把所有权语义变成可断言的数字，
  从 06 用到 10，以后写任何资源类先想"哪个计数能证明我"
- **lock() 惯用法**：weak → 检查存活 → 临时持有（RAII 保命窗口）——
  与锁守卫同构："拿到引用的期间对象不许死"
