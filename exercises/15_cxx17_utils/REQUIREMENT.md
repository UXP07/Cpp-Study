// 练习 15 —— C++17 工具库：optional / variant / string_view（知识点 #7）

## 背景

行情解析模块的三个日常烦恼：查不到怎么办（魔数 -1？出参？空指针？）、
一个字段可能是三种类型怎么办（union + 手写 tag？）、只想切一段字符串
怎么办（整串拷贝太贵？）——C++17 各发了一件专用工具。

四个术语先翻成白话：

- **optional\<T\>**：一个"要么有值、要么空"的盒子——把"可能没有"
  变成类型系统里的一等公民
- **variant\<A,B,C\>**：记得自己现在装的是哪个类型的 union（安全版联合）
- **visit**：对 variant 问一句"你现在装的是啥"，自动分发到对应处理
- **string_view**：只带"地址 + 长度"的字符串观察窗——不拥有、不拷贝，
  看向谁的内存就读谁（**这既是它快的理由，也是它唯一的命门**）

**零输出要求**照旧；注释不要求；main 留空；判题端驱动验证
（含"零拷贝"的编译期与指针级证明）。

## 需求

**Q1. optional 查价**
`find_price(const std::vector<std::pair<std::string,int>>& table, const std::string& sym)`
→ 返回 `std::optional<int>`：找到返回价位，找不到返回"空"。
**不许用 -1 之类的魔数**——"没有"就让它真的是"没有"。

**Q2. variant 三态字段 + visit 分发**
类型别名 `Field = std::variant<int, double, std::string>`。
写 `describe(const Field&) -> std::string`：**必须经 std::visit** 完成分发，
三种类型分别返回 `"int:42"`、`"dbl:..."`（用 std::to_string）、`"str:tick"`
这样的前缀格式。visitor 怎么写随你（一组重载 / 单个泛型 lambda +
if constexpr 都行——后者正好接 14 号）。

**Q3. variant 的类型询问**
`is_int_field(const Field&) -> bool`：用 `std::holds_alternative` 判断
当前装的是不是 int。

**Q4. string_view 零拷贝切片**
- `first_token(std::string_view line)`：返回第一个空格**之前**的子串
  （没有空格则整串）——返回类型必须是 `std::string_view`（切片，不是拷贝）
- `trim(std::string_view s)`：去掉两端的空格和制表符，返回 `std::string_view`；
  全空白串返回空 view；单个字符、无空白边界都要正确

判题端会做两个"零拷贝证明"：`sizeof(std::string_view) == 2×指针大小`
（编译期），以及切片的 `data()` 指向你传入的原缓冲（运行期指针比对）。

## 验收标准（判题端驱动逐条核对）

- find_price：命中 `has_value()==true && value()==170`；未命中 `has_value()==false`；
  `value_or(-1)` 空时得 -1、命中时得真值
- describe：int → `"int:42"`；string → `"str:tick"`；double → `"dbl:" + std::to_string(值)`
- is_int_field：只对 int 字段为真
- first_token：`"AAPL 170.25 buy"` → `"AAPL"`；无空格整串返回
- trim：`"  sp  "` → `"sp"`；`"\t"` → `""`；`"x"` → `"x"`
- **零拷贝证明**：`static_assert(sizeof(std::string_view) == 2*sizeof(void*))`；
  切片 `data()` 落在原缓冲地址区间内
- 全程零输出零警告

## 简答题（答案写在 main.cpp 顶部注释块，白话作答）

SR1. optional 出现之前，"可能没有"的三种土办法（返回魔数 -1 / 出参 bool+值 /
     返回空指针）各有什么坑？`optional<int>` 装着 0 和空着有什么区别——
     这道题的"坑"的本质是什么？
SR2. variant 比 union 安全在哪（union 读错当前类型会怎样，variant get 错会怎样）？
     std::visit 是干什么的、分发发生在什么期（编译期还是运行期）？
SR3. string_view 的悬垂陷阱（本题最重要的 SR）：什么样的用法安全、什么样的
     完蛋？把"函数收到临时 string、切了 view、存进成员/全局"这个经典事故
     一步步走一遍——临时 string 死在哪一行之后？
SR4. 三个工具各举一个"不该用"的场景：optional 什么时候用它反而亏（想想
     它的内存布局和"值语义"）；variant 类型多了会发生什么；string_view
     出现在什么位置（成员？出参？返回值？）时风险最高。

## 约束

- 头文件允许：`<optional> <variant> <string_view> <string> <vector> <utility>`
  及 `<type_traits>`（if constexpr visitor 用）
- describe 必须经 `std::visit`（不许 holds_alternative + if 链糊弄）；
  find_price 不许魔数
- 零输出；C++17；注释不要求；接口名逐字符照需求抄（第五查）；
  改完自己编译
- 构建：`cmake --build --preset default --target 15_cxx17_utils`
