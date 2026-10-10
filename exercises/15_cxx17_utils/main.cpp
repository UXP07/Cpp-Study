#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

/*
SR1. optional 出现之前，"可能没有"的三种土办法（返回魔数 -1 / 出参 bool+值 /
     返回空指针）各有什么坑？`optional<int>` 装着 0 和空着有什么区别——
     这道题的"坑"的本质是什么？
    1、魔数的坑：调用方分不清"真值是 -1"和"没有"（语义撞车）；
       出参的坑：签名不诚实（返回值位置在说谎）、调用啰嗦；
       空指针的坑：忘判就 UB，且"可能为空"不体现在类型上。
    2、装着 0：has_value()==true、value()==0——"有，值为零"和"没有"是
       两回事，这正是魔数永远说不清的那件事。坑的本质：把"可能没有"
       从注释/约定提升进类型系统，让编译器替你盯。
SR2. variant 比 union 安全在哪（union 读错当前类型会怎样，variant get 错会怎样）？
     std::visit 是干什么的、分发发生在什么期（编译期还是运行期）？
    1、variant 显式记录自己当前存的是什么类型（内部有判别 tag）：
       union 读错当前类型是静默 UB（int 的位模式按 double 读，无报警）；
       variant get 错类型抛 std::bad_variant_access——响亮地错。
    2、visit：对 variant"你现在装的是啥"做分发——编译期为每个候选类型
       实例化一份处理，运行期按 tag 挑一个调：两期各干一半。
SR3. string_view 的悬垂陷阱（本题最重要的 SR）：什么样的用法安全、什么样的
     完蛋？把"函数收到临时 string、切了 view、存进成员/全局"这个经典事故
     一步步走一遍——临时 string 死在哪一行之后？
    1、string 存在的时间比 string_view 长就不会出现悬垂（核心规则）。
    2、临时 string 不是死在函数退出，是死在那条语句的分号处：
       member = first_token(std::string("AAPL 170")); 的分号一过，
       临时 string 的堆缓冲就被释放——member 从下一行开始就是悬垂，
       读它 = UB。"死在分号"是这个事故链最值钱的一句话。
SR4. 三个工具各举一个"不该用"的场景：optional 什么时候用它反而亏（想想
     它的内存布局和"值语义"）；variant 类型多了会发生什么；string_view
     出现在什么位置（成员？出参？返回值？）时风险最高。
    1、返回值永不为空就别包 optional（白多一个 bool + padding，纯增开销）；
       装大对象也亏（sizeof 膨胀 + 每次构造拷进去，值语义的代价）。
    2、variant 类型一多，每个 visit 都要为每个新类型补分支——漏分支
       就是编译错误：类型集合爆炸，但"逼你面对每个类型"其实是优点。
    3、返回值是头号雷位（返回指向局部/临时的 view）；成员变量紧随其后
       （view 存进对象，底料的死活脱离你的视线）。
*/

std::optional<int> find_price(const std::vector<std::pair<std::string, int>>& table, 
                                const std::string& sym)
{
    for(const auto& row : table)
    {
        if(row.first == sym) return row.second;
    }

    return std::nullopt;
}

using Field = std::variant<int, double, std::string>;

std::string describe(const Field& f) {
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

bool is_int_field(const Field& f){
    return std::holds_alternative<int>(f);
}

std::string_view first_token(std::string_view line) {
    std::size_t sp = line.find(' ');
    return sp == std::string_view::npos ? line : line.substr(0, sp);
}

std::string_view trim(std::string_view s) {
    const char* ws = " \t";
    std::size_t b = s.find_first_not_of(ws);
    if (b == std::string_view::npos) {
        return s.substr(s.size());       // 全空白 → 空 view
    }
    std::size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}


int main()
{
    return 0;
}