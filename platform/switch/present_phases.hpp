#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace vcsnx {

// Where the Vulkan backend's per-frame "present" time goes (VCSNX_PRESENT_PHASES=1), averaged over
// 10 s so the SD card is not written every frame: waiting for the GPU's previous frame (fence),
// copying its pixels out (readback), everything else in finish_color_frame (build: pick the frame,
// record and submit), plus the GPU's own execution and command-recording time from its timestamps.
struct PresentPhaseSample {
    double fence_ms{}, readback_ms{}, total_ms{}, gpu_ms{}, record_ms{};
    // When the wait for the previous frame began, how long ago it was submitted, and whether the GPU had
    // already finished it: the Switch waited 9-14 ms, about the whole GPU time, although a guest frame
    // had passed since the submit.
    double since_submit_ms{};
    bool ready{};
};

// At the PSP's 30 FPS the game draws on every other vblank, yet the backend used to start every vblank
// by waiting for the GPU's previous frame (Switch run 12: 6-18 ms per vblank in heavy scenes). When
// this vblank brings no new geometry and that frame is not finished, skip the wait: the game runs its
// next vblank while the GPU works, and the frame is collected one vblank later.
[[nodiscard]] constexpr bool defer_previous_frame(bool submission_pending, bool has_new_geometry,
                                                  bool previous_frame_done) noexcept {
    return submission_pending && !has_new_geometry && !previous_frame_done;
}

// Whether a batch counts as new drawing for defer_previous_frame. After each frame the game composes it into
// the displayed buffer with strips that sample the world target (Mac [finish-timeline]: a bucket of 64 strips
// at 0x178000 on the vblank after the frame's). That bucket is never drawn here (the backend presents the world
// target), yet it made the next vblank wait for the GPU: ~18 ms per game frame on the Switch (present phases:
// since_submit 8.5 ms, ready 0%). Clears and copies of another VRAM target are not drawing; the frame is then
// collected on the next vblank that draws, when the GPU has finished it.
[[nodiscard]] constexpr bool batch_draws(bool clear_mode, std::uint32_t sampled_vram_address,
                                         std::uint32_t bucket_address) noexcept {
    return !clear_mode && (sampled_vram_address == 0u || sampled_vram_address == bucket_address);
}

class PresentPhases {
public:
    void add(const PresentPhaseSample &sample) noexcept;
    void add_deferred() noexcept { ++deferred_; }
    // The first call starts the window; once 10 s have passed, returns
    // "[present-phases] frames=N fence=.. readback=.. build=.. gpu=.. record=.. total_max=.. deferred=N
    // since_submit=.. ready=P%" (ms; deferred counts vblanks that skipped the GPU wait; since_submit and ready
    // count only the vblanks that waited for a previous frame) and restarts.
    [[nodiscard]] std::optional<std::string> take_report_if_due(long long now_ms);

private:
    PresentPhaseSample sum_{};
    double total_max_ms_{};
    long long frames_{};
    long long waits_{};
    long long ready_frames_{};
    long long deferred_{};
    long long window_start_ms_{-1};
};

} // namespace vcsnx
