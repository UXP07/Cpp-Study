// 练习 00 —— 工作区自检（非学习题，tester.h 的自检与目录模板）
// 正常构建运行应输出 [PASS]；想看 FAIL 的输出长什么样，
// 把下面任一断言改成错误值（如 2 * 21 == 43）再重新编译运行即可。
#include <string>

#include "tester.h"

int main() {
    CHECK(1 + 1 == 2);
    CHECK_EQ(2 * 21, 42);

    const std::string s = "hello";
    CHECK_EQ(s + " world", std::string("hello world"));

    return tester::summary();
}
