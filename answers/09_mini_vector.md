# 练习 09 标准答案 —— 迷你 Vec<T>（对拍 std::vector）

## 参考实现

```cpp
// 练习 09 —— 迷你 Vec<T>：内存与对象分离 + 两笔账的完整实现
#include <cstddef>
#include <new>       // placement-new
#include <utility>   // std::move / std::move_if_noexcept

template <typename T>
class Vec {
public:
    Vec() = default;

    Vec(const Vec&) = delete;            // 本容器只移动
    Vec& operator=(const Vec&) = delete;

    // 移动 = 换三个指针/整数：O(1)、noexcept、不触碰任何元素
    Vec(Vec&& other) noexcept
        : buf_(other.buf_), size_(other.size_), cap_(other.cap_) {
        other.steal_reset();
    }

    Vec& operator=(Vec&& other) noexcept {
        if (this != &other) {
            destroy_all();               // 先放掉自己的旧内存与元素
            buf_ = other.buf_;
            size_ = other.size_;
            cap_ = other.cap_;
            other.steal_reset();
        }
        return *this;
    }

    ~Vec() { destroy_all(); }

    void reserve(std::size_t new_cap) {
        if (new_cap <= cap_) {
            return;                      // 不缩容
        }
        reallocate(new_cap);
    }

    // 落位两重载：调用点选谁，槽位构造就照谁来
    void push_back(const T& v) {         // 左值：拷贝构造
        ensure_space();
        ::new (buf_ + size_) T(v);
        ++size_;
    }

    void push_back(T&& v) {              // 右值：移动构造
        ensure_space();
        ::new (buf_ + size_) T(std::move(v));
        ++size_;
    }

    T& operator[](std::size_t i) { return buf_[i]; }
    const T& operator[](std::size_t i) const { return buf_[i]; }
    std::size_t size() const { return size_; }
    std::size_t capacity() const { return cap_; }

private:
    void ensure_space() {
        if (size_ == cap_) {
            reallocate(cap_ ? cap_ * 2 : 1);   // 空→1，之后翻倍（与 std::vector 一致）
        }
    }

    void reallocate(std::size_t new_cap) {
        T* nb = static_cast<T*>(::operator new(new_cap * sizeof(T)));  // 生内存（自带对齐）
        for (std::size_t i = 0; i < size_; ++i) {
            ::new (nb + i) T(std::move_if_noexcept(buf_[i]));          // 搬迁：与 std::vector 同规则
        }
        release_buffer(buf_, size_);     // 销毁旧壳、释放旧内存——不动成员（size_ 必须保住）
        buf_ = nb;
        cap_ = new_cap;
    }

    // 释放一段缓冲：逐个显式析构 + 整段内存归还；不触碰任何成员
    static void release_buffer(T* p, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i) {
            p[i].~T();
        }
        ::operator delete(p);
    }

    void destroy_all() {
        release_buffer(buf_, size_);
        steal_reset();
    }

    void steal_reset() {                 // 被移动/清空后的统一置空
        buf_ = nullptr;
        size_ = 0;
        cap_ = 0;
    }

    T* buf_ = nullptr;
    std::size_t size_ = 0;
    std::size_t cap_ = 0;
};

int main() {
    return 0;  // 验证由判题端驱动完成
}
```

## 判题驱动（判题端使用，放 build/tmp，判完删除；自动引入 08 的 Blob/Tracer 做对拍）

```cpp
// drive09.cpp —— 对拍驱动：同一场景跑 std::vector 与 Vec，计数必须一致
#include <cstdio>
#include <type_traits>
#include <utility>
#include <vector>
#define main main_08
#include "../../exercises/08_move_semantics/main.cpp"
#undef main
#define main main_09
#include "../../exercises/09_mini_vector/main.cpp"
#undef main

static int fails = 0;
static int total = 0;
#define EXPECT(cond) do { ++total; if (!(cond)) { ++fails; \
    std::printf("[FAIL] line %d: %s\n", __LINE__, #cond); } } while (0)

// 编译期特征
static_assert(!std::is_copy_constructible_v<Vec<Blob>>, "vec not copyable");
static_assert(!std::is_copy_assignable_v<Vec<Blob>>, "vec not copy-assignable");
static_assert(std::is_nothrow_move_constructible_v<Vec<Blob>>, "vec move ctor noexcept");
static_assert(std::is_nothrow_move_assignable_v<Vec<Blob>>, "vec move assign noexcept");

static unsigned long long expect_sum(std::size_t n, unsigned seed) {
    unsigned long long s = 0;
    for (std::size_t i = 0; i < n; ++i) {
        s += static_cast<unsigned char>(seed + i);
    }
    return s;
}

int main() {
    // ===== 场景 1：Blob 右值 push ×4（基线：std::vector）=====
    long long m0 = (long long)Blob::moves();
    long long l0 = (long long)Blob::live();
    long long mid_std = 0, mid_my = 0;
    {
        std::vector<Blob> v;
        for (int i = 0; i < 4; ++i) v.push_back(Blob(64, 1 + i));
        mid_std = (long long)Blob::live() - l0;
        EXPECT(v[3].checksum() == expect_sum(64, 4));
    }
    const long long moves_std = (long long)Blob::moves() - m0;
    {
        Vec<Blob> v;
        for (int i = 0; i < 4; ++i) v.push_back(Blob(64, 1 + i));
        mid_my = (long long)Blob::live() - l0;
        EXPECT(v.size() == 4 && v.capacity() == 4);
        EXPECT(v[0].checksum() == expect_sum(64, 1));
        EXPECT(v[3].checksum() == expect_sum(64, 4));
    }
    long long moves_my = (long long)Blob::moves() - m0 - moves_std;
    std::printf("[info] blob push4 : std moves=%lld my moves=%lld (live mid std=%lld my=%lld)\n",
                moves_std, moves_my, mid_std, mid_my);
    EXPECT(moves_my == moves_std);       // 对拍：计数一字不差
    EXPECT(mid_my == mid_std);
    EXPECT((long long)Blob::live() == l0);

    // ===== 场景 2：Tracer 右值 push ×4（重点：搬迁退化为拷贝）=====
    long long tc0 = (long long)Tracer::copies(), tm0 = (long long)Tracer::moves();
    long long tc_std = 0, tm_std = 0;
    {
        std::vector<Tracer> v;
        for (int i = 0; i < 4; ++i) v.push_back(Tracer{});
        tc_std = (long long)Tracer::copies() - tc0;
        tm_std = (long long)Tracer::moves() - tm0;
    }
    long long tc1 = (long long)Tracer::copies(), tm1 = (long long)Tracer::moves();
    {
        Vec<Tracer> v;
        for (int i = 0; i < 4; ++i) v.push_back(Tracer{});
        EXPECT((long long)Tracer::copies() - tc1 == tc_std);   // 我的 copies 与 std 相等
        EXPECT((long long)Tracer::moves() - tm1 == tm_std);    // 我的 moves 与 std 相等
    }
    std::printf("[info] tracer push4: std copies=%lld moves=%lld (Vec 已对拍相等)\n",
                tc_std, tm_std);
    EXPECT(tc_std == 3 && tm_std == 4);  // 锚点数字

    // ===== 场景 3：左值 push 走拷贝、源不受影响 =====
    Tracer::reset();
    {
        Vec<Tracer> v;
        Tracer t;
        v.push_back(t);
        v.push_back(t);
        EXPECT(Tracer::copies() >= 2 && Tracer::moves() == 0);  // 至少 2 次拷贝落位，零移动
    }

    // ===== 场景 4：reserve 预留 → 搬迁消失 =====
    long long m4 = (long long)Blob::moves();
    {
        Vec<Blob> v;
        v.reserve(4);
        for (int i = 0; i < 4; ++i) v.push_back(Blob(64, 10 + i));
        EXPECT((long long)Blob::moves() - m4 == 4);  // 只有落位，无搬迁
        EXPECT(v.capacity() == 4);
        EXPECT(v[3].checksum() == expect_sum(64, 13));
    }
    EXPECT((long long)Blob::live() == 0);

    // ===== 场景 5：Vec 自身的移动（O(1) 过户，元素零搬运）=====
    {
        Vec<Blob> a;
        a.push_back(Blob(64, 5));        // 落位 +1
        long long mb = (long long)Blob::moves(), lb = (long long)Blob::live();
        Vec<Blob> b = std::move(a);      // 移动构造：计数零变化
        EXPECT((long long)Blob::moves() == mb && (long long)Blob::live() == lb);
        EXPECT(a.size() == 0 && a.capacity() == 0);
        EXPECT(b.size() == 1 && b[0].checksum() == expect_sum(64, 5));

        Vec<Blob> c;
        c.push_back(Blob(32, 6));        // c 的旧块将被正确释放
        c = std::move(b);                // 移动赋值
        EXPECT(c.size() == 1 && c[0].checksum() == expect_sum(64, 5));
        EXPECT(b.size() == 0);
    }
    EXPECT((long long)Blob::live() == 0);  // 全部归还：泄漏探测器归零

    if (fails == 0) {
        std::printf("[PASS] %d checks passed\n", total);
    } else {
        std::printf("[FAIL] %d/%d failed\n", fails, total);
    }
    return fails == 0 ? 0 : 1;
}
```

## 预期结果

`[PASS] 21 checks passed`，退出码 0；对拍信息行显示
`blob push4 : std moves=7 my moves=7`、`tracer push4: std copies=3 moves=4`。

## 设计意图

- **对拍验收**（differential testing）：不手工规定每一步该几个计数，而是同一场景
  跑 std::vector 与 Vec，两边差值必须相等——行为等价即正确，这也是工程里验证
  自研组件对标标准实现的常规手段
- **内存与对象分离**：`::operator new` 拿生内存（对齐由库保证），placement-new
  按需构造、显式析构、整段释放——vector/deque/对象池的共同地基；也是
  "T 无默认构造也能用"的唯一解法
- **搬迁与落位分离**：`ensure_space`（可能搬迁）与槽位构造（两重载各管各的落位）
  分开写，两笔账各归各位
- **`steal_reset()` 私有小函数**：被移动置空与清空释放共用一段逻辑，消灭三行重复
- **移动 O(1) 的根源**：容器本体只有指针/整数，元素在堆上——Blob 三题练的
  "壳与货分离"在容器层面的复用

## 评分要点

- 对拍全过（Blob/Tracer 各场景计数与 std::vector 一致）；锚点数字 7 / 4+3 / 4
- T 无默认构造可用（Vec<Blob> 实例化 = 证明生内存方案）
- 搬迁用 std::move_if_noexcept（Tracer 场景 copies=3 抓错）
- 扩容策略 空→1→×2；reserve 不缩容
- 移动构造/赋值 noexcept 且 O(1)（场景 5：计数零变化）
- 析构：先逐个显式析构再释放整段；live 归零
- 两版 operator[]；零输出；注释中文；无泄漏无双重释放

## 常见错误预测（判题重点检查）

1. 用 `new T[cap]` 开内存——要求 T 默认构造 + 语义错误（Vec<Blob> 直接编不过，
   反而当场暴露）
2. push_back 只写一个版本（通常只写 `T&&`）：左值 push 编译失败，或被迫先拷贝
   再 push 导致计数对不上
3. 搬迁直接 `std::move(buf_[i])` 不用 move_if_noexcept：Tracer 场景 copies 对不上
   std::vector（搬迁应是拷贝 3 次）
4. 搬迁后忘销毁旧元素 / 忘释放旧内存（泄漏，live 归零抓不住内存泄漏但
   operator delete 缺失会漏——靠代码审查 + 场景 5 旧块释放检查）
5. 移动赋值忘先 destroy 自己的旧 buffer（场景 5 的 c 旧块泄漏）
6. 用 `new unsigned char[...]` 当缓冲——对齐不保证（UB），约束里已提示用
   ::operator new
7. reserve 允许缩容（new_cap <= cap_ 应直接返回）
8. 08 老三样重犯检查：noexcept 缺失、自移动不安全、释放旧资源漏减计数

## 模式点拨（判题时向用户点明）

- **内存与对象分离**：raw buffer + placement-new + 显式析构——所有自研容器/
  对象池/内存池的第一块地基；波次 9 的定长对象池就是这个模式的泛化
- **对拍测试**（differential testing）：自研组件与标准实现对拍行为，
  是低延迟组件验证的常规姿势（回测引擎对拍交易所数据同理）
- **两重载落位 + move_if_noexcept 搬迁**：从 08 的"观察者"到 09 的"实现者"，
  同一套规则换了立场——这就是"摸到标准库地基"的含义
- **容器 O(1) 移动**：壳（指针/整数）与货（堆上元素）分离——Blob 的教训
  在容器尺度上的复现
