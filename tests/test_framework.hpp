#pragma once

// Tiny test framework, no external dependencies.

#include <bits/stdc++.h>

using namespace std;

namespace testfw {

struct TestCase {
    string name;
    function<void()> fn;
};

inline vector<TestCase>& registry() {
    static vector<TestCase> cases;
    return cases;
}

struct Registrar {
    Registrar(string name, function<void()> fn) {
        registry().push_back({name, fn});
    }
};

inline int failureCount = 0;
inline string currentTest;

inline void reportFailure(const char* expr, const char* file, int line) {
    cerr << "  [FAIL] " << currentTest << ": " << expr << " (" << file << ":" << line << ")\n";
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
