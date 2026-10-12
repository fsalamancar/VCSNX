#pragma once
// One startup clock for VCSNX's diagnostic traces, shared by every translation unit (inline
// function: a single static across the program), so lines from different threads line up.

#include <chrono>

namespace vcsnx {

[[nodiscard]] inline long long trace_ms() {
    static const auto start = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
}

} // namespace vcsnx
