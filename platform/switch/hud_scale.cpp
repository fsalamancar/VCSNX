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
// A bottom draw whose own geometry reaches this far right, and is not centred, is right-aligned text (zone and
// vehicle names), however long: "'101 Bayshore Avenue'" has its centre in the middle third and shrank toward
// the bottom centre with the subtitles, 65 units from the edge (photo, 2026-10-13). Not the clip's: cutscene
// subtitles are clipped to the letterbox bar, edge to edge, and went to the corner too ([hud-box], 2026-10-14).
constexpr float kRightAlignedShare = 0.95f;
constexpr float kCentredShare = 0.05f;  // of the frame width, around its middle
// The user wants the zone name "pegado a la esquina ... como el HUD de arriba": in the game's 512x320
// frame its text ends at x 511 and y 297.6, the top-right HUD 20 units from the right edge and 6.5 from
// the top (same photo). So the bottom-right cluster moves by this much before it shrinks.
constexpr float kBottomRightShiftX = -19.0f / 512.0f;  // of the frame width
constexpr float kBottomRightShiftY = 15.9f / 320.0f;   // of the frame height
// The zone name goes on the vehicle name's line ("en el mismo lugar que el de los autos, es decir un poco
// arriba", 2026-10-14): [hud-box] put the vehicle name's box at y 249-291 and the zone name's at 277-320, both
// right-aligned at the same x. A right-aligned draw starting below kZoneLineTop is on the zone name's line.
constexpr float kZoneLineTop = 262.0f / 320.0f;          // of the frame height
constexpr float kZoneToVehicleLine = -28.0f / 320.0f;    // of the frame height

float anchor_for(float centre, float extent) noexcept {
    if (centre < extent / 3.0f) return 0.0f;
    if (centre > extent * 2.0f / 3.0f) return extent;
    return extent / 2.0f;
}

} // namespace

HudShrink hud_shrink_for(const HudBox &draw, const HudBox &scissor, float frame_width, float frame_height,
                         float scale, bool depth_tested, bool text) noexcept {
    const HudShrink none{false, 0.0f, 0.0f, 1.0f};
    if (!(scale > 0.0f && scale < 1.0f)) return none;
    // A draw clipped to a widget (ammo counter, pager screen, radar tiles) belongs where its clip is,
    // not where its geometry runs: the pager's message is far longer than the pager.
    const bool scissored = scissor.x1 - scissor.x0 < kScissoredShare * frame_width ||
                           scissor.y1 - scissor.y0 < kScissoredShare * frame_height;
    const HudBox &widget = scissored ? scissor : draw;
    if (widget.x1 <= kRadarRight * frame_width && widget.y0 >= kRadarTop * frame_height) return none;
    if (draw.y1 <= kHelpBoxBottom * frame_height && draw.x0 < frame_width * 0.5f) return none;
    const float ay = anchor_for((widget.y0 + widget.y1) * 0.5f, frame_height);
    const bool right_aligned = draw.x1 >= kRightAlignedShare * frame_width &&
                               std::fabs((draw.x0 + draw.x1) * 0.5f - frame_width * 0.5f) > kCentredShare * frame_width;
    const float ax = ay == frame_height && right_aligned ? frame_width
                                                         : anchor_for((widget.x0 + widget.x1) * 0.5f, frame_width);
    // Only the clusters that are too big shrink: top right (clock, money, health, weapon, ammo), bottom
    // right (zone and vehicle names), bottom centre (subtitles) and the text in the middle (some cutscenes'
    // subtitles, "en el medio", 2026-10-14). The rest keeps the game's layout.
    const bool top_right = ax == frame_width && ay == 0.0f;
    const bool bottom_right = ax == frame_width && ay == frame_height;
    const bool bottom_centre = ax == frame_width * 0.5f && ay == frame_height;
    const bool middle_text = text && ax == frame_width * 0.5f && ay == frame_height * 0.5f;
    if (!top_right && !bottom_right && !bottom_centre && !middle_text) return none;
    // In the top right the HUD (clock, money, bars, weapon, ammo) is drawn without a depth test; the big
    // script texts there ("Loading...", the radio name) with one, and those keep the game's size.
    if (top_right && depth_tested) return none;
    if (bottom_right) {
        // Moving by d and then shrinking toward a is shrinking toward a + d * scale / (1 - scale).
        const float k = scale / (1.0f - scale);
        const float dy = kBottomRightShiftY + (draw.y0 > kZoneLineTop * frame_height ? kZoneToVehicleLine : 0.0f);
        return {true, ax + kBottomRightShiftX * frame_width * k, ay + dy * frame_height * k, scale};
    }
    return {true, ax, ay, scale};
}

bool hud_is_text(const HudTexture &texture) noexcept {
    constexpr std::uint32_t kClut4 = 4u;
    constexpr std::uint32_t kFontMinWidth = 128u;
    return texture.address != 0u && texture.format == kClut4 && texture.width >= kFontMinWidth;
}

bool hud_shrink_candidate(bool filled_primitive, bool clear_mode, bool full_width, bool not_world_effect,
                          bool text) noexcept {
    return filled_primitive && !clear_mode && (!full_width || text) && not_world_effect;
}

HudBox hud_shrunk_box(const HudBox &box, const HudShrink &shrink) noexcept {
    return {std::floor(hud_shrunk(box.x0, shrink.anchor_x, shrink.scale)),
            std::floor(hud_shrunk(box.y0, shrink.anchor_y, shrink.scale)),
            std::ceil(hud_shrunk(box.x1, shrink.anchor_x, shrink.scale)),
            std::ceil(hud_shrunk(box.y1, shrink.anchor_y, shrink.scale))};
}

MenuShown &menu_shown() noexcept {
    static MenuShown menu;
    return menu;
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
    // Open until the world is drawn again: the game draws no world while its pause menu is up, and the hold
    // alone kept the HUD big for the first 250 ms of play after it ("se agranda el hud al volver de la pausa").
    const bool menu_open = now_ms - last_menu_draw_ms_ < kMenuHoldMs && last_menu_draw_ms_ >= last_world_draw_ms_;
    return on_ && !menu_open;
}

} // namespace vcsnx
