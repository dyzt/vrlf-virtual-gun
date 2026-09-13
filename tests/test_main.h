#pragma once
// Minimal test harness: TEST registers, CHECK records, run_all() reports.
#include <cstdio>
#include <functional>
#include <utility>
#include <vector>

struct TestCase {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& test_registry() {
    static std::vector<TestCase> r;
    return r;
}

inline int& test_failures() {
    static int f = 0;
    return f;
}

struct TestRegistrar {
    TestRegistrar(const char* name, std::function<void()> fn) {
        test_registry().push_back({name, std::move(fn)});
    }
};

#define TEST(name)                                          \
    static void name();                                     \
    static TestRegistrar registrar_##name(#name, name);     \
    static void name()

#define CHECK(cond)                                                             \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
            ++test_failures();                                                  \
        }                                                                       \
    } while (0)

#define CHECK_EQ(a, b)                                                          \
    do {                                                                        \
        const long long check_a = static_cast<long long>(a);                    \
        const long long check_b = static_cast<long long>(b);                    \
        if (check_a != check_b) {                                               \
            std::printf("  FAIL %s:%d: %s == %s (%lld vs %lld)\n", __FILE__,    \
                        __LINE__, #a, #b, check_a, check_b);                    \
            ++test_failures();                                                  \
        }                                                                       \
    } while (0)

inline int run_all() {
    for (auto& t : test_registry()) {
        const int before = test_failures();
        t.fn();
        std::printf("%s %s\n", test_failures() == before ? "ok  " : "FAIL", t.name);
    }
    std::printf("%d failure(s)\n", test_failures());
    return test_failures() == 0 ? 0 : 1;
}
