#pragma once
// Minimal test runner in the vendor's test style (plain executables, require() throws).

#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace vcsnx_test {

inline void require(bool condition, const std::string &message) {
    if (!condition) throw std::runtime_error(message);
}

using Case = std::pair<const char *, std::function<void()>>;

// Runs every case, prints ok/FAIL per case, and returns the process exit status.
inline int run(const std::vector<Case> &cases) {
    int failures = 0;
    for (const auto &[name, body] : cases) {
        try {
            body();
            std::cout << "ok   " << name << "\n";
        } catch (const std::exception &error) {
            ++failures;
            std::cout << "FAIL " << name << ": " << error.what() << "\n";
        }
    }
    return failures == 0 ? 0 : 1;
}

} // namespace vcsnx_test
