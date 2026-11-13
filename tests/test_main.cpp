#include "test_framework.hpp"

int main() {
    int total = 0;
    for (auto& tc : testfw::registry()) {
        testfw::currentTest = tc.name;
        ++total;
        tc.fn();
    }

    std::cout << total << " test case(s) run, " << testfw::failureCount << " assertion failure(s).\n";
    return testfw::failureCount == 0 ? 0 : 1;
}
