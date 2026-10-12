#include "boot_splash_sdl.hpp"
#include "pad_mapping.hpp"
#include "pad_source.hpp"
#include "trace_clock.hpp"

#include <SDL.h>

// Declarations only: vcs_texture_replace.cpp compiles stb_image (PNG only) into the game.
#include "../third_party/stb/stb_image.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <vector>

namespace vcsnx {
namespace {

constexpr std::uint32_t kSkipButtons = kPadA | kPadPlus;
constexpr float kSkipFadeSeconds = 0.3f;

// The splash sound, queued whole on its own audio device, which closes before the game opens its own.
// The audio subsystem is left initialised: SDL counts its users, so the game's SDL_InitSubSystem only
// adds one, and the console's audio service is not shut down and started again in between.
class SplashSound {
public:
    SplashSound() {
        if (kSplashWavSize == 0u) return;
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            std::cerr << "[vcsnx-splash] no audio: " << SDL_GetError() << "\n";
            return;
        }
        SDL_AudioSpec spec{};
        SDL_RWops *wav = SDL_RWFromConstMem(kSplashWav, static_cast<int>(kSplashWavSize));
        if (SDL_LoadWAV_RW(wav, 1, &spec, &samples_, &length_) == nullptr) {
            std::cerr << "[vcsnx-splash] cannot read the splash sound: " << SDL_GetError() << "\n";
            return;
        }
        if (spec.format != AUDIO_S16LSB || spec.channels == 0) {
            std::cerr << "[vcsnx-splash] the splash sound is not 16-bit PCM\n";
            return;
        }
        channels_ = spec.channels;
        rate_ = spec.freq;
        device_ = SDL_OpenAudioDevice(nullptr, 0, &spec, nullptr, 0); // SDL converts to the device's format
        if (device_ == 0) {
            std::cerr << "[vcsnx-splash] SDL_OpenAudioDevice failed: " << SDL_GetError() << "\n";
            return;
        }
        SDL_QueueAudio(device_, samples_, length_);
        SDL_PauseAudioDevice(device_, 0);
    }
    ~SplashSound() {
        if (device_ != 0) SDL_CloseAudioDevice(device_);
        if (samples_ != nullptr) SDL_FreeWAV(samples_);
    }
    SplashSound(const SplashSound &) = delete;
    SplashSound &operator=(const SplashSound &) = delete;

    // Replaces what is left to play with its next kSkipFadeSeconds, faded to silence.
    void fade_out() {
        if (device_ == 0) return;
        const Uint32 frame_bytes = 2u * channels_;
        const Uint32 queued = std::min(SDL_GetQueuedAudioSize(device_), length_);
        const Uint32 played = (length_ - queued) / frame_bytes * frame_bytes;
        const auto fade_frames = static_cast<Uint32>(kSkipFadeSeconds * static_cast<float>(rate_));
        const Uint32 bytes = std::min(length_ - played, fade_frames * frame_bytes);
        std::vector<std::int16_t> tail(bytes / 2u);
        std::memcpy(tail.data(), samples_ + played, bytes);
        splash_fade_out(tail, channels_);
        SDL_ClearQueuedAudio(device_);
        SDL_QueueAudio(device_, tail.data(), bytes);
    }

private:
    SDL_AudioDeviceID device_{0};
    Uint8 *samples_{nullptr};
    Uint32 length_{0};
    int channels_{0};
    int rate_{0};
};

SDL_Texture *load_splash_texture(SDL_Renderer *renderer, int &width, int &height) {
    int channels = 0;
    stbi_uc *rgba = stbi_load_from_memory(kSplashPng, static_cast<int>(kSplashPngSize), &width, &height, &channels, 4);
    if (rgba == nullptr) {
        std::cerr << "[vcsnx-splash] cannot decode the splash image: " << stbi_failure_reason() << "\n";
        return nullptr;
    }
    SDL_Texture *texture = nullptr;
    if (height % 3 != 0) {
        std::cerr << "[vcsnx-splash] the splash image is not three stacked layers (" << width << "x" << height << ")\n";
    } else {
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, width, height);
        if (texture == nullptr) {
            std::cerr << "[vcsnx-splash] SDL_CreateTexture failed: " << SDL_GetError() << "\n";
        } else {
            SDL_UpdateTexture(texture, nullptr, rgba, width * 4);
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_ADD); // the layers add up on black
        }
    }
    stbi_image_free(rgba);
    return texture;
}

bool window_closed() {
    bool closed = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT ||
            (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE))
            closed = true;
    }
    return closed;
}

void draw_frame(SDL_Renderer *renderer, SDL_Texture *texture, float t, float logo_width, float logo_height) {
    int output_w = 0, output_h = 0;
    SDL_GetRendererOutputSize(renderer, &output_w, &output_h);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    const std::span<const SplashPiece> pieces(kSplashPieces, kSplashPieceCount);
    const SplashScreen screen{static_cast<float>(output_w), static_cast<float>(output_h)};
    for (const SplashDraw &draw : splash_frame(t, pieces, logo_width, logo_height, screen)) {
        const auto level = [&](float tint) { return static_cast<Uint8>(std::lround(draw.intensity * tint * 255.0f)); };
        SDL_SetTextureColorMod(texture, level(draw.tint.r), level(draw.tint.g), level(draw.tint.b));
        const float layer_top = static_cast<float>(draw.layer) * logo_height;
        const SDL_Rect source{static_cast<int>(draw.source.x0), static_cast<int>(draw.source.y0 + layer_top),
                              static_cast<int>(draw.source.x1 - draw.source.x0),
                              static_cast<int>(draw.source.y1 - draw.source.y0)};
        const SDL_FRect target{draw.target.x0, draw.target.y0, draw.target.x1 - draw.target.x0,
                               draw.target.y1 - draw.target.y0};
        SDL_RenderCopyF(renderer, texture, &source, &target);
    }
    SDL_RenderPresent(renderer); // the renderer waits for vsync
}

} // namespace

bool play_boot_splash(SDL_Renderer *renderer) {
    int width = 0, height = 0;
    SDL_Texture *texture = load_splash_texture(renderer, width, height);
    if (texture == nullptr) return true; // no splash, the game starts as before
    const float logo_width = static_cast<float>(width);
    const float logo_height = static_cast<float>(height) / 3.0f;

    SplashSound sound;
    const auto start = std::chrono::steady_clock::now();
    float skip_offset = 0.0f;
    pad_source_pump();
    std::uint32_t held = pad_source_read().buttons; // A still held from launching the game is not a skip
    bool open = true;
    bool skipped = false;
    for (;;) {
        if (window_closed()) {
            open = false;
            break;
        }
        pad_source_pump();
        const std::uint32_t buttons = pad_source_read().buttons;
        const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
        if ((buttons & ~held & kSkipButtons) != 0u && !skipped) {
            skip_offset = splash_skip(elapsed + skip_offset) - elapsed;
            sound.fade_out();
            skipped = true;
        }
        held = buttons;
        const float t = elapsed + skip_offset;
        if (t >= kSplashSeconds) break;
        draw_frame(renderer, texture, t, logo_width, logo_height);
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    SDL_DestroyTexture(texture);
    std::cerr << "[vcsnx-splash] t=" << trace_ms() << "ms done" << (skipped ? " (skipped)" : "")
              << (open ? "" : " (window closed)") << "\n";
    return open;
}

} // namespace vcsnx
