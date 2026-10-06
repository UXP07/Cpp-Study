# 练习 08 标准答案 —— 移动语义实验室（知识点 #2）

## 参考实现

```cpp
// 练习 08 —— 移动语义实验室
#include <cstddef>

// Part A：只移动的内存块（行情快照）
class Blob {
public:
    // 获取：分配 + 按规则填充；live +1
    Blob(std::size_t n, unsigned seed) : data_(new unsigned char[n]), size_(n) {
        for (std::size_t i = 0; i < n; ++i) {
            data_[i] = static_cast<unsigned char>(seed + i);
        }
        ++live_;
    }

    Blob(const Blob&) = delete;             // 快照复制语义不成立：禁
    Blob& operator=(const Blob&) = delete;

    // 移动 = 过户：不分配，noexcept 是"无获取"的收据
    Blob(Blob&& other) noexcept : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
        ++moves_;
    }

    Blob& operator=(Blob&& other) noexcept {
        if (this != &other) {               // 自移动安全：状态不变
            if (data_ != nullptr) {         // 先放自己的旧块（live 跟所有权走）
                delete[] data_;
                --live_;
            }
            data_ = other.data_;
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
            ++moves_;
        }
        return *this;
    }

    ~Blob() {
        if (data_ != nullptr) {             // moved-from 不减计数
            delete[] data_;
            --live_;
        }
    }

    // moved-from 约定：valid=false / size=0 / checksum=0，可析构、可复活
    bool valid() const { return data_ != nullptr; }
    std::size_t size() const { return size_; }
    unsigned long long checksum() const {
        unsigned long long s = 0;
        for (std::size_t i = 0; i < size_; ++i) {
            s += data_[i];
        }
        return s;
    }

    static long long moves() { return moves_; }
    static long long live() { return live_; }
    static void reset() { moves_ = 0; live_ = 0; }

private:
    unsigned char* data_;                   // 唯一资源
    std::size_t size_;
    inline static long long moves_ = 0;
    inline static long long live_ = 0;
};

// Part B：计数小白鼠——移动构造故意不 noexcept（实验变量）
class Tracer {
public:
    Tracer() = default;
    Tracer(const Tracer&) { ++copies_; }    // 拷贝记账
    Tracer(Tracer&&) { ++moves_; }          // 故意不标 noexcept：让 vector 扩容退化为拷贝

    static long long copies() { return copies_; }
    static long long moves() { return moves_; }
    static void reset() { copies_ = 0; moves_ = 0; }

private:
    inline static long long copies_ = 0;
    inline static long long moves_ = 0;
};

int main() {
    return 0;  // 验证由判题端驱动完成
}
```

## 判题驱动（判题端使用，放 build/tmp，判完删除）

```cpp
// drive08.cpp
#include <cstdio>
#include <type_traits>
#include <utility>
#include <vector>
#define main user_main
#include "../../exercises/08_move_semantics/main.cpp"
#undef main

static int fails = 0;
static int total = 0;
#define EXPECT(cond) do { ++total; if (!(cond)) { ++fails; \
    std::printf("[FAIL] line %d: %s\n", __LINE__, #cond); } } while (0)

// 编译期特征
static_assert(!std::is_copy_constructible_v<Blob>, "blob not copyable");
static_assert(!std::is_copy_assignable_v<Blob>, "blob not copy-assignable");
static_assert(std::is_nothrow_move_constructible_v<Blob>, "blob move ctor noexcept");
static_assert(std::is_nothrow_move_assignable_v<Blob>, "blob move assign noexcept");
static_assert(std::is_copy_constructible_v<Tracer>, "tracer copyable");
static_assert(!std::is_nothrow_move_constructible_v<Tracer>,
              "实验变量：Tracer 移动构造必须非 noexcept");

static int take_val(Tracer t) { (void)t; return 0; }   // 按值传参探针

// 与 Blob 填充规则一致的期望校验和
static unsigned long long expect_sum(std::size_t n, unsigned seed) {
    unsigned long long s = 0;
    for (std::size_t i = 0; i < n; ++i) {
        s += static_cast<unsigned char>(seed + i);
    }
    return s;
}

int main() {
    // 1. 重载决议：左值拷贝、std::move 移动
    Tracer::reset();
    Tracer a;
    [[maybe_unused]] Tracer b = a;
    [[maybe_unused]] Tracer c = std::move(a);
    EXPECT(Tracer::copies() == 1);
    EXPECT(Tracer::moves() == 1);

    // 2. C++17 保证消除：prvalue 直接在目标位置物化
    Tracer::reset();
    Tracer d = Tracer();
    EXPECT(Tracer::copies() == 0 && Tracer::moves() == 0);
    Tracer::reset();
    take_val(Tracer{});
    EXPECT(Tracer::copies() == 0 && Tracer::moves() == 0);
    take_val(std::move(d));
    EXPECT(Tracer::moves() == 1 && Tracer::copies() == 0);

    // 3. Tracer 扩容路径：非 noexcept 移动 -> 搬迁全走拷贝（move_if_noexcept）
    Tracer::reset();
    {
        std::vector<Tracer> v;
        for (int i = 0; i < 4; ++i) v.push_back(Tracer{});
        EXPECT(Tracer::moves() == 4);      // 4 次落位：param -> 槽位各 1 次移动
        EXPECT(Tracer::copies() == 3);     // 扩容搬迁 1+2：全部拷贝！
    }

    // 4. Blob 生命周期：过户 / 复活 / 释放旧块 / 自移动
    Blob::reset();
    {
        Blob x(64, 7);
        EXPECT(Blob::live() == 1 && Blob::moves() == 0);
        EXPECT(x.checksum() == expect_sum(64, 7));
        Blob y = std::move(x);             // 过户
        EXPECT(Blob::moves() == 1 && Blob::live() == 1);
        EXPECT(y.checksum() == expect_sum(64, 7));
        EXPECT(!x.valid() && x.size() == 0 && x.checksum() == 0);
        x = Blob(32, 9);                   // 复活
        EXPECT(x.valid() && x.checksum() == expect_sum(32, 9));
        EXPECT(Blob::live() == 2);
        Blob z(16, 3);
        EXPECT(Blob::live() == 3);
        z = std::move(x);                  // z 放旧块接管 x 的
        EXPECT(Blob::live() == 2);
        EXPECT(z.checksum() == expect_sum(32, 9));
        EXPECT(!x.valid());
        Blob& rz = z;
        z = std::move(rz);                 // 自移动：状态不变
        EXPECT(z.valid() && z.checksum() == expect_sum(32, 9));
        EXPECT(Blob::live() == 2);
    }
    EXPECT(Blob::live() == 0);             // 泄漏探测器归零

    // 5. Blob 扩容路径：noexcept 移动 -> 搬迁全走移动，零拷贝
    Blob::reset();
    {
        std::vector<Blob> v;
        for (int i = 0; i < 4; ++i) v.push_back(Blob(64, 1 + i));
        EXPECT(Blob::live() == 4);
        EXPECT(Blob::moves() == 7);        // 4 落位 + 3 搬迁：全移动
        EXPECT(v[0].checksum() == expect_sum(64, 1));
        EXPECT(v[1].checksum() == expect_sum(64, 2));
        EXPECT(v[2].checksum() == expect_sum(64, 3));
        EXPECT(v[3].checksum() == expect_sum(64, 4));
    }
    EXPECT(Blob::live() == 0);

    if (fails == 0) {
        std::printf("[PASS] %d checks passed\n", total);
    } else {
        std::printf("[FAIL] %d/%d failed\n", fails, total);
    }
    return fails == 0 ? 0 : 1;
}
```

## 预期结果

驱动全部 EXPECT 通过，`[PASS] 28 checks passed`，退出码 0；
6 条编译期 static_assert 全部成立。

## 设计意图

- **计数器代替输出**：把"哪次拷贝、哪次移动"变成可断言的数字——重载决议与扩容路径
  从"听说"变成"实测"
- **Tracer 移动故意非 noexcept**：制造 move_if_noexcept 的分岔条件。vector 扩容时
  它明明有移动构造，却被迫逐个拷贝搬迁（1+2=3 次）——noexcept 的价值第一次被
  数字打在脸上
- **Blob 与 Tracer 对照**：同样的 vector 操作，Blob 7 次移动零拷贝——
  "删拷贝 + noexcept 移动"就是 vector 最爱的形状
- **C++17 消除探针**（`Tracer d = Tracer();` 与 `take_val(Tracer{})` 计 0）：
  区分"prvalue 直接物化"与"先构造再移动"的分界线是实参是不是纯右值
- **自移动安全**：`z = std::move(rz)` 场景小众但属于移动赋值的完备契约

## 评分要点

- Blob：只移动（拷贝删除）、两移动 noexcept、moved-from 三零约定、复活、
  释放旧块时 live 精确、自移动安全、checksum const
- Tracer：拷贝/移动计数准确、移动构造非 noexcept（static_assert 把关）
- 计数语义：moves 只在真实过户时 +1（自移动不算）；live 跟所有权不跟对象个数
- 简答题 SR3 的预测数字：Tracer 4 移 3 拷、Blob 7 移——预测+原理双得分
- 质量要求：零输出、注释中文、无冗余状态

## 简答题要点（判题对照）

- SR1：std::move 只是 `static_cast<T&&>`——编译期类型转换，零运行期动作，
  什么都不移动；作用是把表达式变成 xvalue，让重载决议选中移动版本。
  a 还能用：它是被自己类里的移动构造置空的，Tracer 无资源，状态完好；
  "合法但值未指定"是标准给的底线，具体状态由类的作者（你）定义
- SR2：`Tracer()` 是纯右值（prvalue），C++17 保证它**直接在 d 的位置物化**，
  不产生临时对象、不发生任何拷贝/移动（guaranteed copy elision）。
  分界线：实参是纯右值（直接物化，0 次）还是经 std::move 的 xvalue（已是
  物化对象，需要一次移动构造进形参，1 次）
- SR3：**Tracer：moves=4、copies=3；Blob：moves=7**。规则：扩容搬迁用
  move_if_noexcept——移动构造 noexcept **或** 拷贝不可用 → 搬移动；否则搬拷贝。
  为什么拷贝可时不搬可能抛的移动：搬到一半抛异常，旧元素已被掏空无法回滚，
  vector 的强异常保证碎掉；拷贝抛了可以原地复原
- SR4：moved-from Blob = valid() 假、size 0、checksum 0——"有效但未指定"的
  **具体化**：本类作者把未指定状态定义为"空"。允许：析构、被移动赋值（复活）、
  查询（返回零值不崩）。使用错误：把它当有效快照读数据。设计资源类时应像本题
  一样显式约定并文档化，而不是留给人猜

## 常见错误预测（判题重点检查）

1. Blob 移动赋值忘记释放自身旧块（06/07 同款错误第三现）或忘自移动检查
2. moves 计数挂错位置：自移动也 +1、或移动构造忘 +1
3. moved-from 忘置空 → dtor 双重递减 live（07 同款计数漂移）
4. Tracer 手滑给移动构造标了 noexcept（实验变量被毁，static_assert 直接抓）
5. Blob 用 string/vector/智能指针存数据（违反裸数组约束）
6. 查询接口忘 const（06/07 老毛病第三次查）
7. 把 `Tracer d = Tracer();` 预测成 1 次移动（SR2：C++17 消除）
8. SR3 预测 vector 搬迁走移动（没建立 move_if_noexcept 规则）

## 模式点拨（判题时向用户点明）

- **move_if_noexcept 分岔**：vector 扩容 = noexcept 移动 ? 移动 : (可拷贝 ? 拷贝 : 移动)。
  这是 06 SR3"搬一半炸了回不去"的机器证明版
- **只移动类型**（copyable=false, movable=true）：unique_ptr/lock_guard/thread 的共同形状；
  "复制语义荒谬或昂贵"时的标准答案
- **计数器实验室**：copies/moves/live 三个静态计数是自测利器——以后写任何资源类，
  花五分钟加个计数，路径选择立刻从玄学变实测
- **prvalue 直接物化**（C++17 消除）：返回局部对象、按值传纯右值都零成本；
  但经 std::move 的对象是 xvalue，必然付一次移动——**别对纯右值用 std::move**
  （`return std::move(local)` 反模式的根源）
