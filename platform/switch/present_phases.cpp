#include "present_phases.hpp"

#include <algorithm>
#include <cstdio>

namespace vcsnx {

void PresentPhases::add(const PresentPhaseSample &sample) noexcept {
    sum_.fence_ms += sample.fence_ms;
    sum_.readback_ms += sample.readback_ms;
    sum_.total_ms += sample.total_ms;
    sum_.gpu_ms += sample.gpu_ms;
    sum_.record_ms += sample.record_ms;
    total_max_ms_ = std::max(total_max_ms_, sample.total_ms);
    ++frames_;
}

std::optional<std::string> PresentPhases::take_report_if_due(long long now_ms) {
    if (window_start_ms_ < 0) {
        window_start_ms_ = now_ms;
        return std::nullopt;
    }
    if (now_ms - window_start_ms_ < 10'000) return std::nullopt;
    const double n = frames_ > 0 ? static_cast<double>(frames_) : 1.0;
    const double build = (sum_.total_ms - sum_.fence_ms - sum_.readback_ms) / n;
    char line[256];
    std::snprintf(line, sizeof(line),
                  "[present-phases] frames=%lld fence=%.2f readback=%.2f build=%.2f gpu=%.2f record=%.2f total_max=%.2f "
                  "deferred=%lld",
                  frames_, sum_.fence_ms / n, sum_.readback_ms / n, build, sum_.gpu_ms / n, sum_.record_ms / n,
                  total_max_ms_, deferred_);
    sum_ = {};
    total_max_ms_ = 0.0;
    frames_ = 0;
    deferred_ = 0;
    window_start_ms_ = now_ms;
    return std::string(line);
}

} // namespace vcsnx
