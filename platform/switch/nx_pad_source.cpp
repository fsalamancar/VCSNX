// Switch controller source: libnx pad (handheld + player 1), read on the main thread.
#include "pad_source.hpp"

#include <switch.h>

#include <algorithm>
#include <mutex>

namespace vcsnx {
namespace {

PadState g_pad;
std::mutex g_mutex;
PadSample g_latest{};

std::int16_t to_axis(s32 value) {
    return static_cast<std::int16_t>(std::clamp<s32>(value, -32768, 32767));
}

} // namespace

void pad_source_start() {
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&g_pad);
}

void pad_source_pump() {
    padUpdate(&g_pad);
    PadSample sample{};
    sample.connected = padIsConnected(&g_pad);
    const u64 held = padGetButtons(&g_pad);
    const auto map = [&sample, held](u64 nx_button, std::uint32_t pad_button) {
        if ((held & nx_button) != 0u) sample.buttons |= pad_button;
    };
    map(HidNpadButton_A, kPadA);
    map(HidNpadButton_B, kPadB);
    map(HidNpadButton_X, kPadX);
    map(HidNpadButton_Y, kPadY);
    map(HidNpadButton_L, kPadL);
    map(HidNpadButton_R, kPadR);
    map(HidNpadButton_ZL, kPadZL);
    map(HidNpadButton_ZR, kPadZR);
    map(HidNpadButton_Plus, kPadPlus);
    map(HidNpadButton_Minus, kPadMinus);
    map(HidNpadButton_Up, kPadUp);
    map(HidNpadButton_Down, kPadDown);
    map(HidNpadButton_Left, kPadLeft);
    map(HidNpadButton_Right, kPadRight);
    map(HidNpadButton_StickL, kPadStickL);
    map(HidNpadButton_StickR, kPadStickR);
    const HidAnalogStickState left = padGetStickPos(&g_pad, 0);
    const HidAnalogStickState right = padGetStickPos(&g_pad, 1);
    sample.left_x = to_axis(left.x);
    sample.left_y = to_axis(-left.y); // libnx Y grows upward; PadSample grows downward
    sample.right_x = to_axis(right.x);
    sample.right_y = to_axis(-right.y);
    std::lock_guard<std::mutex> guard(g_mutex);
    g_latest = sample;
}

PadSample pad_source_read() noexcept {
    std::lock_guard<std::mutex> guard(g_mutex);
    return g_latest;
}

void pad_source_shutdown() {}

} // namespace vcsnx
