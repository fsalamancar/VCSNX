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

// The averages of the last finished window, for LoadingWatch.
struct WindowStats {
    double fps_avg{};
    double ge_ms_avg{};
};

// Accumulates frames until window_us of frame time has passed, then returns one summary line:
// "t=<s>s frames=N fps_avg fps_min guest_ms_avg/max ge_ms_avg/max present_ms_avg/max mem_peak_mb".
class PerfWindow {
public:
    explicit PerfWindow(std::uint64_t window_us = 10'000'000u) : window_us_(window_us) {}

    [[nodiscard]] std::optional<std::string> add(const FrameTimeSample &sample,
                                                 std::uint64_t memory_in_use_mb);
    [[nodiscard]] WindowStats last_window() const noexcept { return last_; }

private:
    std::uint64_t window_us_;
    std::uint64_t elapsed_us_{};
    std::uint64_t frames_{};
    std::uint64_t sum_frame_us_{}, max_frame_us_{};
    std::uint64_t sum_guest_us_{}, max_guest_us_{};
    std::uint64_t sum_ge_us_{}, max_ge_us_{};
    std::uint64_t sum_present_us_{}, max_present_us_{};
    std::uint64_t memory_peak_mb_{};
    WindowStats last_{};
};

// Backlog bug 1: some loads never end. A loading screen in perf.log is ~60 fps with almost no GE work;
// three such windows in a row (30 s) ask once for a dump of the game's scheduler, at most three times
// per session, and again only after the game has moved on.
class LoadingWatch {
public:
    [[nodiscard]] bool observe(const WindowStats &window) noexcept;

private:
    int consecutive_{};
    int reports_{};
    bool reported_{};
};

} // namespace vcsnx
