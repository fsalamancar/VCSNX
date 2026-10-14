#pragma once
#include "hud_scale.hpp"

#include <atomic>
#include <string_view>
#include <utility>

namespace vcsnx {

// Settings > Controls with VCSNX's own picture (the author, 2026-10-14: "no puedes dejar en blanco todo lo de atrás?
// que no se vea el psp ni rótulos. y lo ponemos lo nuestro bien y ordenado"). The game draws there a 256 x 128 PSP
// drawing (replaced by the author's Switch with VCSNX's labels, Textures/cc29539db154b0c7.png), two 128 x 128
// overlays with its label lines, and its labels in two columns. While the picture is shown the overlays and labels
// are not drawn, and the picture fills the band they took. Coordinates in the menu's 480 x 272 part of the frame
// ([hud-box], Switch, 2026-10-14).

inline constexpr long long kControlsHoldMs = 200;  // the menu is drawn every 4 vblanks
// VCSNX's picture in the replacements folder (tools/make_switch_textures.py, CONTROLS_PSP). Without it, or with
// [Textures] Replacement=false, the page stays the game's own: hiding its labels would leave the PSP drawing bare.
inline constexpr std::string_view kControlsPictureFile = "cc29539db154b0c7.png";

// Whether a draw is the controls picture: the 256 x 128 4-bit drawing at (140,110)-(340,210).
[[nodiscard]] bool is_controls_picture(const HudBox &box, const HudTexture &texture) noexcept;
// Where VCSNX's picture goes: the band between the settings above ("PLAYER MOVEMENT", ending at y 103) and the
// tabs below.
[[nodiscard]] HudBox controls_panel() noexcept;
// Where a point of the picture's own box lands in the panel.
[[nodiscard]] std::pair<float, float> controls_panel_point(const HudBox &picture, float x, float y) noexcept;

class ControlsScreen {
public:
    // At startup: whether VCSNX's picture replaces the game's drawing (kControlsPictureFile).
    void set_picture_present(bool present) noexcept { present_.store(present, std::memory_order_relaxed); }
    // The GE thread: whether a draw is the controls picture and VCSNX's picture replaces it; if so the page is
    // shown, and the caller moves the draw into controls_panel().
    [[nodiscard]] bool takes(const HudBox &box, const HudTexture &texture, long long now_ms) noexcept;
    [[nodiscard]] bool shown(long long now_ms) const noexcept {
        return now_ms - last_picture_ms_.load(std::memory_order_relaxed) <= kControlsHoldMs;
    }
    // Whether a draw is one of the game's labels or label lines on that page (its text, inside the band), hidden
    // while the page is shown.
    [[nodiscard]] bool hides(const HudBox &box, const HudTexture &texture, long long now_ms) const noexcept;

private:
    std::atomic<bool> present_{false};
    std::atomic<long long> last_picture_ms_{-1'000'000};
};

ControlsScreen &controls_screen() noexcept;

} // namespace vcsnx
