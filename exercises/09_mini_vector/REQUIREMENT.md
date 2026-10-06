// 练习 09 —— 迷你 Vec<T>：自己造一个 vector（知识点 #2 收尾；综合 #1；预告 #4 模板）

## 背景

三道题你一直隔着计数器观察 vector：Blob 7 次移动、Tracer 3 次拷贝搬迁、落位永远
移动、reserve 让搬迁消失……现在把观察者变成实现者——**手写一个迷你动态数组
`Vec<T>`**。

验收只有一条硬指标：**同样的操作序列，你的 Vec 与 std::vector 的拷贝/移动计数
一字不差**（判题端对拍：同一场景跑两边，计数必须相等）。

本题按你的要求**不设简答题**——概念全部用代码说话。

## 需求

**Q1. 内存与对象分离**
模板类 `Vec<T>` 管理一段**生内存**，size 与 capacity 分离；**T 没有默认构造也必须
能用**（测试对象就是 08 的 Blob——它没有默认构造）。槽位只在 push 时才构造对象，
析构时逐个销毁、整段内存一次释放。

**Q2. push_back 两个重载**
左值实参 → 落位**拷贝**构造；右值实参 → 落位**移动**构造。
（调用点按实参值类别选重载，槽位构造照着重载来——你上一轮刚讲对的机制，写出来。）

**Q3. 容量与扩容**
`reserve(n)` 预留容量；容量满自动增长，策略与 std::vector 相同：**空 → 1 → 之后
每次翻倍**。扩容搬迁规则也必须与 std::vector 相同，否则 Tracer 场景的计数对不上
（提示：`std::move_if_noexcept`）。

**Q4. 五法则**
拷贝构造/拷贝赋值 `=delete`（本容器只移动）；移动构造/移动赋值 `noexcept` 且
**O(1)**——换指针/整数，绝不触碰元素——源置空。

**Q5. 接口**
`size()`、`capacity()`、`operator[]`（非 const 与 const 两个版本）。类**零输出**。

## 验收标准（判题端驱动逐条核对）

- **对拍基线**：Blob/Tracer 的多种场景（右值 push、左值 push、reserve 后 push）
  下，你的 Vec 与 std::vector 的 copies/moves/live 计数完全一致
- 具体锚点：`Vec<Blob>` 右值 push ×4 → moves=**7**、live=4、各块数据完好；
  `Vec<Tracer>` 右值 push ×4 → moves=**4**、copies=**3**；
  reserve(4) 后再 push ×4 → moves=**4**（搬迁消失）
- 左值 push 走拷贝、源对象不受影响
- Vec 的移动构造/移动赋值：过户 O(1)（计数零变化）、源清空、被接管方旧资源正确释放
- 程序结束 live 归零（无泄漏）；编译期：Vec 不可拷贝、两个移动操作 noexcept
- `Vec<Blob>` 能实例化（证明你没有偷用需要默认构造的 `new T[cap]`）

## 约束

- **禁止**用 std::vector / unique_ptr / any 容器或智能指针托管内存——本题就是手写它
- 生内存工具提示（语言设施，直接用）：`::operator new(字节数)` / `::operator delete(指针)`
  （自带对齐保证，比 `new unsigned char[]` 当缓冲安全）；槽位构造 `::new (p) T(实参)`；
  显式析构 `p->~T()`
- 测试对象复用 08 的 Blob/Tracer（判题驱动自动引入，你不用管）
- C++17（g++ 8.1）；类零输出；注释中文
- 构建：`cmake --build --preset default --target 09_mini_vector`
