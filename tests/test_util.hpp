//
// Minimal assertion helpers. Deliberately dependency-free: the project does not
// depend on a test framework, and a handful of checks does not justify adding one.
//
#pragma once
#include <cmath>
#include <cstdio>
#include <string>

namespace test_util {

inline int &failureCount() {
    static int failures = 0;
    return failures;
}

inline void report(bool passed, const char *expression, const char *file, int line) {
    if (!passed) {
        ++failureCount();
        std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, expression);
    }
}

inline int summary(const char *suite) {
    if (failureCount() == 0) {
        std::printf("PASS %s\n", suite);
        return 0;
    }
    std::fprintf(stderr, "FAILED %s: %d check(s)\n", suite, failureCount());
    return 1;
}

} // namespace test_util

#define CHECK(expr) test_util::report((expr), #expr, __FILE__, __LINE__)
#define CHECK_EQ(a, b) test_util::report((a) == (b), #a " == " #b, __FILE__, __LINE__)
#define CHECK_NEAR(a, b, tol) test_util::report(std::fabs((a) - (b)) <= (tol), #a " ~= " #b, __FILE__, __LINE__)
// Same as CHECK, with a run-time label identifying which case failed.
#define CHECK_LABELED(expr, label)                                                                                     \
    test_util::report((expr), (std::string(#expr) + " [" + (label) + "]").c_str(), __FILE__, __LINE__)
