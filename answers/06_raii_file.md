# 练习 06 标准答案 —— RAII 文件句柄（LogFile 与五法则）

## 参考实现

```cpp
// 练习 06 —— RAII 文件句柄（LogFile 与五法则）
// 简答题：
// SR1: 构造函数抛异常时，对象的生命周期从未开始——析构函数只对"完整构造完成"的
//      对象调用，所以不会被调用；但已构造完成的成员会按构造的逆序自动析构。
//      因此"资源获取于构造"天然防泄漏：要么对象存在且持有句柄，要么对象根本不存在，
//      没有"半构造却抓着资源"的中间态。若构造要先后获取两份资源，第二份失败时，
//      第一份必须手动释放（try/catch 或借助局部 RAII 对象）——本题单资源无此负担。
// SR2: std::move 什么都不移动。它只是 static_cast<T&&>，把左值表达式变成右值引用，
//      让重载决议选中移动版本；真正的"移动"发生在被选中函数的函数体里。
//      不把源句柄置空，则源对象析构时 fclose 一次、新对象析构时再 fclose 同一句柄，
//      double close 是未定义行为，CRT 通常直接报错终止。
// SR3: vector 扩容要把旧元素搬到新内存。若移动构造可能抛异常，搬到一半抛出时无法
//      回滚（旧元素已被掏空），vector 为维持强异常安全只能退化为逐个拷贝；
//      标了 noexcept 才敢用移动。拥有资源的类几乎总会进容器，所以这是硬要求。
// SR4: 栈展开是 throw 到进入匹配 catch 之间的自动清理：调用栈逐层退出，每层已完整
//      构造的局部对象按构造逆序析构。证据：输出中
//      "[LogFile] dtor: unwind.log (closed)" 出现在 "[main] caught: simulated failure"
//      之前——异常还在半路，文件已关闭，句柄没有泄漏。
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

// RAII 文件句柄类：句柄的生命周期 == 对象的生命周期
class LogFile {
public:
    // Q1：成员一律初始化列表（句柄直接在列表中打开）；失败抛异常 = 对象从未存在
    LogFile(const std::string& name, const char* mode)
        : name_(name), handle_(std::fopen(name.c_str(), mode)), bytes_(0) {
        if (handle_ == nullptr) {
            throw std::runtime_error("cannot open: " + name);
        }
        trace("ctor");
    }

    // Q2：析构——资源的常规唯一出口；判空防 moved-from / 空对象
    ~LogFile() {
        if (handle_ != nullptr) {
            std::fclose(handle_);
            std::printf("[LogFile] dtor: %s (closed)\n", name_.c_str());
        } else {
            std::printf("[LogFile] dtor: (empty)\n");
        }
    }

    // Q4：拷贝构造 = 深拷贝资源：独立重开句柄，追加模式，绝不截断共享文件
    LogFile(const LogFile& other)
        : name_(other.name_), handle_(std::fopen(other.name_.c_str(), "a")), bytes_(0) {
        if (handle_ == nullptr) {
            throw std::runtime_error("cannot reopen: " + other.name_);
        }
        trace("copy-ctor");
    }

    // Q4：拷贝赋值——自赋值安全；先获取新句柄、成功后才释放旧句柄（强异常保证）
    LogFile& operator=(const LogFile& other) {
        if (this == &other) {
            std::printf("[LogFile] copy-assign: %s -> %s (self)\n",
                        name_.c_str(), name_.c_str());
            return *this;
        }
        FILE* fresh = std::fopen(other.name_.c_str(), "a");
        if (fresh == nullptr) {
            throw std::runtime_error("cannot reopen: " + other.name_);
        }
        std::string old = name_;
        if (handle_ != nullptr) {
            std::fclose(handle_);
        }
        handle_ = fresh;
        name_   = other.name_;
        bytes_  = 0;
        std::printf("[LogFile] copy-assign: %s -> %s\n", old.c_str(), name_.c_str());
        return *this;
    }

    // Q5：移动构造——接管句柄与计数，源置空；noexcept 理由见 SR3
    LogFile(LogFile&& other) noexcept
        : name_(std::move(other.name_)),
          handle_(other.handle_),
          bytes_(other.bytes_) {
        other.handle_ = nullptr;
        other.bytes_  = 0;
        trace("move-ctor");
    }

    // Q5：移动赋值——释放自身旧句柄（判空），接管对方，对方置空
    LogFile& operator=(LogFile&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        std::string old = name_;
        if (handle_ != nullptr) {
            std::fclose(handle_);
        }
        handle_       = other.handle_;
        bytes_        = other.bytes_;
        name_         = std::move(other.name_);
        other.handle_ = nullptr;
        other.bytes_  = 0;
        std::printf("[LogFile] move-assign: %s -> %s\n", old.c_str(), name_.c_str());
        return *this;
    }

    // Q3：写入即落盘；计数以 fwrite 实际写入的字节数为准
    void write(const std::string& line) {
        if (handle_ == nullptr) {
            throw std::logic_error("write on empty LogFile");
        }
        bytes_ += std::fwrite(line.data(), 1, line.size(), handle_);
        std::fflush(handle_);
    }

    const std::string& name() const { return name_; }
    bool has_handle() const { return handle_ != nullptr; }
    std::size_t bytes() const { return bytes_; }

private:
    void trace(const char* op) const {
        std::printf("[LogFile] %s: %s\n", op, name_.c_str());
    }

    std::string name_;    // 文件名（描述性成员）
    FILE*       handle_;  // 裸资源：需要五法则的唯一原因
    std::size_t bytes_;   // 经当前句柄累计写入的字节数
};

// 状态打印：标签用对象变量名，便于核对
static void stat(const char* tag, const LogFile& f) {
    std::printf("[stat] %s: %s, bytes=%lu, handle=%s\n",
                tag, f.name().c_str(),
                static_cast<unsigned long>(f.bytes()),
                f.has_handle() ? "yes" : "no");
}

// 栈展开演示：局部对象完整构造后抛异常，析构必须发生在 catch 之前
static void unwind_demo() {
    LogFile u("unwind.log", "w");
    u.write("before-throw\n");
    throw std::runtime_error("simulated failure");
}

int main() {
    // 1. 基本生命周期：作用域结束自动关闭
    {
        LogFile a("log.txt", "w");
        a.write("alpha\n");
        a.write("bravo\n");
        stat("a", a);
    }

    // 2. 追加打开
    LogFile b("log.txt", "a");
    b.write("charlie\n");
    stat("b", b);

    // 3. 拷贝构造：独立句柄、计数从 0 开始
    LogFile c = b;
    c.write("delta\n");
    stat("c", c);
    stat("b", b);

    // 4. 拷贝赋值：释放 other.txt 句柄、重开 log.txt；随后自赋值
    LogFile d("other.txt", "w");
    d.write("echo\n");
    d = b;
    stat("d", d);
    d.write("foxtrot\n");
    LogFile& r = d;
    d = r;                      // 自赋值：状态与计数不得变化
    stat("d", d);

    // 5. 移动构造：b 交出所有权变空
    LogFile e = std::move(b);
    stat("e", e);
    stat("b", b);
    e.write("golf\n");
    stat("e", e);

    // 6. 移动赋值：f 释放 tmp.txt、接管 e 的句柄与计数
    LogFile f("tmp.txt", "w");
    f = std::move(e);
    stat("f", f);
    stat("e", e);
    f.write("hotel\n");
    stat("f", f);

    // 7. 构造失败：对象从未存在——注意没有它的 dtor trace
    try {
        LogFile bad("missing.txt", "r");
    } catch (const std::exception& ex) {
        std::printf("[main] caught: %s\n", ex.what());
    }

    // 8. 栈展开：dtor trace 必须先于 caught
    try {
        unwind_demo();
    } catch (const std::exception& ex) {
        std::printf("[main] caught: %s\n", ex.what());
    }

    // 9. 回读 log.txt（裸句柄仅为演示验证，工程代码应同样用 RAII 包裹）
    std::FILE* in = std::fopen("log.txt", "r");
    if (in != nullptr) {
        char buf[256];
        while (std::fgets(buf, sizeof buf, in) != nullptr) {
            std::printf("[file] %s", buf);
        }
        std::fclose(in);
    }
    return 0;
}
```

## 预期输出（判题核对基准）

```text
[LogFile] ctor: log.txt
[stat] a: log.txt, bytes=12, handle=yes
[LogFile] dtor: log.txt (closed)
[LogFile] ctor: log.txt
[stat] b: log.txt, bytes=8, handle=yes
[LogFile] copy-ctor: log.txt
[stat] c: log.txt, bytes=6, handle=yes
[stat] b: log.txt, bytes=8, handle=yes
[LogFile] ctor: other.txt
[LogFile] copy-assign: other.txt -> log.txt
[stat] d: log.txt, bytes=0, handle=yes
[LogFile] copy-assign: log.txt -> log.txt (self)
[stat] d: log.txt, bytes=8, handle=yes
[LogFile] move-ctor: log.txt
[stat] e: log.txt, bytes=8, handle=yes
[stat] b: , bytes=0, handle=no
[stat] e: log.txt, bytes=13, handle=yes
[LogFile] ctor: tmp.txt
[LogFile] move-assign: tmp.txt -> log.txt
[stat] f: log.txt, bytes=13, handle=yes
[stat] e: , bytes=0, handle=no
[stat] f: log.txt, bytes=19, handle=yes
[main] caught: cannot open: missing.txt
[LogFile] ctor: unwind.log
[LogFile] dtor: unwind.log (closed)
[main] caught: simulated failure
[file] alpha
[file] bravo
[file] charlie
[file] delta
[file] foxtrot
[file] golf
[file] hotel
[LogFile] dtor: log.txt (closed)
[LogFile] dtor: (empty)
[LogFile] dtor: log.txt (closed)
[LogFile] dtor: log.txt (closed)
[LogFile] dtor: (empty)
```

判题弹性：`[main] caught:` 后的消息文本只要含文件名/触发词即可（各人异常消息措辞不同）；
被移动对象 stat 行的 name 字段任意（moved-from string 是"有效但未指定"，
libstdc++ 下为空串），关键字段是 bytes=0、handle=no。

磁盘核对：`log.txt` 7 行；`other.txt` 仅 echo；`tmp.txt` 存在且 0 字节；
`unwind.log` 含 before-throw。以行数为准——Windows 文本模式下 `\n` 落盘为
`\r\n`，log.txt 物理 52 字节（45+7）；对象计数器统计 fwrite 返回的逻辑字节数，
不受转换影响。

## 设计意图

- **句柄在初始化列表里直接 `fopen`**：初始化列表练习的进阶用法——成员初始化可以
  含函数调用；失败检查留构造体，失败即抛 = 对象从未存在，无中间态可泄漏
- **拷贝固定追加模式 `"a"`**：拷贝的语义是"另一个独立写者"，`"w"` 重开会截断
  共享文件（经典事故）；这是"深拷贝资源 ≠ 复制参数"的核心教学点
- **先 `fopen` 后 `fclose`（拷贝赋值）**：新资源到手才放旧的，失败时原句柄原封不动
  （强异常保证）；copy-and-swap 亦可，此处直写更易读
- **移动连字节数一起接管**：计数语义定义为"经当前句柄写入"，句柄走了账也跟着走
- **`bytes_ += fwrite(...)`**：以实际写入为准，不信任入参长度
- **`fclose` 前判空**：`fclose(nullptr)` 是未定义行为；moved-from 对象必须能安全析构
- **成员声明顺序 = 初始化列表顺序**（name_, handle_, bytes_），避开 -Wreorder 隐患

## 评分要点

- 五件套齐全且语义正确（Q1–Q5）；成员初始化列表（Q1）
- trace 格式逐行一致（Q6）；数值链 12/8/6/0→8→8/8→13/13→19 全对（Q7）
- 拷贝赋值：自赋值安全 + 先获取后释放（代码审查）
- 移动赋值：释放旧句柄 + 源置空；两移动操作 noexcept
- 异常路径：构造失败无 dtor trace 且程序继续；栈展开 dtor 先于 caught
- 构造失败无资源泄漏；无 double-close（程序正常退出本身是证据）
- 质量要求：简洁（无多余抽象）、输出英文、注释中文

## 常见错误预测（判题重点检查）

1. 拷贝用 `"w"` 或原模式重开 → 截断共享文件：log.txt 最终行数/字节数穿帮
2. 移动后不置空源 → double fclose，CRT 报错或崩溃
3. 移动赋值忘记释放自身旧句柄 → tmp.txt 句柄泄漏（输出看不出，靠代码审查）
4. 拷贝赋值先 `fclose` 后 `fopen` → 失败即丢句柄（代码审查 + SR 追问）
5. 拷贝赋值无自赋值保护 → d 的计数被清零（stat d bytes=8 穿帮）
6. 析构不判空直接 `fclose` → moved-from 对象析构崩溃
7. 移动操作忘标 `noexcept`（SR3 扣分点）
8. 成员在构造体内用 `=` 赋值而非初始化列表（练习 01 回炉）
9. `write` 不 `fflush` → 回读缺行（Windows 下缓冲未落盘）
10. 用 `fstream` / 智能指针 / 公开 `close()` → 违反约束
11. `return std::move(local)` 反模式（如有，点拨）

## 模式点拨（判题时向用户点明）

- **Rule of Five**：拥有裸资源的类，五个特殊成员要么全写、要么全禁（=delete）；
  只写析构不写移动，移动会被抑制、静默退化为拷贝（性能暗坑）
- **RAII 范式**：资源获取于构造、释放于析构；确定性析构（作用域结束/栈展开/程序退出）
  取代手动释放与 GC；构造失败 = 对象不存在
- **acquire-then-commit / copy-and-swap**：赋值运算符强异常安全的两种标准写法
- **moved-from 约定**：标准语义是"有效但未指定"；自设计资源类时应显式定义清楚
  （本题定义为"空对象"：可析构、可查询、可再赋值）
- **真实日志类通常 delete 拷贝、只留移动**（参照 unique_ptr/lock_guard 的思路）——
  本题要求可拷贝是为完整练习五法则；何时禁拷贝是下一个知识点的取舍题
