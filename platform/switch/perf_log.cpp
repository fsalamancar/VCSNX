#include "perf_log.hpp"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <system_error>

namespace vcsnx {

std::optional<FrameTimeSample> parse_frame_time_line(std::string_view line) {
    constexpr std::string_view kPrefix = "[frame-time] ";
    if (line.substr(0, kPrefix.size()) != kPrefix) return std::nullopt;
    line.remove_prefix(kPrefix.size());

    FrameTimeSample sample;
    bool have_frame = false;
    while (!line.empty()) {
        const std::size_t space = line.find(' ');
        const std::string_view token = line.substr(0, space);
        line = space == std::string_view::npos ? std::string_view{} : line.substr(space + 1u);
        const std::size_t equals = token.find('=');
        if (equals == std::string_view::npos) continue;
        const std::string_view key = token.substr(0, equals);
        const std::string_view text = token.substr(equals + 1u);
        std::uint64_t value = 0u;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        const bool valid = error == std::errc{} && end == text.data() + text.size();
        if (key == "frame_us") {
            if (!valid) return std::nullopt;
            sample.frame_us = value;
            have_frame = true;
        } else if (valid && key == "ge_us") {
            sample.ge_us = value;
        } else if (valid && key == "present_us") {
            sample.present_us = value;
        } else if (valid && key == "guest_cpu_us") {
            sample.guest_cpu_us = value;
        }
    }
    if (!have_frame) return std::nullopt;
    return sample;
}

std::optional<std::string> PerfWindow::add(const FrameTimeSample &sample, std::uint64_t memory_in_use_mb) {
    ++frames_;
    sum_frame_us_ += sample.frame_us;
    max_frame_us_ = std::max(max_frame_us_, sample.frame_us);
    sum_guest_us_ += sample.guest_cpu_us;
    max_guest_us_ = std::max(max_guest_us_, sample.guest_cpu_us);
    sum_ge_us_ += sample.ge_us;
    max_ge_us_ = std::max(max_ge_us_, sample.ge_us);
    sum_present_us_ += sample.present_us;
    max_present_us_ = std::max(max_present_us_, sample.present_us);
    memory_peak_mb_ = std::max(memory_peak_mb_, memory_in_use_mb);
    if (sum_frame_us_ < window_us_) return std::nullopt;

    elapsed_us_ += sum_frame_us_;
    const double frames = static_cast<double>(frames_);
    const auto average_ms = [frames](std::uint64_t sum_us) { return static_cast<double>(sum_us) / 1000.0 / frames; };
    const auto ms = [](std::uint64_t us) { return static_cast<double>(us) / 1000.0; };
    const double fps_average = frames * 1'000'000.0 / static_cast<double>(sum_frame_us_);
    const double fps_minimum = max_frame_us_ == 0u ? 0.0 : 1'000'000.0 / static_cast<double>(max_frame_us_);

    char text[320];
    std::snprintf(text, sizeof(text),
                  "t=%llus frames=%llu fps_avg=%.1f fps_min=%.1f guest_ms_avg=%.1f guest_ms_max=%.1f "
                  "ge_ms_avg=%.1f ge_ms_max=%.1f present_ms_avg=%.1f present_ms_max=%.1f mem_peak_mb=%llu",
                  static_cast<unsigned long long>(elapsed_us_ / 1'000'000u),
                  static_cast<unsigned long long>(frames_), fps_average, fps_minimum,
                  average_ms(sum_guest_us_), ms(max_guest_us_), average_ms(sum_ge_us_), ms(max_ge_us_),
                  average_ms(sum_present_us_), ms(max_present_us_),
                  static_cast<unsigned long long>(memory_peak_mb_));

    frames_ = 0u;
    sum_frame_us_ = max_frame_us_ = 0u;
    sum_guest_us_ = max_guest_us_ = 0u;
    sum_ge_us_ = max_ge_us_ = 0u;
    sum_present_us_ = max_present_us_ = 0u;
    memory_peak_mb_ = 0u;
    return std::string(text);
}

} // namespace vcsnx
