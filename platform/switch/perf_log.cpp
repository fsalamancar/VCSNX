#include "perf_log.hpp"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <system_error>

namespace vcsnx {
namespace {

// Calls field(key, value, valid) for each "key=value" token after `prefix`; false if the prefix differs.
template <typename Field>
bool for_each_field(std::string_view line, std::string_view prefix, Field field) {
    if (line.substr(0, prefix.size()) != prefix) return false;
    line.remove_prefix(prefix.size());
    while (!line.empty()) {
        const std::size_t space = line.find(' ');
        const std::string_view token = line.substr(0, space);
        line = space == std::string_view::npos ? std::string_view{} : line.substr(space + 1u);
        const std::size_t equals = token.find('=');
        if (equals == std::string_view::npos) continue;
        const std::string_view text = token.substr(equals + 1u);
        std::uint64_t value = 0u;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (!field(token.substr(0, equals), value, error == std::errc{} && end == text.data() + text.size()))
            return false;
    }
    return true;
}

} // namespace

std::optional<FrameTimeSample> parse_frame_time_line(std::string_view line) {
    FrameTimeSample sample;
    bool have_frame = false;
    const bool parsed = for_each_field(line, "[frame-time] ", [&](std::string_view key, std::uint64_t value, bool valid) {
        if (key == "frame_us") {
            if (!valid) return false;
            sample.frame_us = value;
            have_frame = true;
        } else if (valid && key == "ge_us") {
            sample.ge_us = value;
        } else if (valid && key == "present_us") {
            sample.present_us = value;
        } else if (valid && key == "guest_cpu_us") {
            sample.guest_cpu_us = value;
        } else if (valid && key == "ge_async_wait_us") {
            sample.ge_wait_us = value;
        } else if (valid && key == "io_us") {
            sample.io_us = value;
        }
        return true;
    });
    if (!parsed || !have_frame) return std::nullopt;
    return sample;
}

std::optional<GePhaseSample> parse_ge_phase_line(std::string_view line) {
    GePhaseSample sample;
    bool have_ge = false;
    const bool parsed = for_each_field(line, "[ge-phase] ", [&](std::string_view key, std::uint64_t value, bool valid) {
        if (!valid) return true;
        if (key == "ge_us") have_ge = true;
        else if (key == "setup_us") sample.setup_us = value;
        else if (key == "texupload_us") sample.texupload_us = value;
        else if (key == "vdecode_us") sample.vdecode_us = value;
        else if (key == "stage_us") sample.stage_us = value;
        else if (key == "triprep_us") sample.triprep_us = value;
        else if (key == "accum_us") sample.accum_us = value;
        else if (key == "pixel_us") sample.pixel_us = value;
        else if (key == "list_us") sample.list_us = value;
        return true;
    });
    if (!parsed || !have_ge) return std::nullopt;
    return sample;
}

void PerfWindow::add_phase(const GePhaseSample &sample) noexcept {
    ++phase_lines_;
    phase_sum_.setup_us += sample.setup_us;
    phase_sum_.texupload_us += sample.texupload_us;
    phase_sum_.vdecode_us += sample.vdecode_us;
    phase_sum_.stage_us += sample.stage_us;
    phase_sum_.triprep_us += sample.triprep_us;
    phase_sum_.accum_us += sample.accum_us;
    phase_sum_.pixel_us += sample.pixel_us;
    phase_sum_.list_us += sample.list_us;
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
    sum_ge_wait_us_ += sample.ge_wait_us;
    sum_io_us_ += sample.io_us;
    max_io_us_ = std::max(max_io_us_, sample.io_us);
    memory_peak_mb_ = std::max(memory_peak_mb_, memory_in_use_mb);
    if (sum_frame_us_ < window_us_) return std::nullopt;

    elapsed_us_ += sum_frame_us_;
    const double frames = static_cast<double>(frames_);
    const auto average_ms = [frames](std::uint64_t sum_us) { return static_cast<double>(sum_us) / 1000.0 / frames; };
    const auto ms = [](std::uint64_t us) { return static_cast<double>(us) / 1000.0; };
    const double fps_average = frames * 1'000'000.0 / static_cast<double>(sum_frame_us_);
    const double fps_minimum = max_frame_us_ == 0u ? 0.0 : 1'000'000.0 / static_cast<double>(max_frame_us_);
    last_ = WindowStats{fps_average, average_ms(sum_ge_us_)};

    char text[448];
    std::snprintf(text, sizeof(text),
                  "t=%llus frames=%llu fps_avg=%.1f fps_min=%.1f guest_ms_avg=%.1f guest_ms_max=%.1f "
                  "ge_ms_avg=%.1f ge_ms_max=%.1f present_ms_avg=%.1f present_ms_max=%.1f mem_peak_mb=%llu "
                  "gewait_ms_avg=%.1f io_ms_avg=%.1f io_ms_max=%.1f",
                  static_cast<unsigned long long>(elapsed_us_ / 1'000'000u),
                  static_cast<unsigned long long>(frames_), fps_average, fps_minimum,
                  average_ms(sum_guest_us_), ms(max_guest_us_), average_ms(sum_ge_us_), ms(max_ge_us_),
                  average_ms(sum_present_us_), ms(max_present_us_),
                  static_cast<unsigned long long>(memory_peak_mb_), average_ms(sum_ge_wait_us_),
                  average_ms(sum_io_us_), ms(max_io_us_));
    std::string summary(text);
    if (phase_lines_ != 0u) {
        const double lines = static_cast<double>(phase_lines_);
        const auto phase_ms = [lines](std::uint64_t sum_us) { return static_cast<double>(sum_us) / 1000.0 / lines; };
        std::snprintf(text, sizeof(text),
                      " phase_ms setup=%.1f tex=%.1f vdec=%.1f stage=%.1f prep=%.1f accum=%.1f pixel=%.1f list=%.1f",
                      phase_ms(phase_sum_.setup_us), phase_ms(phase_sum_.texupload_us), phase_ms(phase_sum_.vdecode_us),
                      phase_ms(phase_sum_.stage_us), phase_ms(phase_sum_.triprep_us), phase_ms(phase_sum_.accum_us),
                      phase_ms(phase_sum_.pixel_us), phase_ms(phase_sum_.list_us));
        summary += text;
    }

    frames_ = 0u;
    sum_frame_us_ = max_frame_us_ = 0u;
    sum_guest_us_ = max_guest_us_ = 0u;
    sum_ge_us_ = max_ge_us_ = 0u;
    sum_present_us_ = max_present_us_ = 0u;
    sum_ge_wait_us_ = 0u;
    sum_io_us_ = max_io_us_ = 0u;
    memory_peak_mb_ = 0u;
    phase_sum_ = GePhaseSample{};
    phase_lines_ = 0u;
    return summary;
}

bool LoadingWatch::observe(const WindowStats &window) noexcept {
    const bool loading = window.fps_avg >= 50.0 && window.ge_ms_avg < 1.5;
    if (!loading) {
        consecutive_ = 0;
        reported_ = false;
        return false;
    }
    if (++consecutive_ < 3 || reported_ || reports_ >= 3) return false;
    reported_ = true;
    ++reports_;
    return true;
}

} // namespace vcsnx
