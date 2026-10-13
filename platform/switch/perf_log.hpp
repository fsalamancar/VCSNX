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
    std::uint64_t ge_wait_us{};   // the game thread waiting at the vblank for the GE worker (ge_async_wait_us)
    std::uint64_t io_us{};        // the game thread reading files (sceIoRead, the world streaming)
};

// Returns nullopt for any line that is not "[frame-time] ..." or has no valid frame_us.
[[nodiscard]] std::optional<FrameTimeSample> parse_frame_time_line(std::string_view line);

// Where the GE worker's time goes in one vblank: one "[ge-phase]" line (vcs_profile.cpp,
// PSPRECOMP_GE_PHASE_DIAG). list_us is what the named phases leave: display-list interpretation.
struct GePhaseSample {
    std::uint64_t setup_us{};      // draw setup and texture binding
    std::uint64_t texupload_us{};  // texture decode and upload
    std::uint64_t vdecode_us{};    // vertex decode (and CPU lighting on the hybrid lit path)
    std::uint64_t stage_us{};
    std::uint64_t triprep_us{};
    std::uint64_t accum_us{};      // appending GPU batches
    std::uint64_t pixel_us{};      // CPU rasterizer pixel loop
    std::uint64_t list_us{};
};

// Returns nullopt for any line that is not "[ge-phase] ..." or has no valid ge_us.
[[nodiscard]] std::optional<GePhaseSample> parse_ge_phase_line(std::string_view line);

// The averages of the last finished window, for LoadingWatch.
struct WindowStats {
    double fps_avg{};
    double ge_ms_avg{};
};

// Accumulates frames until window_us of frame time has passed, then returns one summary line:
// "t=<s>s frames=N fps_avg fps_min guest_ms_avg/max ge_ms_avg/max present_ms_avg/max mem_peak_mb gewait_ms_avg
// io_ms_avg/max",
// then, if the window had ge-phase lines, " phase_ms setup tex vdec stage prep accum pixel list" (per vblank).
class PerfWindow {
public:
    explicit PerfWindow(std::uint64_t window_us = 10'000'000u) : window_us_(window_us) {}

    [[nodiscard]] std::optional<std::string> add(const FrameTimeSample &sample,
                                                 std::uint64_t memory_in_use_mb);
    void add_phase(const GePhaseSample &sample) noexcept;
    [[nodiscard]] WindowStats last_window() const noexcept { return last_; }

private:
    std::uint64_t window_us_;
    std::uint64_t elapsed_us_{};
    std::uint64_t frames_{};
    std::uint64_t sum_frame_us_{}, max_frame_us_{};
    std::uint64_t sum_guest_us_{}, max_guest_us_{};
    std::uint64_t sum_ge_us_{}, max_ge_us_{};
    std::uint64_t sum_present_us_{}, max_present_us_{};
    std::uint64_t sum_ge_wait_us_{};
    std::uint64_t sum_io_us_{}, max_io_us_{};
    std::uint64_t memory_peak_mb_{};
    GePhaseSample phase_sum_{};
    std::uint64_t phase_lines_{};
    WindowStats last_{};
};

// Reads the memory in use for perf.log's mem_peak_mb at most once every `every` frames (and on the first).
// On the Switch memory_in_use_mb() is newlib's mallinfo(), which walks every free chunk of the heap under
// malloc's lock: called for every [frame-time] line it cost the game thread 5-6 ms of every vblank
// ([ge-tl] A -> F, 2026-10-13), about 10 ms of each game frame for every player; read once a second it
// still made a 5-7 ms hitch every second, growing as the heap fragments.
class MemorySampler {
public:
    explicit MemorySampler(std::uint32_t every) noexcept : every_(every == 0u ? 1u : every) {}

    template <typename Read>
    [[nodiscard]] std::uint64_t sample(Read read) {
        if (frames_++ % every_ == 0u) last_ = read();
        return last_;
    }

private:
    std::uint32_t every_;
    std::uint64_t frames_{};
    std::uint64_t last_{};
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
