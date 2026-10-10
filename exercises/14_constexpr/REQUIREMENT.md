// 练习 14 —— constexpr：把计算搬到编译期（知识点 #6 constexpr 与编译期计算）

## 背景

低延迟系统的一条铁律：**能在编译期算完的，绝不留到运行期**。价格换算表、
tick 取整规则、数组尺寸——这些在程序启动前就该躺在二进制里。

四个术语先翻成白话：

- **constexpr（函数）**："我能在编译期算"的**资质证书**——拿到证书两头都能用：
  编译期当常量算，运行期当普通函数调（**双重生命**）
- **constexpr（变量/对象）**：这个值编译期就算好了，直接写进二进制
- **static_assert**：编译期断言——条件不成立直接编译失败（判题拿它当探针）
- **if constexpr**：编译期的 if——条件是**类型**级别的判断，不成立的分支
  **当不存在**（连编译都不参与）；普通 if 的两个分支都必须编过

场景：行情系统的价格换算与参数分发。

**零输出要求**照旧；注释不要求；main 留空；判题端用"static_assert 编译期
+ EXPECT 运行期"双通道验证。

## 需求

**Q1. constexpr 换算函数（三个，全部 constexpr）**
- `round_to_tick(int price, int tick)`：价格四舍五入到最近的 tick
  （正数域；例：round_to_tick(103,10)==100、(105,10)==110、(104,10)==100）
- `ticks_between(int p1, int p2, int tick)`：两价格间隔几个 tick
  （与顺序无关，例：(100,130,10)==3、(130,100,10)==3）
- `gcd(int a, int b)`：最大公约数，**循环版辗转相除**（constexpr 函数里
  可以有循环和分支——C++14 起；不建议递归版：编译器对递归深度有限制）

**Q2. constexpr 字面类型 `Price`**
构造接收"分"（int cents）；`constexpr` 成员 `dollars()`（整元部分）与
`rem()`（剩余分数，0-99）。要求 `constexpr Price p{12345}` 能在
static_assert 里直接用（p.dollars()==123、p.rem()==45）。

**Q3. if constexpr 按类型剪枝 `scale<T>(const T&)`**
整数类型 → 返回 `v * 2`；其他（有 `.size()` 的容器）→ 返回 `v.size()`。
关键：**写普通 if 编不过**（错分支也要实例化，`vector * 2` 不存在）——
只有 if constexpr 能让错分支整个消失。

**Q4. constexpr lambda `square`**
名字空间级定义 `constexpr` 的 lambda：接收 int 返回平方。
能在 static_assert 里用（square(9)==81）。

**Q5. 编译期数组维度（"真 constexpr"的铁证）**
无需额外代码——判题驱动会拿你的 `gcd` 当数组维度
（`std::array<int, gcd(12, 18)>`）。数组维度只认编译期常量，
这是比"static_assert 里能算"更硬的证明。

## 验收标准（判题端驱动双通道核对）

- **编译期通道（static_assert）**：Q1 三函数的全部锚点数字、
  `Price p{12345}` 的 123/45、`scale(21)==42`、`square(9)==81`、
  `std::array<int, gcd(12,18)>` 维度为 6——任何一个是"伪 constexpr"
  （只能在运行期算）都直接编译失败
- **运行期通道（EXPECT）**：同一批函数用运行期变量再调一遍，
  结果一致（constexpr 的双重生命：编译期算过的，运行期照常能调）
- `scale` 整数路径与容器路径都对（int → 翻倍；vector<int> → 尺寸）
- 全程零输出零警告

## 简答题（答案写在 main.cpp 顶部注释块，白话作答）

SR1. constexpr 是给函数的什么承诺？拿到证书的函数"双重生命"指什么？
     constexpr 函数里能不能有循环、分支？（哪一年起就可以）
SR2. static_assert 和运行期断言（比如你写过的 EXPECT）有什么本质区别？
     "能当数组维度/模板实参"为什么是比"static_assert 能算"更硬的
     constexpr 证明？（提示：这些位置只认什么）
SR3. if constexpr 和普通 if 的本质区别？普通 if 为什么编不过 Q3
     （两个分支都要实例化——错的那支编不过就全盘编不过）？
     这个特性在模板里的经典用途是什么？
SR4. constexpr lambda 是什么（C++17）？它和你 #5 写的闭包是什么关系——
     捕获列表里的东西想进 constexpr 世界有什么天然限制？

## 约束

- 禁用 `std::gcd`、`std::abs` 之外的现成算法（gcd 手写循环版）
- 所有 constexpr 函数必须同时出现在编译期与运行期两个通道
- 零输出；C++17；注释不要求；接口名逐字符照需求抄（老规矩）；
  改完自己编译（你的工作流）
- 构建：`cmake --build --preset default --target 14_constexpr`
