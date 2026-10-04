# 练习 07 标准答案 —— 连接与锁：五法则的两条出路（复习：练习 06 / 知识点 #1）

## 参考实现

```cpp
// 练习 07 —— 连接与锁：五法则的两条出路
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

// Part A：可复制的资源类（copy-and-swap 统一赋值）
class Conn {
public:
    // 获取：先校验容量再分配（验证在获取之前，失败即无物可漏）
    Conn(const std::string& endpoint) : ctx_(nullptr), size_(0) {
        if (live_ >= cap_) {
            throw std::runtime_error("capacity exceeded: " + endpoint);
        }
        size_ = endpoint.size();
        ctx_  = new char[size_];
        std::memcpy(ctx_, endpoint.data(), size_);
        ++live_;                       // 构造 = 新开连接
    }

    // 拷贝 = 深拷贝资源 + 新开连接；other 全程只读
    Conn(const Conn& other) : ctx_(nullptr), size_(other.size_) {
        if (live_ >= cap_) {
            throw std::runtime_error("capacity exceeded on copy");
        }
        ctx_ = new char[other.size_];
        std::memcpy(ctx_, other.ctx_, other.size_);
        ++live_;                       // 拷贝也是新开
    }

    // 移动 = 所有权过户：不分配、不查容量、计数不变——noexcept 的根源
    Conn(Conn&& other) noexcept : ctx_(other.ctx_), size_(other.size_) {
        other.ctx_  = nullptr;         // 给源的析构拆雷
        other.size_ = 0;
    }

    // 统一赋值：按值收参（左值实参→拷贝构造形参，右值实参→移动构造形参）
    // 形参在进入函数体之前完成所有可能抛异常的工作；swap 绝不抛
    Conn& operator=(Conn other) noexcept {
        swap(other);                   // 指针交换，旧资源此刻挂到了形参身上
        return *this;
    }                                  // 形参析构，带走旧资源

    void swap(Conn& other) noexcept {
        std::swap(ctx_, other.ctx_);
        std::swap(size_, other.size_);
    }

    ~Conn() {
        if (ctx_ != nullptr) {         // moved-from 不减计数
            delete[] ctx_;
            --live_;
        }
    }

    std::string endpoint() const { return std::string(ctx_, size_); }
    static int live() { return live_; }
    static void set_capacity(int n) { cap_ = n; }

private:
    char*     ctx_;
    std::size_t size_;
    inline static int live_ = 0;       // C++17 inline 静态成员
    inline static int cap_  = 1 << 30; // 默认不限
};

// Part B：禁拷贝、只移动的守卫（lock_guard 与 unique_lock 的取舍课）
class ScopedLock {
public:
    explicit ScopedLock(std::mutex& m) : m_(&m), locked_(true) {
        m_->lock();                    // 加锁失败抛异常 = 对象从未存在
    }

    ScopedLock(const ScopedLock&) = delete;             // 复制锁 = 语义不成立
    ScopedLock& operator=(const ScopedLock&) = delete;

    ScopedLock(ScopedLock&& other) noexcept
        : m_(other.m_), locked_(other.locked_) {
        other.m_      = nullptr;
        other.locked_ = false;
    }

    ScopedLock& operator=(ScopedLock&& other) noexcept {
        if (this != &other) {
            release();                 // 先放自己的旧锁
            m_      = other.m_;
            locked_ = other.locked_;
            other.m_      = nullptr;
            other.locked_ = false;
        }
        return *this;
    }

    bool holds() const { return locked_ && m_ != nullptr; }

    ~ScopedLock() { release(); }

private:
    void release() {
        if (locked_ && m_ != nullptr) {
            m_->unlock();
        }
        m_      = nullptr;
        locked_ = false;
    }

    std::mutex* m_;
    bool        locked_;
};

int main() {
    return 0;  // 演示与验证由判题端驱动完成
}
```

## 判题驱动（判题端使用，放 build/tmp，判完删除）

```cpp
// drive07.cpp
#include <cstdio>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>
#define main user_main
#include "../../exercises/07_conn_guard/main.cpp"
#undef main

static int fails = 0;
static int total = 0;
#define EXPECT(cond) do { ++total; if (!(cond)) { ++fails; \
    std::printf("[FAIL] line %d: %s\n", __LINE__, #cond); } } while (0)

// 编译期特征（不满足直接编译失败）
static_assert(!std::is_copy_constructible_v<ScopedLock>, "lock not copyable");
static_assert(!std::is_copy_assignable_v<ScopedLock>, "lock not copy-assignable");
static_assert(std::is_nothrow_move_constructible_v<ScopedLock>, "lock move ctor noexcept");
static_assert(std::is_nothrow_move_assignable_v<ScopedLock>, "lock move assign noexcept");
static_assert(std::is_copy_constructible_v<Conn>, "conn copyable");
static_assert(std::is_nothrow_move_constructible_v<Conn>, "conn move ctor noexcept");
static_assert(std::is_nothrow_move_assignable_v<Conn>, "conn move-assign path noexcept");
static_assert(!std::is_nothrow_copy_assignable_v<Conn>, "conn copy-assign may throw");

int main() {
    // A. 计数链 + 深拷贝 + 自赋值 + 移交
    EXPECT(Conn::live() == 0);
    {
        Conn a("tcp://gw1:5001");
        Conn b("tcp://gw2:5002");
        EXPECT(Conn::live() == 2);
        Conn c = a;                          // 拷贝：+1
        EXPECT(Conn::live() == 3);
        EXPECT(c.endpoint() == "tcp://gw1:5001");
        Conn& rc = c;
        c = rc;                              // 自赋值（无特判也应正确）
        EXPECT(c.endpoint() == "tcp://gw1:5001");
        EXPECT(Conn::live() == 3);
        {
            Conn d = std::move(b);           // 移动：计数不变
            EXPECT(Conn::live() == 3);
            EXPECT(d.endpoint() == "tcp://gw2:5002");
        }
        EXPECT(Conn::live() == 2);
        EXPECT(b.endpoint().empty());        // moved-from：空且可析构
    }
    EXPECT(Conn::live() == 0);               // 泄漏探测器归零
    {
        Conn a("x");                         // 深拷贝证据：源死后副本完好
        Conn keep = a;
        EXPECT(keep.endpoint() == "x");
    }
    EXPECT(Conn::live() == 0);

    // B. 容量、构造失败、强保证、饱和下移动
    Conn::set_capacity(2);
    {
        Conn x("a"), y("b");
        EXPECT(Conn::live() == 2);
        bool threw = false;
        try { Conn z("c"); } catch (const std::exception&) { threw = true; }
        EXPECT(threw);                       // 构造失败
        EXPECT(Conn::live() == 2);           // 计数不漂移

        threw = false;
        try { x = y; } catch (const std::exception&) { threw = true; }
        EXPECT(threw);                       // 容量满：拷贝赋值失败
        EXPECT(x.endpoint() == "a");         // 强保证：x 原封不动
        EXPECT(Conn::live() == 2);

        x = std::move(y);                    // 饱和下移动赋值成功
        EXPECT(x.endpoint() == "b");
        EXPECT(y.endpoint().empty());
        EXPECT(Conn::live() == 1);
    }
    EXPECT(Conn::live() == 0);
    Conn::set_capacity(1 << 30);

    // C. 锁的证据链（单线程 try_lock 探针）
    std::mutex m, m2;
    {
        ScopedLock g(m);
        EXPECT(!m.try_lock());               // 被持有
        ScopedLock h = std::move(g);         // 移交
        EXPECT(h.holds());
        EXPECT(!g.holds());
        EXPECT(!m.try_lock());               // 新主人持有
        ScopedLock k(m2);
        k = std::move(h);                    // 移动赋值：k 先放 m2 再接管 m
        EXPECT(k.holds());
        EXPECT(!h.holds());
        EXPECT(!m.try_lock());
        EXPECT(m2.try_lock());               // m2 已被释放
        m2.unlock();
    }
    EXPECT(m.try_lock());                    // 析构已解锁
    m.unlock();

    if (fails == 0) {
        std::printf("[PASS] %d checks passed\n", total);
    } else {
        std::printf("[FAIL] %d/%d failed\n", fails, total);
    }
    return fails == 0 ? 0 : 1;
}
```

## 预期结果

驱动全部 EXPECT 通过，输出 `[PASS] N checks passed`，退出码 0；
编译期 8 条 static_assert 全部成立（不成立则驱动编译失败）。

## 设计意图

- **容量 mock** 模拟真实获取失败（对应 06 的 missing.txt），且**校验先于分配**——
  失败路径上没有任何需要手动归还的东西
- **live 计数 = 把"不泄漏"变成一个可断言的数字**：RAII 正确性不再靠 trace 眼看，
  而是计数归零一锤定音
- **拷贝 +1 / 移动不变**：计数语义强制区分"新开资源"与"过户"——06 号错误的疫苗
- **统一赋值按值收参**：形参的构造（可能抛）在进入函数体之前完成，
  函数体内只剩绝不抛的 swap——强保证三段论的现成教材
- **operator= 标 noexcept 是合法的**：形参构造发生在调用点，不属于函数体；
  于是 `is_nothrow_copy_assignable=false`（实参拷贝可抛）与
  `is_nothrow_move_assignable=true`（实参移动不抛）两个特征同时成立
- **ScopedLock 用 =delete 而非只声明不定义**：=delete 把误用变成编译错误
  （只声明私有则是链接错误，报错晚且难懂）

## 评分要点

- live 计数全链精确（+1/+1/不变/-1，归零）；深拷贝证据成立
- 构造失败、容量下拷贝赋值失败均抛 runtime_error 且状态/计数不漂移
- 强保证实证：`x = y` 失败后 x 原内容完好
- 饱和下移动赋值成功；moved-from 的 endpoint() 为空串且可析构
- 统一赋值形式正确（按值 + swap，无自赋值特判、无手动 delete）
- ScopedLock：=delete、移动 noexcept、holds() 语义正确
- 简答题四问要点齐全（见下）
- 质量要求：类零输出、注释中文、无冗余状态（06 的 isFp 教训不重犯）

## 简答题要点（判题对照）

- SR1：判据 = **拷贝的语义是否说得通**。复制连接 = 再要一条新连接，说得通（贵但合法）；
  复制"已持有的锁" = 两个对象都认为该由自己 unlock，或各自阻塞等同一把锁——
  语义荒谬。通用准则：资源类先问"拷贝意味着什么"，答不出或答案是荒谬的 → delete；
  答案是"重新获取一份" → 深拷贝（或考虑禁拷贝只留移动，性能更优）
- SR2：**重载决议发生在形参初始化那一步**——左值实参走拷贝构造、右值实参走移动构造，
  同一个函数体吃两种赋值。自赋值：形参是副本，swap 后自身内容不变，天然正确；
  代价 = 多付一次拷贝（自赋值时省不掉的那次）
- SR3：**所有可能抛异常的操作（形参的构造）都发生在修改自身之前**；
  进入函数体后只剩指针交换，绝不抛。失败时自身一字未动——这就是强保证
- SR4：移动 = 所有权过户，不分配新资源、不向服务器要任何东西，
  所以容量与它无关；计数不变同理。它"天生不可能失败"，这正是它敢承诺
  noexcept 的原因——noexcept 的前提是"无获取、无分配、只交换"

## 常见错误预测（判题重点检查）

1. **【复习首查，06 重犯项】拷贝又掏空源 / 赋值不释放旧资源 / 先改状态后检查**——
   copy-and-swap 结构下若手痒写成引用形参或函数体里手动 delete，立即暴露
2. live 计数放错位置：移动构造 +1（拷贝≠移动的资源语义没立住）；
   moved-from 析构也减计数（双重递减）
3. 统一赋值写成 `operator=(const Conn&)`（丢移动路径）或函数体里手动
   delete[]（没理解 swap 让形参析构带走旧资源）
4. swap 忘标 noexcept（破坏统一赋值的 noexcept 链）
5. ScopedLock 拷贝写成"声明不定义"或私有化而不 =delete（报错晚、语义弱）
6. 容量检查放在 new 之后（失败路径需要手动归还，06 SR1 的教训重考）
7. ++live_ 放在抛异常路径之后或分配之前（计数漂移）
8. holds() 只看 bool 不看指针（moved-from 双真相，06 的 isFp 变体重犯）

## 模式点拨（判题时向用户点明）

- **copy-and-swap 统一赋值**：一个 operator= 吃掉拷贝/移动两种赋值，
  强保证自动成立；代价是自赋值多一次拷贝。适用：资源类、追求正确性优先
- **禁拷贝判据**：先问"拷贝意味着什么"——语义荒谬就 =delete；
  std::lock_guard（不可移动）与 unique_lock（可移动）就是两条出路的官方示范
- **静态活计数 = RAII 泄漏探测器**：把生命周期正确性压缩成一个可断言的数字，
  生产代码里同款思路叫 RAII 计数/泄漏审计
- **move ≠ acquire**：过户不是新开——这是 noexcept、容量豁免、计数不变的共同根源；
  06 的"移动接管计数"与 07 的"移动计数不变"是同一件事在两种资源上的投影
