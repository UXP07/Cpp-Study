// 练习 19 —— atomic 与内存序：release/acquire 配对（知识点 #12）

## 背景

锁的粒度再小也有价：拿放锁各一次就是几十纳秒。热路径上更轻的工具是
原子操作——单条读改写不需要锁。但原子带来第二个维度：**内存序**——
"这条原子操作周围的其他读写，能被别的线程看见吗、按什么顺序看见"。
这是 C++ 最深的一口井，本题只打你需要的两口：**release/acquire 配对**
（发布数据的标准姿势）和 **relaxed**（纯计数）。

术语白话：

- **原子操作**：不可分割的单步读/写/读改写——做就是做了，没有中间态
- **release（写端）**：像"写锁的闸门"——**这条 store 之前的所有写**，
  对配对的 acquire 读者全部可见
- **acquire（读端）**：像"读锁的闸门"——配对读到之后，写端 release 前
  的那些写你全都能看见
- **relaxed**：只保证这一条操作自己是原子的，**不管任何顺序**——
  独立计数够用，发布数据不够
- **seq_cst**：默认档——全体 seq_cst 操作共享一条"全世界公认的时间线"，
  最安全也最贵

**零输出**照旧；注释不要求；main 留空；判题端驱动验证。

## 需求

**Q1. class Channel（release/acquire 发布数据——主菜）**
- `publish(int v)`：先写普通成员 `data_ = v`，再
  `ready_.store(true, std::memory_order_release)`——载荷先落、旗子后竖
- `consume() -> int`：自旋 `while (!ready_.load(std::memory_order_acquire))`
  等旗子，竖起后返回 `data_`——acquire 配对保证你看到的 data_ 一定是
  publish 写进去的那个值
- 判题端：发布线程/消费线程循环千次，每次值都正确配对

**Q2. class RelaxCounter（relaxed 的合法用武之地）**
`bump()` 用 `fetch_add(1, std::memory_order_relaxed)`、`get()` 用
relaxed load——纯计数、不携带依赖数据，relaxed 足够。
判题端：8 线程 × 10000 次 bump == 80000。

**Q3.（SR 主考）六种内存序语义表**：relaxed / acquire / release /
acq_rel / seq_cst（consume 实践中当 acquire 用，声明即可）——白话各一句
+ 配对规则 + 默认档的取舍。验收"能解释各内存序语义"全在这一题。

## 验收标准（判题端驱动核对）

- Channel：1000 轮发布-消费（每轮新实例、新线程），consume 返回值全部
  与 publish 一致——release/acquire 用对是编译期+运行期双验证
  （用 relaxed 也会碰巧过很多轮，代码审查抓内存序写法）
- RelaxCounter：8 线程 × 10000 == 80000，多轮稳定
- Channel 的 store/load 必须显式 release/acquire（审查）；RelaxCounter
  必须显式 relaxed（审查）
- 零输出零警告

## 简答题（答案写在 main.cpp 顶部注释块，白话作答）

SR1. 六种内存序各自一句话（白话表：relaxed=只原子不管顺序；
     acquire=读闸门；release=写闸门；acq_rel=读写双向闸门；
     seq_cst=全局统一时间线+默认；consume 实际当 acquire 用）。
     默认是哪档、为什么编译器不帮你挑更快的？
SR2. release/acquire 配对：写端 publish 里两条语句的顺序能换吗——换了
     会出什么事故（消费者看到旗子却读到旧 data）？两边都用 relaxed
     会怎样？为什么说这对配对是"单生产者-单消费者的最小同步"？
SR3. 什么时候 relaxed 就够（独立计数、统计打点）？为什么说"默认
     seq_cst、profile 说话之前不降级"是工程纪律？内存序 bug 的
     复现率有什么特点（低概率、难复现——所以靠什么防：靠模式而不是
     靠测试）？

## 约束

- 本题禁 mutex（体会原子版的"无锁"）；允许 <atomic>
- 内存序必须显式写出（release/acquire/relaxed），不许偷用默认档
- 零输出；C++17；接口名逐字符照抄（第七查）；改完自己编译
- 构建：`cmake --build --preset default --target 19_atomic_order`
