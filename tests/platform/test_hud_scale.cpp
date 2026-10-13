// The HUD shrinks toward the edge or corner each element belongs to, leaves the radar alone, and
// only applies while the world is drawn (never in menus). Boxes are from a Mac run's [hud-draw]
// log in VCS's 512x320 logical frame.
#include "hud_scale.hpp"
#include "test_support.hpp"

#include <cmath>
#include <string>

namespace {

constexpr float kW = 512.0f;
constexpr float kH = 320.0f;
constexpr vcsnx::HudBox kFullScissor{0.0f, 0.0f, 512.0f, 320.0f};

void require_anchor(const vcsnx::HudBox &draw, float ax, float ay, const char *what) {
    const vcsnx::HudShrink s = vcsnx::hud_shrink_for(draw, kFullScissor, kW, kH, 0.75f);
    vcsnx_test::require(s.apply, std::string(what) + ": should shrink");
    vcsnx_test::require(s.anchor_x == ax && s.anchor_y == ay && s.scale == 0.75f,
                        std::string(what) + ": anchor (" + std::to_string(s.anchor_x) + "," +
                            std::to_string(s.anchor_y) + ") scale " + std::to_string(s.scale));
}

} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"the help text box at the top left keeps its size (the user's call)", [] {
             const auto text = vcsnx::hud_shrink_for({9.0f, 5.0f, 224.0f, 57.0f}, kFullScissor, kW, kH, 0.75f);
             const auto box = vcsnx::hud_shrink_for({6.0f, 3.0f, 245.0f, 61.0f}, kFullScissor, kW, kH, 0.75f);
             const auto box_right_piece = vcsnx::hud_shrink_for({180.0f, 3.0f, 245.0f, 61.0f}, kFullScissor, kW, kH, 0.75f);
             require(!text.apply && !box.apply && !box_right_piece.apply, "help box and its text untouched");
         }},
        {"depth-tested text in the top right keeps its size (\"Loading...\", the radio name)", [] {
             // Mac [ammo] log: the clock, money, bars, weapon and ammo there are drawn without a depth
             // test; the big script texts there with one. The user wants "Loading..." at its own size.
             require(!vcsnx::hud_shrink_for({410.0f, 5.0f, 509.0f, 41.0f}, kFullScissor, kW, kH, 0.75f, true).apply,
                     "Loading... stays");
             require(vcsnx::hud_shrink_for({442.0f, 70.0f, 455.0f, 89.0f}, kFullScissor, kW, kH, 0.75f, false).apply,
                     "a money digit still shrinks");
         }},
        {"a money digit shrinks into the top-right corner", [] {
             require_anchor({442.0f, 70.0f, 455.0f, 89.0f}, kW, 0.0f, "money digit");
         }},
        {"the zone name shrinks into the bottom-right corner", [] {
             require_anchor({260.0f, 277.0f, 514.0f, 320.0f}, kW, kH, "zone name");
         }},
        {"a subtitle shrinks toward the bottom centre", [] {
             require_anchor({130.0f, 290.0f, 380.0f, 310.0f}, kW / 2, kH, "subtitle");
         }},
        {"a centred message keeps its size", [] {
             require(!vcsnx::hud_shrink_for({180.0f, 140.0f, 330.0f, 170.0f}, kFullScissor, kW, kH, 0.75f).apply,
                     "only the top-right, bottom-right and bottom-centre clusters shrink");
         }},
        {"the pager's scrolling message stays inside the pager", [] {
             // Mac frames: the message is longer than the pager screen and clipped to it; its anchor must
             // come from that clip (top left, left alone), not from the long text.
             require(!vcsnx::hud_shrink_for({300.0f, 30.0f, 750.0f, 45.0f}, {30.0f, 28.0f, 140.0f, 44.0f},
                                            kW, kH, 0.75f).apply,
                     "pager text keeps its place");
         }},
        {"the radar's own corner is left alone", [] {
             const auto ring = vcsnx::hud_shrink_for({8.0f, 225.0f, 87.0f, 312.0f}, kFullScissor, kW, kH, 0.75f);
             const auto blip = vcsnx::hud_shrink_for({35.0f, 228.0f, 41.0f, 235.0f}, kFullScissor, kW, kH, 0.75f);
             require(!ring.apply && !blip.apply, "radar ring and blips keep their size");
         }},
        {"the ammo counter, clipped to its own scissor, shrinks with the money", [] {
             // From the Mac [ammo] log: "0-17" is drawn piece by piece, each clipped under the weapon icon.
             const auto ammo = vcsnx::hud_shrink_for({472.0f, 55.0f, 479.0f, 66.0f}, {453.0f, 52.0f, 480.0f, 70.0f},
                                                     kW, kH, 0.75f);
             require(ammo.apply && ammo.anchor_x == kW && ammo.anchor_y == 0.0f,
                     "ammo shrinks toward the top-right corner like the money under it");
         }},
        {"a shrunk box moves with its draw, rounded outwards", [] {
             const vcsnx::HudShrink s{true, kW, 0.0f, 0.75f};
             const vcsnx::HudBox clip = vcsnx::hud_shrunk_box({453.0f, 52.0f, 480.0f, 70.0f}, s);
             require(clip.x0 == 467.0f && clip.y0 == 39.0f && clip.x1 == 488.0f && clip.y1 == 53.0f,
                     "clip (" + std::to_string(clip.x0) + "," + std::to_string(clip.y0) + ")-(" + std::to_string(clip.x1) +
                         "," + std::to_string(clip.y1) + ")");
         }},
        {"scissored draws (the radar map tiles) are left alone", [] {
             const auto tile = vcsnx::hud_shrink_for({-74.0f, 247.0f, 145.0f, 490.0f}, {8.0f, 225.0f, 87.0f, 312.0f},
                                                     kW, kH, 0.75f);
             require(!tile.apply, "a draw clipped to a small scissor is not moved");
         }},
        {"scale 1 changes nothing", [] {
             require(!vcsnx::hud_shrink_for({9.0f, 5.0f, 224.0f, 57.0f}, kFullScissor, kW, kH, 1.0f).apply, "scale 1");
         }},
        {"shrinking moves a point toward its anchor", [] {
             require(std::fabs(vcsnx::hud_shrunk(442.0f, kW, 0.75f) - 459.5f) < 1e-4f, "442 -> 459.5");
             require(vcsnx::hud_shrunk(0.0f, 0.0f, 0.75f) == 0.0f, "the anchor stays put");
         }},
        {"the pause menu's own render target is recognised", [] {
             // Mac [world-diag] run: with the pause menu open the game draws ~1,100 2D draws/s into
             // 0x110000 and keeps drawing the world behind it; during play that target is never used.
             require(vcsnx::is_frontend_target(0x110000u), "menu target");
             require(!vcsnx::is_frontend_target(0x088000u), "world target");
             require(!vcsnx::is_frontend_target(0x000000u) && !vcsnx::is_frontend_target(0x044000u), "display buffers");
             require(!vcsnx::is_frontend_target(0x178000u), "copy target");
         }},
        {"draws into the pause menu's target are never shrunk", [] {
             require(vcsnx::hud_draw_shrinkable(true, 0x088000u), "a HUD draw while playing");
             require(!vcsnx::hud_draw_shrinkable(true, 0x110000u), "a pause-menu draw, the world still drawn behind");
             require(!vcsnx::hud_draw_shrinkable(false, 0x088000u), "loading screen, title menu");
         }},
        {"the shrink turns on when the world is drawn at a gameplay rate", [] {
             vcsnx::HudGate gate;
             require(!gate.on(1000), "nothing drawn yet: title menu");
             for (int i = 0; i < 100; ++i) gate.note_world_draw(1000);
             require(!gate.on(1100), "a trickle of world draws (the end of a load) does not turn it on");
             for (int i = 0; i < vcsnx::kWorldDrawsForPlay; ++i) gate.note_world_draw(1200);
             require(gate.on(1300), "playing");
         }},
        {"the shrink stays on through dips in the world's draw rate", [] {
             // Run 15b [hud-diag], Switch: 1,123-1,172 world draws per window, once only 34, while playing.
             vcsnx::HudGate gate;
             for (int i = 0; i < vcsnx::kWorldDrawsForPlay; ++i) gate.note_world_draw(1000);
             require(gate.on(1000), "playing");
             for (long long t = 1000; t <= 4000; t += vcsnx::kWorldActivityWindowMs) {
                 for (int i = 0; i < 34; ++i) gate.note_world_draw(t);
                 require(gate.on(t + 100), "a slow stretch keeps the HUD's size: " + std::to_string(t));
             }
         }},
        {"the shrink stays on through a short pause of the world", [] {
             // Run 15b [hud-diag]: about 1.7 s without a world draw while playing (91 s).
             vcsnx::HudGate gate;
             for (int i = 0; i < vcsnx::kWorldDrawsForPlay; ++i) gate.note_world_draw(1000);
             require(gate.on(1000), "playing");
             require(gate.on(2700), "1.7 s without the world");
             require(gate.on(1000 + vcsnx::kHudOffAfterMs - 1), "just under the limit");
         }},
        {"opening the pause menu turns the shrink off at once", [] {
             // Run 16, Switch: a bottom-right element flashed in its shrunk place for the menu's first 3 s.
             vcsnx::HudGate gate;
             for (int i = 0; i < vcsnx::kWorldDrawsForPlay; ++i) gate.note_world_draw(1000);
             require(gate.on(1000), "playing");
             gate.note_menu_draw(1100);
             require(!gate.on(1100), "the menu's first draw");
             gate.note_world_draw(1200);
             require(!gate.on(1200), "world drawn behind the menu");
             gate.note_menu_draw(5000);
             require(!gate.on(5000), "a long stay in the menu");
         }},
        {"closing the pause menu brings the shrink straight back", [] {
             // The Switch stops drawing the world behind the menu, for longer than kHudOffAfterMs.
             vcsnx::HudGate gate;
             for (int i = 0; i < vcsnx::kWorldDrawsForPlay; ++i) gate.note_world_draw(1000);
             for (long long t = 1100; t <= 9000; t += 33) gate.note_menu_draw(t);
             require(!gate.on(9000), "still in the menu");
             require(gate.on(9000 + vcsnx::kMenuHoldMs), "the menu is closed");
         }},
        {"the shrink turns off once the world is gone and on again when it returns", [] {
             vcsnx::HudGate gate;
             for (int i = 0; i < vcsnx::kWorldDrawsForPlay; ++i) gate.note_world_draw(1000);
             require(gate.on(1000), "playing");
             require(!gate.on(1000 + vcsnx::kHudOffAfterMs), "a loading screen");
             for (int i = 0; i < vcsnx::kWorldDrawsForPlay; ++i) gate.note_world_draw(9000);
             require(gate.on(9000), "playing again");
         }},
        {"the world counts as active only while it is drawn at a gameplay rate", [] {
             vcsnx::WorldActivity world;
             require(!world.active(1000), "no world draw yet");
             for (int i = 0; i < 100; ++i) world.note_world_draw(1000);
             require(!world.active(1100), "a trickle of world draws (menu, end of a load) is not play");
             for (int i = 0; i < vcsnx::kWorldDrawsForPlay; ++i) world.note_world_draw(1200);
             require(world.active(1300), "thousands of world draws: playing");
             require(!world.active(1200 + 2 * vcsnx::kWorldActivityWindowMs + 1), "world gone");
         }},
        {"a menu counts as shown while its target is drawn", [] {
             vcsnx::MenuShown menu;
             require(!menu.shown(1000), "no menu yet");
             menu.note_menu_draw(1000);
             require(menu.shown(1000), "just drawn");
             // In the pause menu the game draws its target every 4 vblanks (Mac [present-diag]).
             require(menu.shown(1000 + vcsnx::kMenuShownMs - 1), "between two menu frames");
             require(!menu.shown(1000 + vcsnx::kMenuShownMs + 1), "menu closed");
         }},
        {"the menu is gone from the first world draw after it", [] {
             // User, Mac window: "al cerrar el menu el hud del dinero y armas se agranda y se achica".
             vcsnx::MenuShown menu;
             menu.note_menu_draw(1000);
             menu.note_world_draw();
             require(!menu.shown(1033), "the first gameplay frame is shown whole");
             menu.note_menu_draw(1066);
             require(menu.shown(1066), "the menu again");
         }},
    });
}
