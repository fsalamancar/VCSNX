#include "pad_mapping.hpp"
#include "test_support.hpp"

#include <string>

namespace {

using namespace vcsnx;
using vcsnx_test::require;

PadSample connected_with(std::uint32_t buttons) {
    PadSample pad{};
    pad.connected = true;
    pad.buttons = buttons;
    return pad;
}

void require_button(std::uint32_t pad_button, std::uint32_t psp_button, const std::string &name) {
    const auto input = map_pad_to_psp(connected_with(pad_button), {});
    require(input.buttons == psp_button, name + " maps to the wrong PSP button");
}

} // namespace

int main() {
    return vcsnx_test::run({
        {"disconnected pad is neutral", [] {
             PadSample pad{};
             pad.buttons = kPadA | kPadZR;
             pad.left_x = 32767;
             pad.connected = false;
             const auto input = map_pad_to_psp(pad, {});
             require(input.buttons == 0u && input.analog_x == 128u && input.analog_y == 128u, "buttons/stick");
             require(input.camera_x == 0 && input.camera_y == 0 && !input.accelerate && !input.brake, "camera/vehicle");
         }},
        {"A confirms and B goes back, as on Nintendo consoles", [] {
             require_button(kPadA, kPspCross, "A (confirm)");
             require_button(kPadB, kPspCircle, "B (back)");
             require_button(kPadY, kPspSquare, "Y (left)");
             require_button(kPadX, kPspTriangle, "X (top)");
         }},
        {"in a vehicle L looks left, R looks right and the right stick click looks back", [] {
             // The game looks sideways with L held and the stick pushed left or right (user, run 12).
             PadMappingOptions driving{};
             driving.driving = true;
             const auto left = map_pad_to_psp(connected_with(kPadL), driving);
             require(left.buttons == kPspLTrigger && left.analog_x == 0u, "L: L held, stick full left");
             const auto right = map_pad_to_psp(connected_with(kPadR), driving);
             require(right.buttons == kPspLTrigger && right.analog_x == 255u, "R: L held, stick full right");
             require(map_pad_to_psp(connected_with(kPadStickR), driving).buttons == (kPspLTrigger | kPspRTrigger),
                     "R3 looks back (L+R)");
             require(map_pad_to_psp(connected_with(kPadL | kPadR), driving).buttons == (kPspLTrigger | kPspRTrigger),
                     "L and R together look back");
         }},
        {"on foot L and R stay the PSP's L and R", [] {
             const auto input = map_pad_to_psp(connected_with(kPadR), {});
             require(input.buttons == kPspRTrigger && input.analog_x == 128u, "R on foot");
         }},
        {"the right stick stays free for the camera, also in a vehicle", [] {
             PadMappingOptions driving{};
             driving.driving = true;
             PadSample pad = connected_with(0u);
             pad.right_x = -30000;
             pad.right_y = 30000;
             const auto input = map_pad_to_psp(pad, driving);
             require(input.buttons == 0u && input.camera_x < 0, "tilting the stick presses nothing");
         }},
        {"on foot the right stick only moves the camera", [] {
             PadSample pad = connected_with(0u);
             pad.right_x = -30000;
             const auto input = map_pad_to_psp(pad, {});
             require(input.buttons == 0u && input.camera_x < 0, "no look buttons on foot");
         }},
        {"shoulders, plus/minus and d-pad", [] {
             require_button(kPadL, kPspLTrigger, "L");
             require_button(kPadR, kPspRTrigger, "R");
             require_button(kPadPlus, kPspStart, "+");
             require_button(kPadMinus, kPspSelect, "-");
             require_button(kPadUp, kPspUp, "up");
             require_button(kPadDown, kPspDown, "down");
             require_button(kPadLeft, kPspLeft, "left");
             require_button(kPadRight, kPspRight, "right");
         }},
        {"on foot ZL/ZR repeat L/R", [] {
             const auto input = map_pad_to_psp(connected_with(kPadZL | kPadZR), {});
             require(input.buttons == (kPspLTrigger | kPspRTrigger), "ZL/ZR on foot");
         }},
        {"in a vehicle ZR accelerates and ZL brakes without aiming", [] {
             PadMappingOptions driving{};
             driving.driving = true;
             const auto throttle = map_pad_to_psp(connected_with(kPadZR), driving);
             require(throttle.accelerate && !throttle.brake && throttle.buttons == 0u, "ZR in vehicle");
             const auto brake = map_pad_to_psp(connected_with(kPadZL), driving);
             require(brake.brake && !brake.accelerate && brake.buttons == 0u, "ZL in vehicle");
         }},
        {"left stick: dead zone, full range and partial deflection", [] {
             PadSample pad = connected_with(0u);
             pad.left_x = 7000;
             require(map_pad_to_psp(pad, {}).analog_x == 128u, "inside dead zone");
             pad.left_x = 32767;
             require(map_pad_to_psp(pad, {}).analog_x == 255u, "full right");
             pad.left_x = -32768;
             require(map_pad_to_psp(pad, {}).analog_x == 1u, "full left");
             pad.left_x = 20000;
             require(map_pad_to_psp(pad, {}).analog_x == 190u, "partial right");
             pad.left_x = 0;
             pad.left_y = 32767;
             require(map_pad_to_psp(pad, {}).analog_y == 255u, "full down (Y grows downward)");
         }},
        {"right stick drives the camera, up is positive", [] {
             PadSample pad = connected_with(0u);
             pad.right_x = 32767;
             pad.right_y = -32768;
             const auto input = map_pad_to_psp(pad, {});
             require(input.camera_x == 127 && input.camera_y == 127, "camera right/up");
             PadMappingOptions inverted{};
             inverted.invert_camera_y = true;
             require(map_pad_to_psp(pad, inverted).camera_y == -127, "inverted camera Y");
         }},
    });
}
