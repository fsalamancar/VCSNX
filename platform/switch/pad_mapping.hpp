#pragma once
// Controller reading -> PSP input (spec §5 control table). Pure: no SDL, no libnx.

#include "display_window.hpp" // vcs::HostInputState

#include <cstdint>

namespace vcsnx {

// Buttons by their Nintendo label (A is the right face button, B the bottom one).
inline constexpr std::uint32_t kPadA = 1u << 0;
inline constexpr std::uint32_t kPadB = 1u << 1;
inline constexpr std::uint32_t kPadX = 1u << 2;
inline constexpr std::uint32_t kPadY = 1u << 3;
inline constexpr std::uint32_t kPadL = 1u << 4;
inline constexpr std::uint32_t kPadR = 1u << 5;
inline constexpr std::uint32_t kPadZL = 1u << 6;
inline constexpr std::uint32_t kPadZR = 1u << 7;
inline constexpr std::uint32_t kPadPlus = 1u << 8;
inline constexpr std::uint32_t kPadMinus = 1u << 9;
inline constexpr std::uint32_t kPadUp = 1u << 10;
inline constexpr std::uint32_t kPadDown = 1u << 11;
inline constexpr std::uint32_t kPadLeft = 1u << 12;
inline constexpr std::uint32_t kPadRight = 1u << 13;
inline constexpr std::uint32_t kPadStickL = 1u << 14;  // left stick click (L3)
inline constexpr std::uint32_t kPadStickR = 1u << 15;  // right stick click (R3)

// PSP pad bits as read through sceCtrl (same values as the vendor platform layers).
inline constexpr std::uint32_t kPspSelect = 0x000001u;
inline constexpr std::uint32_t kPspStart = 0x000008u;
inline constexpr std::uint32_t kPspUp = 0x000010u;
inline constexpr std::uint32_t kPspRight = 0x000020u;
inline constexpr std::uint32_t kPspDown = 0x000040u;
inline constexpr std::uint32_t kPspLeft = 0x000080u;
inline constexpr std::uint32_t kPspLTrigger = 0x000100u;
inline constexpr std::uint32_t kPspRTrigger = 0x000200u;
inline constexpr std::uint32_t kPspTriangle = 0x001000u;
inline constexpr std::uint32_t kPspCircle = 0x002000u;
inline constexpr std::uint32_t kPspCross = 0x004000u;
inline constexpr std::uint32_t kPspSquare = 0x008000u;

// One controller reading in platform-neutral form. Sticks are -32768..32767 with Y growing
// downward (SDL convention; the libnx source flips its upward Y before filling this in).
struct PadSample {
    std::uint32_t buttons{};
    std::int16_t left_x{};
    std::int16_t left_y{};
    std::int16_t right_x{};
    std::int16_t right_y{};
    bool connected{};
};

struct PadMappingOptions {
    bool invert_camera_y{};
    bool driving{}; // vcs::vcs_player_in_vehicle()
};

// Nintendo-style face buttons (A confirms = cross, B = circle, X = triangle, Y = square) plus the
// fork's trigger behaviour: on foot ZL/ZR repeat L/R; in a vehicle they do not (aiming suppresses
// the throttle), L/R look left/right (the game's L held + stick pushed that way) and the right
// stick click looks back (L+R); throttle/brake follow ZR/ZL. The right stick always moves the camera.
[[nodiscard]] vcs::HostInputState map_pad_to_psp(const PadSample &pad,
                                                 const PadMappingOptions &options) noexcept;

} // namespace vcsnx
