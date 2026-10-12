#pragma once
#include <cstdint>

namespace vcsnx {

// A 2D draw's bounds, or its scissor rectangle, in the GE's logical frame (512x320 in VCS).
struct HudBox {
    float x0, y0, x1, y1;
};

// How one HUD draw is shrunk: every vertex moves toward (anchor_x, anchor_y) by `scale`.
struct HudShrink {
    bool apply;
    float anchor_x, anchor_y, scale;
};

// Smaller HUD (user request: text, money and zone names too big on the Switch). The too-big clusters
// shrink toward their own corner or edge, picked from the centre (or the clip) by thirds of the frame:
// top right, bottom right and bottom centre. The game draws a whole text box, a whole line or one
// digit per draw, so nothing that belongs together is split. Everything else keeps its place. The radar is
// left as it is: draws inside its corner and its map tiles, clipped to a scissor there. So is the
// help text box at the top left, which the user wants at the game's own size.
[[nodiscard]] HudShrink hud_shrink_for(const HudBox &draw, const HudBox &scissor, float frame_width,
                                       float frame_height, float scale, bool depth_tested = false) noexcept;

[[nodiscard]] inline float hud_shrunk(float value, float anchor, float scale) noexcept {
    return anchor + (value - anchor) * scale;
}

// A rectangle (a draw's scissor) after the shrink, rounded outwards to whole pixels so the clip never
// cuts the shrunk geometry it belongs to.
[[nodiscard]] HudBox hud_shrunk_box(const HudBox &box, const HudShrink &shrink) noexcept;

// The HUD shrinks while the game is played: HudGate turns it on once the world is drawn at a gameplay
// rate (the end of a load draws a little world too), and the pause menu, drawn over the world into its
// own target, is never shrunk (hud_draw_shrinkable).
inline constexpr long long kWorldActivityWindowMs = 250;
// World draws within one window that mean gameplay (Mac [world-diag]: play 7,000-35,000 depth-writing
// draws/s; the pause menu and the end of a load 0-3,300/s).
inline constexpr int kWorldDrawsForPlay = 1200;

// The pause menu is drawn into its own VRAM target (0x110000 in VCS) while the world keeps being drawn
// behind it, so "the world is drawn" alone does not mean "playing".
[[nodiscard]] bool is_frontend_target(std::uint32_t target_address) noexcept;

class WorldActivity {
public:
    void note_world_draw(long long now_ms) noexcept;
    [[nodiscard]] bool active(long long now_ms) const noexcept;

private:
    long long window_start_ms_ = -1'000'000;
    int current_ = 0;   // world draws in the window that started at window_start_ms_
    int previous_ = 0;  // in the window before it
};

// Whether one interface draw may be shrunk: the shrink is on (HudGate) and the draw does not go to
// the pause menu's own target.
[[nodiscard]] bool hud_draw_shrinkable(bool shrink_on, std::uint32_t target_address) noexcept;

// Whether the shrink is on. Run 15b [hud-diag] on the Switch: while playing, the world's draw rate sits
// around the gameplay threshold (1,100-1,300 per window, once 34) and the world is not drawn at all for
// up to ~1.7 s at mission starts; switching off at each dip made the HUD and the subtitles jump between
// sizes ("they always have to be the same size"). So the gate turns on when the world is drawn at a
// gameplay rate and only turns off after kHudOffAfterMs without a single world or menu draw (a loading
// screen). The pause menu turns it off from its first draw until kMenuHoldMs after its last (note_menu_draw;
// its target is never used while playing): in run 16 a bottom-right element drawn during the menu, outside
// that target, flashed in its shrunk place for the menu's first seconds.
inline constexpr long long kHudOffAfterMs = 3000;
inline constexpr long long kMenuHoldMs = 250;

class HudGate {
public:
    void note_world_draw(long long now_ms) noexcept;
    void note_menu_draw(long long now_ms) noexcept;
    [[nodiscard]] bool on(long long now_ms) noexcept;

private:
    WorldActivity world_;
    long long last_world_draw_ms_ = -1'000'000;
    long long last_menu_draw_ms_ = -1'000'000;
    bool on_ = false;
};

} // namespace vcsnx
