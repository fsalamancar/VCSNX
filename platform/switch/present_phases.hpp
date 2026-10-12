#pragma once
#include <optional>
#include <string>

namespace vcsnx {

// Where the Vulkan backend's per-frame "present" time goes (VCSNX_PRESENT_PHASES=1), averaged over
// 10 s so the SD card is not written every frame: waiting for the GPU's previous frame (fence),
// copying its pixels out (readback), everything else in finish_color_frame (build: pick the frame,
// record and submit), plus the GPU's own execution and command-recording time from its timestamps.
struct PresentPhaseSample {
    double fence_ms{}, readback_ms{}, total_ms{}, gpu_ms{}, record_ms{};
};

// At the PSP's 30 FPS the game draws on every other vblank, yet the backend used to start every vblank
// by waiting for the GPU's previous frame (Switch run 12: 6-18 ms per vblank in heavy scenes). When
// this vblank brings no new geometry and that frame is not finished, skip the wait: the game runs its
// next vblank while the GPU works, and the frame is collected one vblank later.
[[nodiscard]] constexpr bool defer_previous_frame(bool submission_pending, bool has_new_geometry,
                                                  bool previous_frame_done) noexcept {
    return submission_pending && !has_new_geometry && !previous_frame_done;
}

class PresentPhases {
public:
    void add(const PresentPhaseSample &sample) noexcept;
    void add_deferred() noexcept { ++deferred_; }
    // The first call starts the window; once 10 s have passed, returns
    // "[present-phases] frames=N fence=.. readback=.. build=.. gpu=.. record=.. total_max=.. deferred=N"
    // (ms; deferred counts vblanks that skipped the GPU wait) and restarts.
    [[nodiscard]] std::optional<std::string> take_report_if_due(long long now_ms);

private:
    PresentPhaseSample sum_{};
    double total_max_ms_{};
    long long frames_{};
    long long deferred_{};
    long long window_start_ms_{-1};
};

} // namespace vcsnx
