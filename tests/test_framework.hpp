#pragma once

// Minimal test harness: TEST registers a function, CHECK* record failures
// and let the test continue.

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace testkit {

struct TestCase {
    const char* name;
    void (*body)();
};

std::vector<TestCase>& registry();
void reportFailure(const char* file, int line, const std::string& message);

struct Registrar {
    Registrar(const char* name, void (*body)()) { registry().push_back({name, body}); }
};

}  // namespace testkit

#define TEST(name)                                                   \
    static void name();                                              \
    static const ::testkit::Registrar name##Registrar{#name, &name}; \
    static void name()

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            ::testkit::reportFailure(__FILE__, __LINE__, "CHECK(" #condition ") failed"); \
        }                                                                              \
    } while (false)

#define CHECK_EQ(actual, expected)                                                         \
    do {                                                                                   \
        const auto& actualValue = (actual);                                                \
        const auto& expectedValue = (expected);                                            \
        if (!(actualValue == expectedValue)) {                                             \
            std::ostringstream message;                                                    \
            message << "CHECK_EQ(" #actual ", " #expected ")\n      got:      " << actualValue \
                    << "\n      expected: " << expectedValue;                              \
            ::testkit::reportFailure(__FILE__, __LINE__, message.str());                   \
        }                                                                                  \
    } while (false)

// Checks that `statement` throws `ExceptionType` whose what() contains `fragment`.
#define CHECK_THROWS_WITH(statement, ExceptionType, fragment)                                    \
    do {                                                                                         \
        bool thrown = false;                                                                     \
        try {                                                                                    \
            statement;                                                                           \
        } catch (const ExceptionType& error) {                                                   \
            thrown = true;                                                                       \
            if (std::string_view(error.what()).find(fragment) == std::string_view::npos) {       \
                ::testkit::reportFailure(__FILE__, __LINE__,                                     \
                                         std::string("message '") + error.what() +               \
                                             "' does not contain '" + (fragment) + "'");         \
            }                                                                                    \
        }                                                                                        \
        if (!thrown) {                                                                           \
            ::testkit::reportFailure(__FILE__, __LINE__, "expected " #ExceptionType " from " #statement); \
        }                                                                                        \
    } while (false)
