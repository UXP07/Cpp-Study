# 练习 15 标准答案 —— C++17 工具库（知识点 #7）

## 参考实现

```cpp
// 练习 15 —— optional / variant+visit / string_view
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// ===== Q1：optional 查价——"没有"就让它真的是"没有" =====
inline std::optional<int> find_price(
    const std::vector<std::pair<std::string, int>>& table,
    const std::string& sym) {
    for (const auto& row : table) {
        if (row.first == sym) {
            return row.second;           // 有值
        }
    }
    return std::nullopt;                 // 没有（不是 -1，不是 nullptr）
}

// ===== Q2：variant 三态字段 + visit 分发 =====
using Field = std::variant<int, double, std::string>;

inline std::string describe(const Field& f) {
    // 单泛型 lambda + if constexpr：分发在编译期完成类型剪枝（接 14 号）
    return std::visit(
        [](const auto& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, int>) {
                return "int:" + std::to_string(v);
            } else if constexpr (std::is_same_v<T, double>) {
                return "dbl:" + std::to_string(v);
            } else {
                return "str:" + v;
            }
        },
        f);
}

// ===== Q3：类型询问 =====
inline bool is_int_field(const Field& f) {
    return std::holds_alternative<int>(f);
}

// ===== Q4：string_view 零拷贝切片 =====
inline std::string_view first_token(std::string_view line) {
    std::size_t sp = line.find(' ');
    return sp == std::string_view::npos ? line : line.substr(0, sp);
}

inline std::string_view trim(std::string_view s) {
    const char* ws = " \t";
    std::size_t b = s.find_first_not_of(ws);
    if (b == std::string_view::npos) {
        return s.substr(s.size());       // 全空白 → 空 view
    }
    std::size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

int main() {
    return 0;  // 验证由判题端驱动完成
}
```

## 判题驱动（判题端使用，放 build/tmp，判完删除）

```cpp
// drive15.cpp
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#define main user_main
#include "../../exercises/15_cxx17_utils/main.cpp"
#undef main

static int fails = 0;
static int total = 0;
#define EXPECT(cond) do { ++total; if (!(cond)) { ++fails; \
    std::printf("[FAIL] line %d: %s\n", __LINE__, #cond); } } while (0)

// 零拷贝证明一（编译期）：view 只是一对指针
static_assert(sizeof(std::string_view) == 2 * sizeof(void*),
              "string_view must be a ptr+len pair, not a copy");

int main() {
    // ===== optional =====
    std::vector<std::pair<std::string, int>> table{{"AAPL", 170}, {"MSFT", 420}};
    auto hit = find_price(table, "AAPL");
    EXPECT(hit.has_value() && hit.value() == 170);
    EXPECT(!find_price(table, "GOOG").has_value());
    EXPECT(find_price(table, "GOOG").value_or(-1) == -1);
    EXPECT(find_price(table, "MSFT").value_or(-1) == 420);

    // ===== variant + visit =====
    Field fi = 42;
    Field fd = 3.5;
    Field fs = std::string("tick");
    EXPECT(describe(fi) == "int:42");
    EXPECT(describe(fs) == "str:tick");
    EXPECT(describe(fd) == "dbl:" + std::to_string(3.5));
    EXPECT(is_int_field(fi) && !is_int_field(fd) && !is_int_field(fs));

    // ===== string_view：内容 + 零拷贝证明二（指针级） =====
    std::string line = "AAPL 170.25 buy";
    std::string_view tok = first_token(line);
    EXPECT(tok == "AAPL");
    EXPECT(tok.data() >= line.data() && tok.data() < line.data() + line.size());
    EXPECT(first_token("SOLO") == "SOLO");
    EXPECT(trim("  sp  ") == "sp");
    EXPECT(trim("\t") == "");
    EXPECT(trim("x") == "x");

    if (fails == 0) {
        std::printf("[PASS] %d checks passed\n", total);
    } else {
        std::printf("[FAIL] %d/%d failed\n", fails, total);
    }
    return fails == 0 ? 0 : 1;
}
```

## 预期结果

`[PASS] 14 checks passed`，退出码 0；`sizeof(string_view)` 编译期证明成立。

## 设计意图

- **三工具各配一个"被替代的土办法"**：optional 顶魔数/-1、variant 顶
  union+tag、string_view 顶整串拷贝——工具的价值在对比里显形
- **describe 用 visit + 泛型 lambda + if constexpr**：把 5/6/7 三个知识点
  缝在一起（闭包分发、编译期剪枝、variant）——也允许重载 visitor，
  两条路都对，判题只看行为
- **零拷贝双证明**：sizeof（编译期，它是"地址+长度"的形状）+
  data() 指针落在原缓冲区间（运行期，切片确实看着原内存）——
  "不拷贝"不是口号，是指针级事实
- **double 的断言用 to_string 拼期望值**：不硬编码 "3.500000"，
  避免浮点格式化细节绑架验收
- **悬垂不做成可运行用例**（触发即 UB）——全部压进 SR3 走事故链，
  这是"能指出误用场景"验收的主战场

## 评分要点

- find_price 无魔数、nullopt 语义正确、value_or 两态全对
- describe 必经 visit；三种前缀格式正确；int 支不落 to_string 直接编不过
- is_int_field 用 holds_alternative；三态判定正确
- first_token/trim 返回 string_view；无空格/全空白/单字符边界全对
- 零拷贝双证明过；零输出零警告；接口名逐字符
- SR 四问要点齐全（见下），SR3 悬垂事故链是重头

## 简答题要点（判题对照）

- SR1：魔数坑——调用方没法区分"真值是 -1"和"没有"（语义撞车）；
  出参坑——签名不诚实（返回值位置在说谎）、调用啰嗦；
  空指针坑——忘了判就 UB，且"可能为空"不体现在类型上。
  optional<int> 装 0：has_value()==true、value()==0——**"有，值为零"和
  "没有"是两回事**，这正是魔数永远说不清的那件事。本质：把"可能没有"
  从注释/约定提升进类型系统，编译器帮你盯
- SR2：union 不记录当前装的是什么类型——读错类型是静默 UB（int 位模式
  按 double 读，无报警）；variant 记住了（内部有判别 tag），get 错类型
  抛 std::bad_variant_access（响亮地错），holds_alternative/get_if 可以
  先问再取。visit：对 variant 的"当前类型"做编译期表驱动分发——
  对每个候选类型实例化一份处理，运行期按 tag 挑一个调
- SR3：安全——被指向的内存活得比 view 久（参数是 const string& 的函数内、
  指向字符串字面量/静态存储期对象）。经典事故链：
  `member_sv = first_token(std::string("AAPL 170"))`——临时 string 在这句
  结束（分号）时析构，string 的堆缓冲被释放，member_sv 的
  地址指向已归还的堆——**下一行开始就是悬垂**，读它 = UB。
  判死刑的一句话：string_view 看向谁的内存，谁就必须活得比它久
- SR4：optional——装大对象时 sizeof 膨胀（值 + bool + padding）且每次构造
  都拷贝进去，频繁移动/无值场景多的接口不如指针/索引；variant——
  候选类型一多（>5）代码就开始腐烂，每个新类型都要补全所有 visit 分支
  （漏分支 = 编译错误，这其实是优点：强迫你面对）；string_view——
  出现在成员变量、函数返回值（指向局部/临时）、跨线程/异步传递时
  风险最高：view 自己不管生死，谁的内存谁负责——**"借用"语义
  离开"被借用者活着"的语境就是悬垂**

## 常见错误预测（判题重点检查）

1. describe 的 visitor 某支忘统一返回 string（int 支直接 return v →
   编不过，被迫 to_string——好事）
2. first_token/trim 返回 std::string（悄悄拷贝，丢掉本题灵魂；返回类型
   写错也会在 data() 指针证明处露馅——string 的 c_str 也能过指针检查，
   所以主要靠代码审查 + 返回类型核对）
3. trim 全空白没处理（find_first_not_of 返回 npos 时 substr 越界 UB 或
   返回整串）——验收专门放 `"\t"` 抓
4. find_price 找不到时返回 -1 包成 optional（魔数回魂——代码审查）
5. value_or 与 value() 混淆（空盒上 value() 抛 bad_optional_access——
   驱动不触发，审查提）
6. visit 里用holds_alternative+if 链（违反"必须 visit 分发"约束）
7. substr 参数语义记错（substr(pos, count)——count 是长度不是终点）
8. 接口拼写第五查：describe/find_price/first_token/trim/Field 逐字符对照

## 模式点拨（判题时向用户点明）

- **"可能没有"→optional、"多选一"→variant、"借用一段"→string_view**：
  C++17 三件套的选型口诀——先问"数据的形状"，再选工具
- **visit + 泛型 lambda + if constexpr**：类型分发的一体化写法，
  波次 2 的消息解析、波次 8 的算子分发都会反复用
- **借用语义**：string_view/optional 的引用/值取舍是 07 号"签名即合同"
  的延续——参数收 view（借用）、返回 view（要求调用方保证底料活着）
- **零拷贝证明手法**：sizeof 钉形状、data() 钉指向——以后写任何
  "零拷贝"组件（内存池视图、缓冲切片）都用这两招自证
