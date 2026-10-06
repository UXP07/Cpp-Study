// 练习 08 —— 移动语义实验室（知识点 #2 移动语义与五法则）

## 背景

两个实验对象：

- **Blob** —— 行情快照内存块（真实系统里动辄几 MB）：复制昂贵，且"两份快照谁真谁假"在语义上就说不通。设计为**只移动**类型：所有权唯一，过户不复制。
- **Tracer** —— 无资源的小白鼠类：唯一功能是给每一次拷贝/移动记账，用来"看见"重载决议和 vector 扩容的真实路径。

**零输出要求**：两个类不打印任何东西，一切由判题端驱动用计数器验证，main 留空即可。
简答题里会让你**先预测**各场景的计数，判题时驱动揭晓答案——预测对了才算真懂。

## 需求

### Part A：class Blob —— 只移动的内存块

**Q1. 资源与构造**
堆上 `new unsigned char[n]`，内容按固定规则填充：`data[i] = (seed + i)` 截断为字节。
构造函数接收长度 n 与种子 seed。静态计数 `live()`：当前活跃块数（构造 +1、移动不变、释放 -1）。
禁止 string/vector/智能指针充当存储。

**Q2. 拷贝全禁**
拷贝构造与拷贝赋值 `=delete`。

**Q3. 移动全套**
移动构造：`noexcept`、接管指针与长度、源置空。
移动赋值：`noexcept`、先释放自身旧块、接管、源置空、**自移动安全**（状态不变）。
静态计数 `moves()`：每次移动构造/移动赋值（自移动不算）+1。

**Q4. moved-from 约定与查询**
被移动后的 Blob：`valid()` 为假、`size()` 为 0、`checksum()` 为 0；
可安全析构；可被移动赋值"复活"（重新持有新块）。
查询接口：`size()`、`valid()`、`checksum()`（全部字节求和；均应为 const）。

### Part B：class Tracer —— 计数小白鼠

**Q5.** 无任何资源。默认构造 + 手写拷贝构造（`copies()` +1）+ 手写移动构造（`moves()` +1）。
**移动构造故意不标 `noexcept`——这是实验变量，标了实验就废了。**
静态接口：`copies()`、`moves()`、`reset()`（两个计数清零）。析构不需要计数。

## 验收标准（判题端驱动逐条核对）

- Blob：移动后 `checksum()` 原样、源 invalid；moved-from 可"复活"；移动赋值正确释放旧块（live 精确）；自移动后数据完好
- Blob：`vector<Blob>` 从空 push 4 块——全程零拷贝（拷贝已删）、live==4、扩容后各块 checksum 完好
- Tracer：`Tracer b = a` 计 1 拷贝；`Tracer c = std::move(a)` 计 1 移动（std::move 之后 a 仍是合法对象）
- Tracer：`Tracer d = Tracer();` **0 拷贝 0 移动**（C++17）
- Tracer：从空 vector 连续 push 4 个右值——扩容搬迁**全部走拷贝**（尽管移动构造明明存在）
- 所有场景计数无漂移；编译期特征：Blob 不可拷贝且移动 noexcept，Tracer 移动**非** noexcept

## 简答题（答案写在 main.cpp 顶部注释块；先写预测再写原理）

SR1. `std::move` 到底做了什么、移动了什么？`Tracer c = std::move(a);` 之后 a 还能用吗，为什么？
SR2. `Tracer d = Tracer();` 为什么是 0 拷贝 0 移动？C++17 在这里保证了什么？
     同理 `f(Tracer{})` 按值传参也是 0，但 `f(std::move(d))` 是 1 移动——分界线在哪？
SR3. **预测并给出依据**：Tracer（移动构造非 noexcept）从空 vector push 4 个右值，
     最终 copies 和 moves 各是多少？Blob（noexcept 移动、无拷贝）同场景 moves 是多少？
     vector 扩容搬迁走拷贝还是移动，由什么规则决定（提示：move_if_noexcept 与强异常安全）？
SR4. Blob 被移动后处于什么状态？哪些操作允许（析构/移动赋值/……），
     哪些属于使用错误？"valid but unspecified"和本题的约定是什么关系？

## 约束

- Blob 裸 `new[]/delete[]`；Tracer 零资源零指针
- 两个类**零输出**；C++17（g++ 8.1）；注释中文
- 构建：`cmake --build --preset default --target 08_move_semantics`
- 这题建议盲写：`answers/08_move_semantics.md` 判完再看——07 的"读懂再写"证明读会 ≠ 写会
