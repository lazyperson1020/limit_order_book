#pragma once

// Minimal self-registering test framework. No external dependencies so the
// project builds offline with nothing beyond CMake + a C++17 compiler.

#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace testfw {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> cases;
    return cases;
}

struct Registrar {
    Registrar(std::string name, std::function<void()> fn) {
        registry().push_back({std::move(name), std::move(fn)});
    }
};

inline int failureCount = 0;
inline std::string currentTest;

inline void reportFailure(const char* expr, const char* file, int line) {
    std::cerr << "  [FAIL] " << currentTest << ": " << expr << " (" << file << ":" << line << ")\n";
    ++failureCount;
}

} // namespace testfw

#define LOB_CONCAT_INNER(a, b) a##b
#define LOB_CONCAT(a, b) LOB_CONCAT_INNER(a, b)

#define TEST_CASE(name)                                                                \
    static void name();                                                                \
    static ::testfw::Registrar LOB_CONCAT(registrar_, name){#name, name};              \
    static void name()

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) ::testfw::reportFailure(#cond, __FILE__, __LINE__);               \
    } while (0)

#define REQUIRE(cond)                                                                  \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            ::testfw::reportFailure(#cond, __FILE__, __LINE__);                        \
            return;                                                                    \
        }                                                                              \
    } while (0)
