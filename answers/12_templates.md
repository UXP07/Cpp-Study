# 练习 12 标准答案 —— 模板：特化、偏特化与推断（知识点 #4）

## 参考实现

```cpp
// 练习 12 —— 模板：特化、偏特化与推断
#include <cstring>

// ========= Part A：函数模板 + 全特化 =========

// 主模板：按值收参（字符数组在这里退化成 const char*，推断才统一）
template <typename T>
bool less_than(T a, T b) {
    return a < b;
}

// 全特化：const char* 按内容比字典序——通用版比的是地址，结果看运气
template <>
bool less_than<const char*>(const char* a, const char* b) {
    return std::strcmp(a, b) < 0;
}

// ========= Part B：类模板 + 偏特化 =========

// 验证载荷：活计数
class Token {
public:
    Token() { ++live_; }
    ~Token() { --live_; }
    Token(const Token&) = delete;
    Token& operator=(const Token&) = delete;
    static int live() { return live_; }
private:
    inline static int live_ = 0;
};

// 主模板：按值持有——值语义，随便拷
template <typename T>
class Holder {
public:
    Holder() = default;
    explicit Holder(const T& v) : v_(v) {}
    const T& get() const { return v_; }
private:
    T v_{};
};

// 偏特化：T 是任意指针时走这套——换掉的不只是实现，是整个契约（RAII 所有权）
template <typename T>
class Holder<T*> {
public:
    Holder() = default;
    explicit Holder(T* p) : p_(p) {}
    ~Holder() { delete p_; }

    Holder(const Holder&) = delete;
    Holder& operator=(const Holder&) = delete;

    Holder(Holder&& other) noexcept : p_(other.p_) {
        other.p_ = nullptr;
    }
    Holder& operator=(Holder&& other) noexcept {
        if (this != &other) {
            delete p_;                       // 放旧（判空由 delete nullptr 合法保证）
            p_ = other.p_;
            other.p_ = nullptr;
        }
        return *this;
    }

    T* get() const { return p_; }
    T* release() {                           // 交出指针、解除责任
        T* p = p_;
        p_ = nullptr;
        return p;
    }
    explicit operator bool() const { return p_ != nullptr; }

private:
    T* p_ = nullptr;                         // NSDMI：空状态从出生就成立
};

int main() {
    return 0;  // 验证由判题端驱动完成
}
```

## 判题驱动（判题端使用，放 build/tmp，判完删除）

```cpp
// drive12.cpp
#include <cstdio>
#include <type_traits>
#include <utility>
#define main user_main
#include "../../exercises/12_templates/main.cpp"
#undef main

static int fails = 0;
static int total = 0;
#define EXPECT(cond) do { ++total; if (!(cond)) { ++fails; \
    std::printf("[FAIL] line %d: %s\n", __LINE__, #cond); } } while (0)

// 编译期特征
static_assert(std::is_copy_constructible_v<Holder<int>>, "holder<int> copyable");
static_assert(!std::is_copy_constructible_v<Holder<Token*>>, "holder<ptr> not copyable");
static_assert(std::is_nothrow_move_constructible_v<Holder<Token*>>, "ptr move noexcept");
static_assert(std::is_same_v<decltype(less_than(3, 7)), bool>, "returns bool");

int main() {
    // ===== A. less_than：数字 =====
    EXPECT(less_than(3, 7) == true);
    EXPECT(less_than(7, 3) == false);
    EXPECT(less_than(2.5, 2.6) == true);

    // ===== B. 全特化：内容比较，不是地址比较 =====
    const char buf1[] = "alpha";                   // 内容相同、地址不同的两个数组
    const char buf2[] = "alpha";
    EXPECT(buf1 != buf2);                    // 前提：地址确实不同
    EXPECT(less_than(buf1, buf2) == false);  // 特化版：内容相等 → false
    EXPECT(less_than(buf2, buf1) == false);
    const char buf3[] = "beta";
    EXPECT(less_than(buf1, buf3) == true);   // 字典序 alpha < beta
    EXPECT(less_than(buf3, buf1) == false);

    // ===== C. Holder<int>：值语义 =====
    Holder<int> h(5);
    Holder<int> h2 = h;                      // 随便拷
    EXPECT(h2.get() == 5 && h.get() == 5);

    // ===== D. Holder<Token*>：RAII 契约 =====
    EXPECT(Token::live() == 0);
    {
        Holder<Token*> p(new Token);         // 接管
        EXPECT(Token::live() == 1 && p);
        Token* raw = p.release();            // 交出责任
        EXPECT(Token::live() == 1 && !p && raw != nullptr);
        delete raw;
        EXPECT(Token::live() == 0);
        p = Holder<Token*>(new Token);       // 移动赋值复活
        EXPECT(Token::live() == 1);
        Holder<Token*> q = std::move(p);     // 移动转移
        EXPECT(Token::live() == 1 && !p && q);
    }                                        // q 析构自动 delete
    EXPECT(Token::live() == 0);

    if (fails == 0) {
        std::printf("[PASS] %d checks passed\n", total);
    } else {
        std::printf("[FAIL] %d/%d failed\n", fails, total);
    }
    return fails == 0 ? 0 : 1;
}
```

```cpp
// probe12_neg.cpp —— 负向探针：推断冲突必须编译失败（判题端单独编译）
#define main user_main
#include "../../exercises/12_templates/main.cpp"
#undef main
int main() { return less_than(3, 7.0) ? 0 : 1; }
```

## 预期结果

驱动 `[PASS] 16 checks passed`，退出码 0；4 条编译期 static_assert 成立；
**负向探针编译必须失败**（推断冲突：int vs double）。

## 设计意图

- **全特化选 const char\***：通用 `<` 比地址是真实系统里存在的坑（两个内容
  相同的字符串比较结果看内存布局心情）——用"内容相同地址不同"的锚点让
  特化的必要性被机器证明，不是背语法
- **按值收参**：一是让字符数组的退化（`char[6]` → `const char*`）发生在
  推断处（SR1 的教材），二是全特化签名干净（`const char*` 直传，
  不用写 `const char* const&`）
- **偏特化 Holder<T\*> 换契约而非实现**：主模板值语义随便拷，指针版整个
  换成 RAII 所有权——展示偏特化的真实用途：形状不同 → 语义不同
- **负向探针**：`less_than(3, 7.0)` 编译失败是**特性**（推断不做隐式调和，
  泛型代码因此不会悄悄产生精度转换）——只能用"编译失败"验证

## 评分要点

- less_than 数字正确；**内容相同地址不同 → false**（特化命中的铁证）；
  字典序正确；`"ab"` 字面量调用命中特化
- 负向探针：`less_than(3, 7.0)` 不得编译
- Holder<int> 可拷贝且互不影响；Holder<Token*> 全套 RAII：不可拷贝、
  noexcept 移动、release 解责、析构自动 delete（live 归零）、operator bool
- 偏特化语法正确（`template <typename T> class Holder<T*>`），
  不是另起类名
- 简答题三问要点齐全（见下）；零输出零警告；交题前跑过构建

## 简答题要点（判题对照）

- SR1：`less_than(3, 7)` → T=int；`less_than(3, 7.0)` → 一个参数推 int、
  一个推 double，**模板推断不做隐式转换调和** → 编译失败；
  修法一：显式 `less_than<double>(3, 7.0)`；修法二：让实参类型一致
  （`less_than(3.0, 7.0)` 或先转换）。`less_than("ab", "cd")` → T=const char*，
  字符数组在按值传参时**退化**成首元素指针（数组不是一等公民，
  传的就是地址）。进阶一刀：**可变** char 数组（`char buf[6]`）退化成
  `char*`，不吃 `const char*` 特化——特化接不住可变缓冲（要 const 数组，
  或为 `char*` 再写一份）；这是标准库字符串比较一律收 `const char*`
  的原因之一。判题驱动用 const 数组做锚点即为此
- SR2：全特化 = 指名道姓匹配**一个**类型（const char*）；偏特化 = 匹配
  **一类形状**（所有 T*、所有数组等），仍是模板（T 还没定死）。
  函数模板**没有偏特化**——语言不允许，用**重载**代替（多个函数模板
  或非模板函数参与重载决议）。Holder<T*> 换的是**契约**：主模板是值语义
  （可拷贝、不管理资源），指针版是所有权语义（禁拷贝、RAII 释放）——
  接口行为、生命周期责任全部不同，不只是"算法换了"
- SR3：两份——每种 T 独立实例化一份完整类（代码膨胀的由来；好处是
  每份都是为具体类型优化过的直线代码，坏处是二进制变大、编译变慢）。
  模板整个放头文件：**实例化发生在用到它的那个 .cpp 里**，编译器生成
  Holder<int> 的代码时必须看到类的完整定义——实现放别的 cpp，
  使用处编译器看不到定义，只能假设别处有（普通函数有链接器兜底，
  模板实例每个 cpp 生成自己的，没人预先生成过"通用版"）→ 链接错误
  （undefined reference to Holder<...>::...）

## 常见错误预测（判题重点检查）

1. 全特化语法错：忘写 `template <>`，或写成偏特化形状
   `template <typename T> bool less_than<T*>(...)`（函数模板不允许）
2. 特化签名与主模板推断结果不匹配（主模板按值、特化写成 const 引起
   不匹配/变成另一个重载）
3. 偏特化另起类名（HolderPtr）糊弄——static_assert 与实例化直接抓
4. Holder<T*> 移动赋值忘删旧指针 / release 忘置空（老三样第 N 次查）
5. 主模板 get() 返回值拷贝而非 const 引用（02 的账）
6. 负向探针意外编译通过：说明参数写成了模板允许混合推断的形状
   （如两个不同模板参数 T1/T2——那就不是"推断不做调和"的语义了）
7. 交题前不跑构建（11 的流程账，清单第一项）

## 模式点拨（判题时向用户点明）

- **特化三问**：这个类型走通用版会错吗（const char* 比地址）？
  错在语义还是性能？语义错→特化/重载；只是性能→通常不值得
- **偏特化 = 换契约**：主模板与偏特化的关系是"同名不同约"——
  标准库的 vector<bool>（偏特化成位压缩）就是这个故事的官方版
- **推断不做隐式调和**是特性不是缺陷：泛型代码里类型必须精确对上，
  混合类型请显式写——防的是悄悄的精度转换
- **头文件纪律**：模板 = 头文件居民。以后写库（波次 8/9 的组件），
  模板类一律 header-only，或显式实例化（.cpp 里 template class Vec<int>;
  一份，链接器才找得到）
