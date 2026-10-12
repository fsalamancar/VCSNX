#include "pad_mapping.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace vcsnx {
namespace {

// Same dead zone and scaling as the fork's SDL layer (platform/macos/display_window_sdl.cpp).
constexpr int kDeadZone = 7849;

float axis_with_deadzone(std::int16_t raw) noexcept {
    int magnitude = std::abs(static_cast<int>(raw));
    if (magnitude <= kDeadZone) return 0.0f;
    magnitude = (magnitude - kDeadZone) * 32767 / (32767 - kDeadZone);
    return static_cast<float>(raw < 0 ? -magnitude : magnitude) / 32767.0f;
}

std::uint8_t stick_to_psp(float normalized) noexcept {
    return static_cast<std::uint8_t>(
        std::clamp(128 + static_cast<int>(std::lround(normalized * 127.0f)), 0, 255));
}

int camera_axis(float normalized) noexcept {
    return std::clamp(static_cast<int>(std::lround(normalized * 127.0f)), -127, 127);
}

} // namespace

vcs::HostInputState map_pad_to_psp(const PadSample &pad, const PadMappingOptions &options) noexcept {
    vcs::HostInputState input{};
    if (!pad.connected) return input;
    const auto held = [&pad](std::uint32_t button) { return (pad.buttons & button) != 0u; };

    // Nintendo's convention: A confirms (the PSP's cross), B goes back (circle). X and Y keep the
    // PSP's places: X on top is triangle, Y on the left is square.
    if (held(kPadA)) input.buttons |= kPspCross;
    if (held(kPadB)) input.buttons |= kPspCircle;
    if (held(kPadY)) input.buttons |= kPspSquare;
    if (held(kPadX)) input.buttons |= kPspTriangle;
    if (held(kPadL)) input.buttons |= kPspLTrigger;
    if (held(kPadR)) input.buttons |= kPspRTrigger;
    if (held(kPadPlus)) input.buttons |= kPspStart;
    if (held(kPadMinus)) input.buttons |= kPspSelect;
    if (held(kPadUp)) input.buttons |= kPspUp;
    if (held(kPadDown)) input.buttons |= kPspDown;
    if (held(kPadLeft)) input.buttons |= kPspLeft;
    if (held(kPadRight)) input.buttons |= kPspRight;

    if (!options.driving) {
        if (held(kPadZL)) input.buttons |= kPspLTrigger;
        if (held(kPadZR)) input.buttons |= kPspRTrigger;
    }
    input.accelerate = held(kPadZR);
    input.brake = held(kPadZL);

    const float left_x = axis_with_deadzone(pad.left_x);
    const float left_y = axis_with_deadzone(pad.left_y);
    if (left_x != 0.0f || left_y != 0.0f) {
        input.analog_x = stick_to_psp(left_x);
        input.analog_y = stick_to_psp(left_y); // pad and PSP both grow downward
    }
    // In a vehicle the game looks sideways with L held and the stick pushed that way, and back with
    // L+R. On the Switch: L looks left, R looks right, R3 (or L and R together) looks back; the right
    // stick stays free for the camera.
    if (options.driving) {
        const bool look_left = held(kPadL);
        const bool look_right = held(kPadR);
        if (held(kPadStickR) || (look_left && look_right)) {
            input.buttons |= kPspLTrigger | kPspRTrigger;
        } else if (look_left || look_right) {
            input.buttons = (input.buttons & ~kPspRTrigger) | kPspLTrigger;
            input.analog_x = look_left ? 0u : 255u;
        }
    }
    float camera_y = -axis_with_deadzone(pad.right_y); // the camera axis grows upward
    if (options.invert_camera_y) camera_y = -camera_y;
    input.camera_x = camera_axis(axis_with_deadzone(pad.right_x));
    input.camera_y = camera_axis(camera_y);
    return input;
}

} // namespace vcsnx
