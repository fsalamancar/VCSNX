#pragma once
#include "hud_scale.hpp"

#include <cstdint>

namespace vcsnx {

// VCSNX_HUD_DIAG=1 (vcsnx_env.txt): where the game draws its bottom texts (subtitles, zone and vehicle names)
// and what the shrink does with each, for the subtitles that keep the game's size in some missions and the
// zone name's height (author, 2026-10-13). Off by default. All of it runs on the GE thread.
[[nodiscard]] bool hud_diag_on() noexcept;
// A depth-writing world draw, the gate's input: counted per 250 ms window like vcsnx::HudGate does.
void hud_diag_world_draw(long long now_ms) noexcept;
// The gate turned on or off.
void hud_diag_gate(long long now_ms, bool on) noexcept;
// One interface draw in the lower two thirds of the frame: its bounds and clip, whether the gate was on, whether it
// went to the pause menu's target, and the shrink it got. Each new box is logged once, and once a second a
// summary of how many bottom draws were shrunk and how many kept the game's size.
void hud_diag_draw(long long now_ms, const HudBox &draw, const HudBox &scissor, float frame_width,
                   float frame_height, bool depth_tested, bool gate_on, bool menu_target,
                   const HudShrink &shrink, const HudTexture &texture = {}) noexcept;

} // namespace vcsnx
