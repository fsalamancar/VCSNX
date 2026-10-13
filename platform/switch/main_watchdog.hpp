#pragma once
#include <atomic>

namespace vcsnx {

// Freeze evidence (2026-10-13: the picture froze in the pause menu and the pad stopped answering while the game
// thread kept running its vblanks at 60/s: the main thread, which presents and pumps the pad, had stopped).
// The main thread notes every present; the game thread, at every frame it publishes, asks whether the main
// thread has gone `limit_ms` without one. Then (Switch) the main thread is paused for an instant, its PC, LR and
// frame-pointer chain are logged as "[vcsnx-watchdog]", and it is resumed. Once per stall.
class StallWatch {
public:
    explicit constexpr StallWatch(long long limit_ms) noexcept : limit_ms_(limit_ms) {}

    void presented(long long now_ms) noexcept {
        last_present_ms_.store(now_ms, std::memory_order_relaxed);
        reported_.store(false, std::memory_order_relaxed);
    }

    [[nodiscard]] bool should_report(long long now_ms) noexcept {
        const long long last = last_present_ms_.load(std::memory_order_relaxed);
        if (last < 0 || now_ms - last < limit_ms_) return false;
        return !reported_.exchange(true, std::memory_order_relaxed);
    }

private:
    long long limit_ms_;
    std::atomic<long long> last_present_ms_{-1};
    std::atomic<bool> reported_{false};
};

// The main thread, after each SDL present.
void note_main_present(long long now_ms) noexcept;
// The game thread, at each published frame: reports a main thread stalled for 3 s.
void check_main_thread(long long now_ms) noexcept;

} // namespace vcsnx
