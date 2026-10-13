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
    if (sample.since_submit_ms > 0.0) {  // a wait for the previous frame happened
        sum_.since_submit_ms += sample.since_submit_ms;
        ++waits_;
        if (sample.ready) ++ready_frames_;
    }
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
    const double waits = waits_ > 0 ? static_cast<double>(waits_) : 1.0;
    const long long ready_percent = waits_ > 0 ? ready_frames_ * 100 / waits_ : 0;
    char line[320];
    std::snprintf(line, sizeof(line),
                  "[present-phases] frames=%lld fence=%.2f readback=%.2f build=%.2f gpu=%.2f record=%.2f total_max=%.2f "
                  "deferred=%lld since_submit=%.2f ready=%lld%%",
                  frames_, sum_.fence_ms / n, sum_.readback_ms / n, build, sum_.gpu_ms / n, sum_.record_ms / n,
                  total_max_ms_, deferred_, sum_.since_submit_ms / waits, ready_percent);
    sum_ = {};
    total_max_ms_ = 0.0;
    frames_ = 0;
    ready_frames_ = 0;
    waits_ = 0;
    deferred_ = 0;
    window_start_ms_ = now_ms;
    return std::string(line);
}

} // namespace vcsnx
