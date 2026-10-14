#include "controls_screen.hpp"

#include <cmath>

namespace vcsnx {
namespace {

constexpr HudBox kPicture{140.0f, 110.0f, 340.0f, 210.0f};
// The labels and overlays run from y 100 to 228; the settings above end at 103 and the tabs start below 228.
constexpr float kBandTop = 98.0f;
constexpr float kBandBottom = 230.0f;
constexpr HudBox kPanel{10.0f, 106.0f, 470.0f, 224.0f};
constexpr float kSlack = 2.0f;

bool near(float a, float b) noexcept { return std::fabs(a - b) <= kSlack; }

} // namespace

bool is_controls_picture(const HudBox &box, const HudTexture &texture) noexcept {
    return texture.address != 0u && texture.width == 256u && texture.height == 128u && near(box.x0, kPicture.x0) &&
           near(box.y0, kPicture.y0) && near(box.x1, kPicture.x1) && near(box.y1, kPicture.y1);
}

HudBox controls_panel() noexcept { return kPanel; }

std::pair<float, float> controls_panel_point(const HudBox &picture, float x, float y) noexcept {
    const float u = (x - picture.x0) / (picture.x1 - picture.x0);
    const float v = (y - picture.y0) / (picture.y1 - picture.y0);
    return {kPanel.x0 + u * (kPanel.x1 - kPanel.x0), kPanel.y0 + v * (kPanel.y1 - kPanel.y0)};
}

bool ControlsScreen::takes(const HudBox &box, const HudTexture &texture, long long now_ms) noexcept {
    if (!present_.load(std::memory_order_relaxed) || !is_controls_picture(box, texture)) return false;
    last_picture_ms_.store(now_ms, std::memory_order_relaxed);
    return true;
}

bool ControlsScreen::hides(const HudBox &box, const HudTexture &texture, long long now_ms) const noexcept {
    if (!shown(now_ms) || is_controls_picture(box, texture) || !hud_is_text(texture)) return false;
    return box.y0 >= kBandTop && box.y1 <= kBandBottom;
}

ControlsScreen &controls_screen() noexcept {
    static ControlsScreen screen;
    return screen;
}

} // namespace vcsnx
