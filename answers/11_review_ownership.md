# 练习 11 标准答案 —— 错账清算（复习 06/07/08/10）

## 参考实现

```cpp
// 练习 11 —— 错账清算：空状态、三同步与所有权选用
#include <cstddef>
#include <cstring>
#include <memory>
#include <utility>

// Part A：行情消息——(ptr, len) 字节缓冲，无终止符
class Msg {
public:
    Msg(const void* data, std::size_t len, int id)
        : buf_(new unsigned char[len]), len_(len), id_(id) {
        std::memcpy(buf_, data, len);          // 字节级拷贝，与终止符无关
        ++live_;
    }
    ~Msg() { delete[] buf_; --live_; }
    Msg(const Msg&) = delete;
    Msg& operator=(const Msg&) = delete;

    std::size_t size() const { return len_; }
    const unsigned char* data() const { return buf_; }
    int id() const { return id_; }
    static int live() { return live_; }

private:
    unsigned char* buf_;
    std::size_t len_;
    int id_;
    inline static int live_ = 0;
};

// 共享引用句柄：Shared 变体 + reset + 语境布尔
template <typename T>
class Ref {
public:
    Ref() = default;
    explicit Ref(T* p) : ptr_(p), count_(p != nullptr ? new long(1) : nullptr) {}

    Ref(const Ref& other) : ptr_(other.ptr_), count_(other.count_) {
        if (count_ != nullptr) {
            ++*count_;                          // 前缀：先解引用再自增值
        }
    }

    Ref(Ref&& other) noexcept : ptr_(other.ptr_), count_(other.count_) {
        other.steal_reset();
    }

    Ref& operator=(const Ref& other) {
        if (this != &other) {
            release();                          // 先放旧份额，再共享对方的
            ptr_ = other.ptr_;
            count_ = other.count_;
            if (count_ != nullptr) {
                ++*count_;
            }
        }
        return *this;
    }

    Ref& operator=(Ref&& other) noexcept {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            count_ = other.count_;
            other.steal_reset();
        }
        return *this;
    }

    ~Ref() { release(); }

    // 主动置空：与析构共用同一条释放路径——空对象上安全，连续调用安全
    void reset() { release(); }

    // 语境布尔：if (ref) 可用；explicit 挡住 ref == 1 / ref + 1 这类误用
    explicit operator bool() const { return ptr_ != nullptr; }

    T* get() const { return ptr_; }
    long use_count() const { return count_ != nullptr ? *count_ : 0; }
    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }

private:
    void release() {
        if (count_ == nullptr) {                // 空状态唯一判定源
            return;
        }
        if (--*count_ == 0) {                   // 最后一份：资源与控制块同亡
            delete ptr_;
            delete count_;
        }
        steal_reset();
    }
    void steal_reset() {
        ptr_ = nullptr;
        count_ = nullptr;
    }

    T* ptr_ = nullptr;                          // NSDMI：= default 不给裸指针清零
    long* count_ = nullptr;
};

// Part B：三种持有姿势——签名即所有权语义
class Owner {                                   // 独占：吞下 unique_ptr（按值）
public:
    explicit Owner(std::unique_ptr<Msg> m) : msg_(std::move(m)) {}
    std::unique_ptr<Msg>& msg() { return msg_; }
private:
    std::unique_ptr<Msg> msg_;                  // 成员不可拷贝 => 类整体自动不可拷贝
};

class Subscriber {                              // 共享：入伙 shared_ptr（按值）
public:
    explicit Subscriber(std::shared_ptr<Msg> s) : msg_(std::move(s)) {}
    std::shared_ptr<Msg>& msg() { return msg_; }
private:
    std::shared_ptr<Msg> msg_;
};

class Observer {                                // 观察：不延长生命周期
public:
    explicit Observer(std::weak_ptr<Msg> w) : watch_(std::move(w)) {}
    std::shared_ptr<Msg> peek() const { return watch_.lock(); }  // 死了给空
private:
    std::weak_ptr<Msg> watch_;
};

int main() {
    return 0;  // 验证由判题端驱动完成
}
```

## 判题驱动（判题端使用，放 build/tmp，判完删除）

```cpp
// drive11.cpp
#include <cstdio>
#include <memory>
#include <type_traits>
#include <utility>
#define main user_main
#include "../../exercises/11_review_ownership/main.cpp"
#undef main

static int fails = 0;
static int total = 0;
#define EXPECT(cond) do { ++total; if (!(cond)) { ++fails; \
    std::printf("[FAIL] line %d: %s\n", __LINE__, #cond); } } while (0)

// 编译期特征
static_assert(std::is_default_constructible_v<Ref<Msg>>, "default");
static_assert(std::is_nothrow_move_constructible_v<Ref<Msg>>, "move noexcept");
static_assert(!std::is_copy_constructible_v<Owner>, "owner not copyable");
static_assert(std::is_move_constructible_v<Owner>, "owner movable");

// explicit operator bool 的负向测试：ref == 1 不得编译
template <typename U, typename = void>
struct has_bad_eq : std::false_type {};
template <typename U>
struct has_bad_eq<U, std::void_t<decltype(std::declval<U>() == 1)>> : std::true_type {};
static_assert(!has_bad_eq<Ref<Msg>>::value, "operator bool must be explicit");

int main() {
    const unsigned char payload[] = {0x01, 0x02, 0x03, 0x04, 0x05};

    // ===== A. Msg：(ptr,len) 内容与 live =====
    EXPECT(Msg::live() == 0);
    {
        Msg m(payload, sizeof payload, 7);
        EXPECT(m.size() == 5 && m.id() == 7);
        EXPECT(std::memcmp(m.data(), payload, 5) == 0);   // 逐字节，无终止符依赖
    }
    EXPECT(Msg::live() == 0);

    // ===== B. Ref 空状态矩阵（主考） =====
    {
        Ref<Msg> e;                                   // 默认构造
        EXPECT(e.use_count() == 0 && e.get() == nullptr && !e);
        Ref<Msg> c = e;                               // 拷贝空
        EXPECT(c.use_count() == 0 && !c && Msg::live() == 0);
        Ref<Msg> mv = std::move(e);                   // 移动空
        EXPECT(mv.use_count() == 0 && !mv && !e);
        Ref<Msg> t;
        t = std::move(c);                             // 对空移动赋值
        EXPECT(!t && !c);
        e.reset();                                    // 空上 reset
        e.reset();                                    // 连续 reset
        EXPECT(!e && Msg::live() == 0);
        Ref<Msg>& re = e;
        e = std::move(re);                            // 自移动
        EXPECT(!e);
    }
    EXPECT(Msg::live() == 0);

    // ===== C. Ref 计数链与 reset 的三同步 =====
    {
        Ref<Msg> a(new Msg(payload, 5, 1));           // 接管
        EXPECT(a.use_count() == 1 && Msg::live() == 1 && a);
        EXPECT(a->id() == 1 && (*a).id() == 1);
        Ref<Msg> b = a;                               // 拷贝
        EXPECT(a.use_count() == 2 && b.use_count() == 2 && Msg::live() == 1);
        b.reset();                                    // 主动放份额：非最后，资源活
        EXPECT(b.use_count() == 0 && !b && Msg::live() == 1);
        a.reset();                                    // 最后一份：资源死亡恰好一次
        EXPECT(a.use_count() == 0 && !a && Msg::live() == 0);
        Ref<Msg> d(new Msg(payload, 5, 2));
        Ref<Msg> f(new Msg(payload, 5, 3));
        EXPECT(Msg::live() == 2);
        d = f;                                        // 拷贝赋值：放旧 +1 新
        EXPECT(Msg::live() == 1 && d.use_count() == 2 && d->id() == 3);
        Ref<Msg> g = std::move(d);                    // 移动：计数不变
        EXPECT(g.use_count() == 2 && Msg::live() == 1 && !d);
        f = std::move(g);
        EXPECT(f.use_count() == 1 && Msg::live() == 1);  // f 放掉自己份额后独占
    }
    EXPECT(Msg::live() == 0);

    // ===== D. Part B：三种持有姿势 =====
    {
        auto up = std::make_unique<Msg>(payload, 5, 10);
        Owner o1(std::move(up));
        EXPECT(Msg::live() == 1 && o1.msg()->id() == 10);
        Owner o2 = std::move(o1);                     // 独占转移：live 不变
        EXPECT(Msg::live() == 1 && o2.msg()->id() == 10 && !o1.msg());
    }
    EXPECT(Msg::live() == 0);
    {
        auto snap = std::make_shared<Msg>(payload, 5, 20);
        EXPECT(snap.use_count() == 1);
        {
            Subscriber s1(snap);
            Subscriber s2(snap);                      // 两个订阅者共享
            EXPECT(snap.use_count() == 3 && Msg::live() == 1);
            Observer obs(snap);                       // 观察者不计数
            EXPECT(snap.use_count() == 3);
            EXPECT(obs.peek() != nullptr && obs.peek()->id() == 20);
        }
        EXPECT(snap.use_count() == 1);
        Observer obs2(snap);
        snap.reset();                                 // 共享全撤
        EXPECT(Msg::live() == 0);
        EXPECT(obs2.peek() == nullptr);               // 观察者探测到死亡
    }
    EXPECT(Msg::live() == 0);

    if (fails == 0) {
        std::printf("[PASS] %d checks passed\n", total);
    } else {
        std::printf("[FAIL] %d/%d failed\n", fails, total);
    }
    return fails == 0 ? 0 : 1;
}
```

## 预期结果

`[PASS] 32 checks passed`，退出码 0；4 条编译期 static_assert 全部成立
（含 `ref == 1` 不得编译的负向探针）。

## 设计意图

- **空状态矩阵**是本题主考：默认构造/拷贝空/移动空/对空赋值/空 reset×2/空 bool/
  自移动——一次把 07/10/盲写三笔空状态旧账全走一遍，漏一条驱动就红
- **`reset()` 与析构共用 `release()`**：主动置空和自然死亡是同一条代码路径——
  "资源死亡恰好一次"由结构保证，不靠每处记忆（DRY 在资源管理上的形态）
- **`explicit operator bool`**：语境转换（if/&&/|| 可用）+ 挡隐式算术——
  智能指针/流/optional 的通用惯用法，第一次上手
- **Part B 签名即语义**：按值收 unique_ptr = "我要吞"、按值收 shared_ptr =
  "我要入伙"、收 weak_ptr = "我只看"——判据表的代码化
- **负向 static_assert**（`ref == 1` 不得编译）：explicit 的价值只能用
  "编译失败"证明，正向用例证不了

## 评分要点

- 空状态矩阵全安全（判空源唯一：以 count_（或等价不变式）为淮）
- 计数三同步全链：接管/拷贝/移动/reset/赋值放旧/析构，资源死亡恰好一次
- `reset()` 复用释放路径；`explicit operator bool`（负向探针必须过）
- (ptr,len) 存取无终止符依赖（memcmp 逐字节验证）
- Owner 不可拷贝可移动（unique_ptr 成员自动带来）；Subscriber use_count 联动；
  Observer 释放后 peek 空、live 归零
- 零输出零警告；模板实例化验证；**SR 三问必须本人默写**（对照下述要点）

## 简答题要点（判题对照）

- SR1：判据表——唯一→unique（默认首选）；共享→shared（控制块代价）；
  观察→weak（lock 探活）；借用→裸指针/引用（参数"只用不拥有"惯例：
  传 const T&/裸指针，签名不给所有权暗示）。Part B 对应：Owner 吞 unique
  （按值=所有权转移的唯一表达）；Subscriber 收 shared（拷贝即入伙+计数）；
  Observer 收 weak（不延长寿命）
- SR2：死因——先接管后放旧，当新旧同资源或旧份额是最后一份时，release 会
  delete 掉刚接上的资源→悬垂。reset() 里体现：release() 的第一步就是判定并
  减计数（先处置旧我）。reset 与析构：同一条释放路径，reset = 提前手动析构份额，
  析构 = 对象死亡时自动 reset——两者共用代码即"死亡恰好一次"的结构保证
- SR3：空状态性质——ptr/count 皆空（单一判空源）、use_count()==0、
  operator bool 为假、get()==nullptr、可析构、可被拷贝（仍空）、可被移动
  （仍空）、可接收移动赋值（复活）、可 reset（幂等）。从空出发/到达空的操作：
  默认构造、reset、被移动、移动赋值给别的对象后、release 至最后一份后。
  为什么是总疫苗：所有"漏判空"bug 的共同形状=某条路径没有维持
  "空 ⟺ 无资源无计数"这个等价——不变式一旦成立，每个操作只需在入口
  问一次"我是不是空"，不存在第二份需要协调的真相

## 常见错误预测（判题重点检查）

1. **【首查，上次错误】裸指针成员没 NSDMI**（盲写刚犯）→ 默认构造垃圾值，
   空状态矩阵当场崩
2. **【重犯检查】拷贝构造漏判空 / `*count_++` 优先级 / `otr_` 类手误**
   （10 的三笔账逐一核对）
3. (ptr,len) 违规：用 `string(const char*)` 读回无终止符缓冲（07 同款过读）
4. reset() 写成独立逻辑不与析构共用（两份释放代码 = 两处可漏）
5. operator bool 忘 explicit（负向探针失败）或写成 operator int
6. 移动构造动了计数（+1/-1 都错——持有者人数没变）
7. Part B：Subscriber 构造里拷贝 shared_ptr 后又存引用/裸指针（use_count 不联动）；
   Observer 用 shared_ptr 成员（变相持有，释放后 live 不归零——行为直接抓）
8. 模板假绿：Ref 成员手误未实例化——驱动全实例化兜底

## 模式点拨（判题时向用户点明）

- **空状态不变式**：设计句柄类先写"空"的定义（哪些成员全空、判空源唯一），
  再让所有操作维持它——这比"每个函数记得判空"可靠一个数量级
- **release/reset/析构三合一**：主动释放与自然死亡必须同路——
  std::shared_ptr 的 reset、fclose 守卫、对象池归还全是这个形状
- **explicit operator bool**：语境转换惯用法——智能指针/流/optional 通用；
  记法："能 if，不能算术"
- **签名即所有权**：参数类型（按值 unique/shared/weak/裸引用）就是所有权
  合同——读 API 先读签名
