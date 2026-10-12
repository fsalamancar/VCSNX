// Mac test bench controller source: the first SDL game controller.
#include "pad_source.hpp"

#include <SDL.h>

#include <iostream>
#include <mutex>

namespace vcsnx {
namespace {

constexpr Sint16 kTriggerThreshold = 8192;

SDL_GameController *g_controller = nullptr; // main thread only
std::mutex g_mutex;
PadSample g_latest{};

std::uint32_t sdl_buttons(SDL_GameController *controller) {
    const auto held = [controller](SDL_GameControllerButton button) {
        return SDL_GameControllerGetButton(controller, button) != 0;
    };
    // SDL names face buttons by position (A bottom, B right, X left, Y top); PadSample uses
    // Nintendo labels, whose positions are B, A, Y, X respectively.
    std::uint32_t buttons = 0u;
    if (held(SDL_CONTROLLER_BUTTON_A)) buttons |= kPadB;
    if (held(SDL_CONTROLLER_BUTTON_B)) buttons |= kPadA;
    if (held(SDL_CONTROLLER_BUTTON_X)) buttons |= kPadY;
    if (held(SDL_CONTROLLER_BUTTON_Y)) buttons |= kPadX;
    if (held(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) buttons |= kPadL;
    if (held(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) buttons |= kPadR;
    if (held(SDL_CONTROLLER_BUTTON_START)) buttons |= kPadPlus;
    if (held(SDL_CONTROLLER_BUTTON_BACK)) buttons |= kPadMinus;
    if (held(SDL_CONTROLLER_BUTTON_DPAD_UP)) buttons |= kPadUp;
    if (held(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) buttons |= kPadDown;
    if (held(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) buttons |= kPadLeft;
    if (held(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) buttons |= kPadRight;
    if (held(SDL_CONTROLLER_BUTTON_LEFTSTICK)) buttons |= kPadStickL;
    if (held(SDL_CONTROLLER_BUTTON_RIGHTSTICK)) buttons |= kPadStickR;
    if (SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > kTriggerThreshold) buttons |= kPadZL;
    if (SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > kTriggerThreshold) buttons |= kPadZR;
    return buttons;
}

} // namespace

void pad_source_start() {
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0)
        std::cerr << "[vcsnx-pad] SDL game controller init failed: " << SDL_GetError() << "\n";
}

void pad_source_pump() {
    if (g_controller != nullptr && !SDL_GameControllerGetAttached(g_controller)) {
        SDL_GameControllerClose(g_controller);
        g_controller = nullptr;
    }
    if (g_controller == nullptr) {
        for (int index = 0; index < SDL_NumJoysticks(); ++index) {
            if (SDL_IsGameController(index)) {
                g_controller = SDL_GameControllerOpen(index);
                break;
            }
        }
    }
    PadSample sample{};
    if (g_controller != nullptr) {
        sample.connected = true;
        sample.buttons = sdl_buttons(g_controller);
        sample.left_x = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTX);
        sample.left_y = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTY);
        sample.right_x = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTX);
        sample.right_y = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTY);
    }
    std::lock_guard<std::mutex> guard(g_mutex);
    g_latest = sample;
}

PadSample pad_source_read() noexcept {
    std::lock_guard<std::mutex> guard(g_mutex);
    return g_latest;
}

void pad_source_shutdown() {
    if (g_controller != nullptr) SDL_GameControllerClose(g_controller);
    g_controller = nullptr;
}

} // namespace vcsnx
