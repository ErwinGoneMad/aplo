#include <exception>
#include <iostream>

#include "test_framework.hpp"

namespace testkit {

namespace {
int currentFailures = 0;
}

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

void reportFailure(const char* file, int line, const std::string& message) {
    ++currentFailures;
    std::cerr << "    " << file << ":" << line << ": " << message << '\n';
}

}  // namespace testkit

int main() {
    int failedTests = 0;
    for (const auto& test : testkit::registry()) {
        testkit::currentFailures = 0;
        try {
            test.body();
        } catch (const std::exception& error) {
            testkit::reportFailure(__FILE__, __LINE__, std::string("unexpected exception: ") + error.what());
        }
        const bool passed = testkit::currentFailures == 0;
        std::cout << (passed ? "[  OK  ] " : "[ FAIL ] ") << test.name << '\n';
        if (!passed) ++failedTests;
    }
    std::cout << testkit::registry().size() - static_cast<std::size_t>(failedTests) << " passed, "
              << failedTests << " failed\n";
    return failedTests == 0 ? 0 : 1;
}
