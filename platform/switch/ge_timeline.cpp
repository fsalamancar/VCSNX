#include "ge_timeline.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace vcsnx {
namespace {

constexpr std::uint32_t kWindowVblanks = 12u;

std::int64_t steady_us() noexcept {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

} // namespace

GeTimeline::GeTimeline(std::uint64_t every, std::uint32_t length, Clock now_us) noexcept
    : every_(every), length_(length), now_us_(now_us) {}

void GeTimeline::note(char kind, std::uint32_t a, std::int64_t b) noexcept {
    if (!on_.load(std::memory_order_relaxed)) return;
    std::lock_guard<std::mutex> lock(mutex_);
    events_.push_back(Event{now_us_(), kind, a, b});
}

std::vector<std::string> GeTimeline::vblank(std::uint64_t index) {
    std::vector<std::string> lines;
    if (every_ == 0u) return lines;
    if (on_.load(std::memory_order_relaxed) && index >= window_end_) {
        on_.store(false, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lock(mutex_);
        const std::int64_t first = events_.empty() ? 0 : events_.front().us;
        lines.reserve(events_.size());
        for (const Event &event : events_) {
            char text[96];
            std::snprintf(text, sizeof(text), "[ge-tl] t=%lld %c a=%u b=%lld",
                          static_cast<long long>(event.us - first), event.kind, event.a,
                          static_cast<long long>(event.b));
            lines.emplace_back(text);
        }
        events_.clear();
    }
    if (!on_.load(std::memory_order_relaxed) && index % every_ == 0u) {
        window_end_ = index + length_;
        on_.store(true, std::memory_order_relaxed);
    }
    return lines;
}

std::uint64_t ge_timeline_every(const char *setting) noexcept {
    if (setting == nullptr || *setting == '\0') return 0u;
    char *end = nullptr;
    const unsigned long long value = std::strtoull(setting, &end, 10);
    if (end == setting || *end != '\0' || value < 2u * kWindowVblanks) return 0u;
    return value;
}

GeTimeline &ge_timeline() {
    static GeTimeline timeline(ge_timeline_every(std::getenv("VCSNX_GE_TIMELINE_EVERY")), kWindowVblanks,
                               &steady_us);
    return timeline;
}

} // namespace vcsnx
