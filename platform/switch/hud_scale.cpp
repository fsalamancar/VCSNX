#include "hud_scale.hpp"

#include <algorithm>
#include <cmath>

namespace vcsnx {
namespace {

// The radar sits in the bottom-left corner of the frame (VCS: x 8..87, y 225..312 of 512x320).
constexpr float kRadarRight = 0.2f;   // of the frame width
constexpr float kRadarTop = 0.66f;    // of the frame height
// The help text box sits in the top band, starting in the left half (VCS: x 6..245, y 3..61). The
// user wants it at the game's own size; part of it can fall in the middle third, so the whole band
// is left alone rather than shrunk toward two different anchors.
constexpr float kHelpBoxBottom = 0.30f;  // of the frame height
// VRAM offset the pause menu renders into (Mac [world-diag]: ~1,100 2D draws/s while it is open,
// none during play).
constexpr std::uint32_t kFrontendTarget = 0x110000u;
// A scissor narrower or shorter than this share of the frame clips the draw to a widget. In the
// radar's corner that is its map tiles, which stay put; elsewhere (the ammo counter under the weapon
// icon) the draw shrinks and its scissor moves with it (hud_shrunk_box).
constexpr float kScissoredShare = 0.6f;

float anchor_for(float centre, float extent) noexcept {
    if (centre < extent / 3.0f) return 0.0f;
    if (centre > extent * 2.0f / 3.0f) return extent;
    return extent / 2.0f;
}

} // namespace

HudShrink hud_shrink_for(const HudBox &draw, const HudBox &scissor, float frame_width, float frame_height,
                         float scale, bool depth_tested) noexcept {
    const HudShrink none{false, 0.0f, 0.0f, 1.0f};
    if (!(scale > 0.0f && scale < 1.0f)) return none;
    // A draw clipped to a widget (ammo counter, pager screen, radar tiles) belongs where its clip is,
    // not where its geometry runs: the pager's message is far longer than the pager.
    const bool scissored = scissor.x1 - scissor.x0 < kScissoredShare * frame_width ||
                           scissor.y1 - scissor.y0 < kScissoredShare * frame_height;
    const HudBox &widget = scissored ? scissor : draw;
    if (widget.x1 <= kRadarRight * frame_width && widget.y0 >= kRadarTop * frame_height) return none;
    if (draw.y1 <= kHelpBoxBottom * frame_height && draw.x0 < frame_width * 0.5f) return none;
    const float ax = anchor_for((widget.x0 + widget.x1) * 0.5f, frame_width);
    const float ay = anchor_for((widget.y0 + widget.y1) * 0.5f, frame_height);
    // Only the clusters that are too big shrink: top right (clock, money, health, weapon, ammo), bottom
    // right (zone and vehicle names) and bottom centre (subtitles). The rest keeps the game's layout.
    const bool top_right = ax == frame_width && ay == 0.0f;
    const bool bottom_right = ax == frame_width && ay == frame_height;
    const bool bottom_centre = ax == frame_width * 0.5f && ay == frame_height;
    if (!top_right && !bottom_right && !bottom_centre) return none;
    // In the top right the HUD (clock, money, bars, weapon, ammo) is drawn without a depth test; the big
    // script texts there ("Loading...", the radio name) with one, and those keep the game's size.
    if (top_right && depth_tested) return none;
    return {true, ax, ay, scale};
}

HudBox hud_shrunk_box(const HudBox &box, const HudShrink &shrink) noexcept {
    return {std::floor(hud_shrunk(box.x0, shrink.anchor_x, shrink.scale)),
            std::floor(hud_shrunk(box.y0, shrink.anchor_y, shrink.scale)),
            std::ceil(hud_shrunk(box.x1, shrink.anchor_x, shrink.scale)),
            std::ceil(hud_shrunk(box.y1, shrink.anchor_y, shrink.scale))};
}

bool is_frontend_target(std::uint32_t target_address) noexcept {
    return (target_address & 0x001FFFF0u) == kFrontendTarget;
}


void WorldActivity::note_world_draw(long long now_ms) noexcept {
    if (now_ms - window_start_ms_ >= kWorldActivityWindowMs) {
        previous_ = now_ms - window_start_ms_ < 2 * kWorldActivityWindowMs ? current_ : 0;
        current_ = 0;
        window_start_ms_ = now_ms;
    }
    ++current_;
}

bool WorldActivity::active(long long now_ms) const noexcept {
    if (now_ms - window_start_ms_ >= 2 * kWorldActivityWindowMs) return false;
    return std::max(current_, previous_) >= kWorldDrawsForPlay;
}

bool hud_draw_shrinkable(bool shrink_on, std::uint32_t target_address) noexcept {
    return shrink_on && !is_frontend_target(target_address);
}

void HudGate::note_world_draw(long long now_ms) noexcept {
    world_.note_world_draw(now_ms);
    last_world_draw_ms_ = now_ms;
    if (world_.active(now_ms)) on_ = true;
}

void HudGate::note_menu_draw(long long now_ms) noexcept { last_menu_draw_ms_ = now_ms; }

bool HudGate::on(long long now_ms) noexcept {
    // The game is still under way while its pause menu is drawn, even with no world behind it.
    const long long last_game_draw_ms = std::max(last_world_draw_ms_, last_menu_draw_ms_);
    if (on_ && now_ms - last_game_draw_ms >= kHudOffAfterMs) on_ = false;
    const bool menu_open = now_ms - last_menu_draw_ms_ < kMenuHoldMs;
    return on_ && !menu_open;
}

} // namespace vcsnx
