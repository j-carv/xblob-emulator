#pragma once

#include <cstdlib>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace xblob::testing {

struct TestCase {
    std::string name;
    std::function<void()> func;
};

inline std::vector<TestCase>& GetRegistry() {
    static std::vector<TestCase> registry;
    return registry;
}

inline bool RegisterTestCase(std::string name, std::function<void()> func) {
    GetRegistry().push_back({std::move(name), std::move(func)});
    return true;
}

inline int RunAllTests() {
    int passed = 0;
    int failed = 0;
    std::cout << "[==========] Executando " << GetRegistry().size() << " testes.\n";

    for (const auto& test : GetRegistry()) {
        std::cout << "[ RUN      ] " << test.name << "\n";
        try {
            test.func();
            std::cout << "[       OK ] " << test.name << "\n";
            passed++;
        } catch (const std::exception& ex) {
            std::cerr << "[  FAILED  ] " << test.name << "\n    Exceção: " << ex.what() << "\n";
            failed++;
        } catch (...) {
            std::cerr << "[  FAILED  ] " << test.name << " com falha não tratada.\n";
            failed++;
        }
    }

    std::cout << "[==========] " << passed << " passaram, " << failed << " falharam.\n";
    return failed == 0 ? 0 : 1;
}

class TestFailureException : public std::runtime_error {
public:
    explicit TestFailureException(const std::string& msg) : std::runtime_error(msg) {}
};

#define TEST_CASE(name)                                                                            \
    static void name();                                                                            \
    static const bool name##_registered = ::xblob::testing::RegisterTestCase(#name, name);         \
    static void name()

#define EXPECT_TRUE(cond)                                                                          \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::ostringstream oss;                                                                \
            oss << "EXPECT_TRUE falhou: (" #cond ") em " << __FILE__ << ":" << __LINE__;           \
            throw ::xblob::testing::TestFailureException(oss.str());                               \
        }                                                                                          \
    } while (0)

#define EXPECT_FALSE(cond)                                                                         \
    do {                                                                                           \
        if (cond) {                                                                                \
            std::ostringstream oss;                                                                \
            oss << "EXPECT_FALSE falhou: (" #cond ") em " << __FILE__ << ":" << __LINE__;          \
            throw ::xblob::testing::TestFailureException(oss.str());                               \
        }                                                                                          \
    } while (0)

#define EXPECT_EQ(a, b)                                                                            \
    do {                                                                                           \
        if (!((a) == (b))) {                                                                       \
            std::ostringstream oss;                                                                \
            oss << "EXPECT_EQ falhou: " #a " == " #b " (" << (a) << " != " << (b) << ") em "       \
                << __FILE__ << ":" << __LINE__;                                                    \
            throw ::xblob::testing::TestFailureException(oss.str());                               \
        }                                                                                          \
    } while (0)

#define EXPECT_NE(a, b)                                                                            \
    do {                                                                                           \
        if ((a) == (b)) {                                                                          \
            std::ostringstream oss;                                                                \
            oss << "EXPECT_NE falhou: " #a " != " #b " em " << __FILE__ << ":" << __LINE__;        \
            throw ::xblob::testing::TestFailureException(oss.str());                               \
        }                                                                                          \
    } while (0)

} // namespace xblob::testing
