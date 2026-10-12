#pragma once
// perf.log (spec §7): parses the vendor's "[frame-time]" lines and summarises them every 10 s.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace vcsnx {

// The fields VCSNX uses from one "[frame-time]" line (vcs_profile.cpp, PSPRECOMP_FRAME_TIME_DIAG).
struct FrameTimeSample {
    std::uint64_t frame_us{};
    std::uint64_t ge_us{};        // GE / software rasterizer time inside the frame
    std::uint64_t present_us{};
    std::uint64_t guest_cpu_us{}; // guest CPU time with GE, present and IO taken out
};

// Returns nullopt for any line that is not "[frame-time] ..." or has no valid frame_us.
[[nodiscard]] std::optional<FrameTimeSample> parse_frame_time_line(std::string_view line);

// Accumulates frames until window_us of frame time has passed, then returns one summary line:
// "t=<s>s frames=N fps_avg fps_min guest_ms_avg/max ge_ms_avg/max present_ms_avg/max mem_peak_mb".
class PerfWindow {
public:
    explicit PerfWindow(std::uint64_t window_us = 10'000'000u) : window_us_(window_us) {}

    [[nodiscard]] std::optional<std::string> add(const FrameTimeSample &sample,
                                                 std::uint64_t memory_in_use_mb);

private:
    std::uint64_t window_us_;
    std::uint64_t elapsed_us_{};
    std::uint64_t frames_{};
    std::uint64_t sum_frame_us_{}, max_frame_us_{};
    std::uint64_t sum_guest_us_{}, max_guest_us_{};
    std::uint64_t sum_ge_us_{}, max_ge_us_{};
    std::uint64_t sum_present_us_{}, max_present_us_{};
    std::uint64_t memory_peak_mb_{};
};

} // namespace vcsnx
