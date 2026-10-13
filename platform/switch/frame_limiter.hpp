#pragma once

namespace vcsnx {

// The frame limiter (vcs_profile.cpp, limit_frame_rate) sleeps each vblank until its wall-clock slot,
// and a vblank that missed its slot moves the slot to "now". At FrameRate=30 the game waits for two
// vblanks per frame: A after the whole frame's work, B a few ms later after only the composition into
// the displayed buffer. On a slow host A is always late, so B always looked early and slept to fill its
// 16.7 ms: 7-15 ms of every game frame (Mac with slow A vblanks, [ge-tl] P -> L), counted in perf.log
// as guest time. After a late vblank the next one does not sleep; a host that keeps its slots still
// sleeps every early vblank, so the cap holds.
class LimiterBurst {
public:
    // Whether the limiter may sleep at the vblank it is pacing now.
    [[nodiscard]] bool may_sleep() const noexcept { return !previous_late_; }
    // Records whether that vblank was late (the limiter caught up instead of sleeping).
    void note(bool late) noexcept { previous_late_ = late; }

private:
    bool previous_late_{};
};

} // namespace vcsnx
