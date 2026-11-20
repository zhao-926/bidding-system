// test_framework.h — 自寫的極簡測試框架（不使用任何第三方函式庫）
//
// 用法：
//   void test_something() {
//       ASSERT_TRUE(1 + 1 == 2);
//       ASSERT_EQ(3, 1 + 2);        // (期望值, 實際值)
//   }
//   void run_xxx_tests() { RUN_TEST(test_something); }
//
// 斷言失敗不會中止程式，會印出檔案與行號，最後統計通過／失敗數。

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <iostream>
#include <sstream>
#include <string>

// C++17 的 inline 變數：放在標頭檔也只會有一份
inline int g_assertPassed = 0;   // 通過的斷言數
inline int g_assertFailed = 0;   // 失敗的斷言數
inline int g_testCount = 0;      // 執行了幾個測試案例
inline int g_testFailed = 0;     // 有幾個測試案例含失敗斷言

inline void assertTrue(bool condition, const char* expression, const char* file, int line) {
    if (condition) {
        g_assertPassed++;
    } else {
        g_assertFailed++;
        std::cout << "    FAIL " << file << ":" << line
                  << "  ASSERT_TRUE(" << expression << ")" << std::endl;
    }
}

template <typename A, typename B>
void assertEq(const A& expected, const B& actual,
              const char* expectedText, const char* actualText,
              const char* file, int line) {
    if (expected == actual) {
        g_assertPassed++;
    } else {
        g_assertFailed++;
        std::ostringstream message;
        message << "    FAIL " << file << ":" << line
                << "  ASSERT_EQ(" << expectedText << ", " << actualText << ")"
                << "  expected=" << expected << "  actual=" << actual;
        std::cout << message.str() << std::endl;
    }
}

#define ASSERT_TRUE(cond)   assertTrue((cond), #cond, __FILE__, __LINE__)
#define ASSERT_FALSE(cond)  assertTrue(!(cond), "!(" #cond ")", __FILE__, __LINE__)
#define ASSERT_EQ(expected, actual) \
    assertEq((expected), (actual), #expected, #actual, __FILE__, __LINE__)

// 執行一個測試函式並印出結果
inline void runTest(void (*testFunction)(), const char* name) {
    int failedBefore = g_assertFailed;
    g_testCount++;
    testFunction();
    if (g_assertFailed == failedBefore) {
        std::cout << "  [ OK ] " << name << std::endl;
    } else {
        g_testFailed++;
        std::cout << "  [FAIL] " << name << std::endl;
    }
}

#define RUN_TEST(fn) runTest(fn, #fn)

// 印出總結；回傳程式結束代碼（0 = 全過，1 = 有失敗）
inline int printTestSummary() {
    std::cout << std::endl
              << "Tests: " << (g_testCount - g_testFailed) << "/" << g_testCount << " passed, "
              << "Assertions: " << g_assertPassed << " passed, "
              << g_assertFailed << " failed" << std::endl;
    return (g_assertFailed == 0) ? 0 : 1;
}

#endif  // TEST_FRAMEWORK_H
