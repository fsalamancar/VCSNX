#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace vcsnx {

// Performance measurement: where a Switch frame waits. With VCSNX_GE_TIMELINE_EVERY=N (vcsnx_env.txt),
// every N vblanks the next `length` vblanks of guest and GE worker events are kept and then printed
// to stderr as "[ge-tl] t=<us since the window's first event> <kind> a=<n> b=<n>". Kinds (vcs_profile.cpp):
// V vblank start (a=index), A GE wait done, d frame-time diagnostics done, k backend locked, r prompt
// text, weather and clock done, F/f finish_color_frame start/end (f a=1: a new frame),
// P frame published, L frame limiter done, q list enqueued (a=address), u stall update (a=list id),
// s/e GE worker task start/end (a=list id), w/x wait for an idle GE (w a=outstanding, x b=us),
// W/X wait for one list (a=list id, X b=us).
// Off (N=0), note() is one relaxed atomic load.
class GeTimeline {
public:
    using Clock = std::int64_t (*)() noexcept;

    GeTimeline(std::uint64_t every, std::uint32_t length, Clock now_us) noexcept;

    // Any thread.
    void note(char kind, std::uint32_t a = 0u, std::int64_t b = 0) noexcept;

    // The game thread, at the start of vblank `index` (1, 2, ...): opens or closes the window. Returns
    // the closed window's lines, empty otherwise.
    [[nodiscard]] std::vector<std::string> vblank(std::uint64_t index);

private:
    struct Event {
        std::int64_t us;
        char kind;
        std::uint32_t a;
        std::int64_t b;
    };
    std::uint64_t every_;
    std::uint32_t length_;
    Clock now_us_;
    std::atomic<bool> on_{false};
    std::uint64_t window_end_{};
    std::mutex mutex_;
    std::vector<Event> events_;
};

// VCSNX_GE_TIMELINE_EVERY: a vblank count of at least 24 (two windows' length), otherwise 0 (off).
[[nodiscard]] std::uint64_t ge_timeline_every(const char *setting) noexcept;

// The process's timeline, from getenv("VCSNX_GE_TIMELINE_EVERY"), 12 vblanks per window.
GeTimeline &ge_timeline();

} // namespace vcsnx
