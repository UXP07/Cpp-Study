#pragma once
// 练习自检工具：把验收标准写成断言，机器判定 PASS/FAIL
// 用法：
//   CHECK(cond);              条件为真则通过
//   CHECK_EQ(actual, expect); 相等则通过，失败时打印两个值
//   main 末尾 return tester::summary();   全部通过返回 0，否则返回 1
#include <cstdio>
#include <sstream>

namespace tester {

inline int checks = 0;   // inline 变量（C++17）：允许在头文件中定义全局变量，所有包含处共享同一份
inline int failures = 0;

inline void report(bool ok, const char* expr, const char* file, int line) {
    ++checks;
    if (!ok) {
        ++failures;
        std::printf("  [FAIL] %s:%d  %s\n", file, line, expr);
    }
}

template <typename Actual, typename Expected>
void report_eq(const Actual& a, const Expected& e, const char* expr,
               const char* file, int line) {
    ++checks;
    if (!(a == e)) {
        std::ostringstream sa, se;
        sa << a;
        se << e;
        ++failures;
        std::printf("  [FAIL] %s:%d  %s\n      actual: %s\n      expected: %s\n",
                    file, line, expr, sa.str().c_str(), se.str().c_str());
    }
}

inline int summary() {
    if (failures == 0) {
        std::printf("[PASS] %d checks passed\n", checks);
        return 0;
    }
    std::printf("[FAIL] %d/%d checks failed\n", failures, checks);
    return 1;
}

}  // namespace tester

#define CHECK(cond) tester::report(static_cast<bool>(cond), #cond, __FILE__, __LINE__)
#define CHECK_EQ(actual, expected) \
    tester::report_eq((actual), (expected), #actual " == " #expected, __FILE__, __LINE__)
