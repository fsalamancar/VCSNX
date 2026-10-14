#include "hud_diag.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unordered_set>

namespace vcsnx {
namespace {

// The gate's input, counted the same way (kWorldActivityWindowMs windows).
struct WorldWindows {
    long long start_ms = -1'000'000;
    int current = 0;
    int previous = 0;
    // Full windows that ended within the last second.
    long long second_start_ms = -1'000'000;
    int second_min = INT_MAX;
    int second_max = 0;
};
WorldWindows world;

// Bottom draws since the last summary line.
struct Summary {
    long long start_ms = -1;
    int shrunk = 0;
    int kept = 0;
};
Summary summary;

std::unordered_set<std::uint64_t> seen_boxes;
constexpr std::size_t kMaxBoxes = 4096;
constexpr int kMaxSummaries = 7200;  // two hours of play with bottom texts on screen
int summaries = 0;

std::uint64_t box_key(const HudBox &box, bool gate_on, bool menu_target, bool shrunk) noexcept {
    const auto q = [](float v) {
        return static_cast<std::uint64_t>(std::clamp(std::lround(v / 2.0f) + 1024L, 0L, 4095L));
    };
    return q(box.x0) | q(box.y0) << 12 | q(box.x1) << 24 | q(box.y1) << 36 | std::uint64_t{gate_on} << 48 |
           std::uint64_t{menu_target} << 49 | std::uint64_t{shrunk} << 50;
}

int world_now(long long now_ms) noexcept {
    return now_ms - world.start_ms < kWorldActivityWindowMs ? world.current : 0;
}

} // namespace

bool hud_diag_on() noexcept {
    static const bool on = std::getenv("VCSNX_HUD_DIAG") != nullptr;
    return on;
}

void hud_diag_world_draw(long long now_ms) noexcept {
    if (now_ms - world.start_ms >= kWorldActivityWindowMs) {
        world.previous = now_ms - world.start_ms < 2 * kWorldActivityWindowMs ? world.current : 0;
        world.current = 0;
        world.start_ms = now_ms;
        if (now_ms - world.second_start_ms >= 1000) {
            world.second_start_ms = now_ms;
            world.second_min = INT_MAX;
            world.second_max = 0;
        }
        world.second_min = std::min(world.second_min, world.previous);
        world.second_max = std::max(world.second_max, world.previous);
    }
    ++world.current;
}

void hud_diag_gate(long long now_ms, bool on) noexcept {
    std::fprintf(stderr, "[hud-diag] t=%lldms shrink=%s world_window=%d previous=%d\n", now_ms, on ? "on" : "off",
                 world_now(now_ms), world.previous);
}

void hud_diag_draw(long long now_ms, const HudBox &draw, const HudBox &scissor, float frame_width,
                   float frame_height, bool depth_tested, bool gate_on, bool menu_target,
                   const HudShrink &shrink, const HudTexture &texture) noexcept {
    const bool scissored = scissor.x1 - scissor.x0 < 0.6f * frame_width || scissor.y1 - scissor.y0 < 0.6f * frame_height;
    const HudBox &widget = scissored ? scissor : draw;
    if (widget.y1 <= frame_height / 3.0f) return;  // the middle (cutscene subtitles) and the bottom
    if (widget.x1 <= 0.2f * frame_width && widget.y0 >= 0.66f * frame_height) return;  // the radar
    if (seen_boxes.size() < kMaxBoxes && seen_boxes.insert(box_key(draw, gate_on, menu_target, shrink.apply)).second) {
        std::fprintf(stderr,
                     "[hud-box] t=%lldms gate=%s%s depth=%d box=(%.1f,%.1f)-(%.1f,%.1f) clip=(%.0f,%.0f)-(%.0f,%.0f)"
                     " -> %s anchor=(%.1f,%.1f) tex=0x%08x %ux%u fmt=%u world_window=%d previous=%d\n",
                     now_ms, gate_on ? "on" : "off", menu_target ? " menu" : "", depth_tested ? 1 : 0, draw.x0,
                     draw.y0, draw.x1, draw.y1, scissor.x0, scissor.y0, scissor.x1, scissor.y1,
                     shrink.apply ? "shrunk" : "kept", shrink.anchor_x, shrink.anchor_y,
                     static_cast<unsigned>(texture.address), static_cast<unsigned>(texture.width),
                     static_cast<unsigned>(texture.height), static_cast<unsigned>(texture.format), world_now(now_ms),
                     world.previous);
    }
    if (summary.start_ms < 0) summary.start_ms = now_ms;
    if (now_ms - summary.start_ms >= 1000) {
        if (summaries++ < kMaxSummaries)
            std::fprintf(stderr, "[hud-diag] t=%lldms bottom shrunk=%d kept=%d gate=%s world_windows=%d..%d\n",
                         summary.start_ms, summary.shrunk, summary.kept, gate_on ? "on" : "off",
                         world.second_min == INT_MAX ? 0 : world.second_min, world.second_max);
        summary = Summary{now_ms, 0, 0};
    }
    ++(shrink.apply ? summary.shrunk : summary.kept);
}

} // namespace vcsnx
