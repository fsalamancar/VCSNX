// Settings > Controls with VCSNX's own picture: the game's PSP drawing, its label lines and its labels are hidden,
// and VCSNX's picture (the author's Switch with its own labels) fills the band they took. Boxes from the Switch's
// [hud-box] log, 2026-10-14, in the menu's 480 x 272 part of the frame.
#include "controls_screen.hpp"
#include "test_support.hpp"

#include <string>

namespace {

constexpr vcsnx::HudTexture kFont{0x08706bf0u, 256u, 256u, 4u};
constexpr vcsnx::HudTexture kPicture{0x0925f480u, 256u, 128u, 4u};
constexpr vcsnx::HudTexture kLinesLeft{0x0925d390u, 128u, 128u, 4u};
constexpr vcsnx::HudBox kPictureBox{140.0f, 110.0f, 340.0f, 210.0f};

} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"the controls picture is the 256 x 128 drawing in the middle of the page", [] {
             require(vcsnx::is_controls_picture(kPictureBox, kPicture), "the picture");
             require(!vcsnx::is_controls_picture({140.0f, 110.0f, 340.0f, 210.0f}, kLinesLeft), "another texture");
             require(!vcsnx::is_controls_picture({20.0f, 40.0f, 220.0f, 140.0f}, kPicture), "somewhere else");
         }},
        {"without VCSNX's picture in Textures the page stays the game's own", [] {
             // Players without the picture (no Textures/cc29539db154b0c7.png, or Replacement=false) would get the
             // PSP drawing stretched and no labels at all.
             vcsnx::ControlsScreen screen;
             require(!screen.takes(kPictureBox, kPicture, 1000), "the drawing keeps its place");
             require(!screen.hides({112.0f, 100.0f, 240.0f, 228.0f}, kLinesLeft, 1010), "and its label lines");
             require(!screen.hides({62.0f, 118.0f, 129.0f, 128.0f}, kFont, 1010), "and its labels");
         }},
        {"with it, the picture is taken over; other draws are not", [] {
             vcsnx::ControlsScreen screen;
             screen.set_picture_present(true);
             require(screen.takes(kPictureBox, kPicture, 1000) && screen.shown(1010), "the picture");
             require(!screen.takes({20.0f, 40.0f, 220.0f, 140.0f}, kPicture, 1000), "the same texture elsewhere");
             require(std::string(vcsnx::kControlsPictureFile) == "cc29539db154b0c7.png",
                     "the file tools/make_switch_textures.py writes");
         }},
        {"while it is shown, the game's labels and label lines on that page are hidden", [] {
             vcsnx::ControlsScreen screen;
             screen.set_picture_present(true);
             require(!screen.hides({112.0f, 100.0f, 240.0f, 228.0f}, kLinesLeft, 1000), "not before the page");
             require(screen.takes(kPictureBox, kPicture, 1000), "the page");
             require(screen.hides({112.0f, 100.0f, 240.0f, 228.0f}, kLinesLeft, 1010), "the left lines");
             require(screen.hides({240.0f, 100.0f, 368.0f, 228.0f}, kLinesLeft, 1010), "the right lines");
             require(screen.hides({62.0f, 118.0f, 129.0f, 128.0f}, kFont, 1010), "'Look/Fine Aim'");
             require(screen.hides({350.0f, 202.0f, 381.0f, 212.0f}, kFont, 1010), "'Pause'");
             require(!screen.hides({97.0f, 90.0f, 230.0f, 103.0f}, kFont, 1010), "'PLAYER MOVEMENT:' stays");
             require(!screen.hides({20.0f, 230.0f, 134.0f, 245.0f}, kFont, 1010), "the tabs stay");
             require(!screen.hides(kPictureBox, kPicture, 1010), "the picture itself stays");
             require(!screen.hides({112.0f, 100.0f, 240.0f, 228.0f}, kLinesLeft, 1000 + vcsnx::kControlsHoldMs + 1),
                     "the page is gone");
         }},
        {"VCSNX's picture fills the band between the settings and the tabs", [] {
             const vcsnx::HudBox panel = vcsnx::controls_panel();
             require(panel.y0 > 103.0f && panel.y1 < 228.0f && panel.x0 < 20.0f && panel.x1 > 460.0f,
                     "y " + std::to_string(panel.y0) + ".." + std::to_string(panel.y1));
             const auto [x0, y0] = vcsnx::controls_panel_point(kPictureBox, 140.0f, 110.0f);
             const auto [x1, y1] = vcsnx::controls_panel_point(kPictureBox, 340.0f, 210.0f);
             require(x0 == panel.x0 && y0 == panel.y0 && x1 == panel.x1 && y1 == panel.y1, "corner to corner");
         }},
    });
}
