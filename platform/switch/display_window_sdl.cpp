// VCSNX window, presentation and input for the Mac test bench and the Switch: SDL2 window, SDL
// renderer for the software rasterizer's frames, controller through pad_source. Derived from the
// fork's platform/macos/display_window_sdl.cpp without Metal, keyboard or mouse.
//
// SDL objects belong to the main thread: main.cpp calls display_window_start() there before it
// starts the guest thread, and its loop calls display_window_pump_events() there afterwards.

#include "display_window.hpp"
#if defined(VCSNX_HAS_SPLASH)
#include "boot_splash_sdl.hpp"
#endif
#include "cpu_clock.hpp"
#include "alloc_diag.hpp"
#include "main_watchdog.hpp"
#include "hud_scale.hpp"
#include "hud_diag.hpp"
#include "hw_verify.hpp"

#include <algorithm>
#include <cstdio>
#include "pad_mapping.hpp"
#include "pad_source.hpp"
#include "loading_screen.hpp"
#include "present_math.hpp"
#include "trace_clock.hpp"
#include "vcs_config.hpp"
#include "vcs_key_prompts.hpp"
#include "vcs_vehicle_input.hpp"
#include "vcsnx_platform.hpp"

#include <SDL.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <exception>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace vcs {
namespace {

constexpr auto kShutdownTimeout = std::chrono::seconds(2);
constexpr long long kStatusTraceIntervalMs = 2000;

using vcsnx::trace_ms;

struct WindowState {
    // Shared between the guest thread (publishers) and the main thread (presenter).
    std::mutex mutex;
    std::vector<std::uint32_t> pixels; // top-down, in pixel_format
    std::uint32_t pixel_format{SDL_PIXELFORMAT_RGB888};
    std::uint32_t width{};
    std::uint32_t height{};
    vcsnx::Rect source{};  // the part of the frame shown, empty for all of it (vcsnx::menu_source)
    std::string pending_title;
    std::atomic<bool> frame_pending{false};
    std::atomic<bool> ready{false};
    std::atomic<bool> close_requested{false};
    std::atomic<bool> shutdown_requested{false};
    std::atomic<bool> destroyed{false};
    // Main thread only.
    SDL_Window *window{nullptr};
    SDL_Renderer *renderer{nullptr};
    SDL_Texture *texture{nullptr};
    int texture_width{};
    int texture_height{};
    std::uint32_t texture_format{};
    std::vector<std::uint32_t> upload;
    // VCSNX's loading screen (vcsnx::loading_screen): its artwork, and which one it is.
    SDL_Texture *loading_texture{nullptr};
    std::uint64_t loading_version{};
    long long loading_presented_ms{-1'000'000};
};

WindowState &window_state() {
    static WindowState state;
    return state;
}

bool create_window(WindowState &state) {
    std::cerr << "[vcsnx-display] t=" << trace_ms() << "ms SDL video init start\n";
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        std::cerr << "[vcsnx-display] SDL video init failed: " << SDL_GetError() << "\n";
        return false;
    }
#if defined(__SWITCH__)
    constexpr int kWidth = 1280;
    constexpr int kHeight = 720;
    // SDL2's Switch port resizes the window to 1920x1080 when docked (and back to 1280x720) only for
    // resizable windows; present_pending_frame() re-letterboxes from the output size every frame.
    constexpr Uint32 kFlags = SDL_WINDOW_RESIZABLE;
#else
    constexpr int kWidth = 960;
    constexpr int kHeight = 544;
    constexpr Uint32 kFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
#endif
    std::cerr << "[vcsnx-display] t=" << trace_ms() << "ms SDL video init done\n";
    state.window = SDL_CreateWindow("VCSNX", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kWidth, kHeight, kFlags);
    if (state.window == nullptr) {
        std::cerr << "[vcsnx-display] SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        return false;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    state.renderer = SDL_CreateRenderer(state.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (state.renderer == nullptr) {
        std::cerr << "[vcsnx-display] SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(state.window);
        state.window = nullptr;
        return false;
    }
    SDL_RendererInfo info{};
    SDL_GetRendererInfo(state.renderer, &info);
    std::cerr << "[vcsnx-display] t=" << trace_ms() << "ms window and renderer ready (" << (info.name ? info.name : "?")
              << ")\n";
    vcsnx::pad_source_start();
    return true;
}

void destroy_window(WindowState &state) {
    vcsnx::pad_source_shutdown();
    if (state.texture != nullptr) SDL_DestroyTexture(state.texture);
    if (state.renderer != nullptr) SDL_DestroyRenderer(state.renderer);
    if (state.window != nullptr) SDL_DestroyWindow(state.window);
    state.texture = nullptr;
    state.renderer = nullptr;
    state.window = nullptr;
    state.destroyed.store(true, std::memory_order_release);
}

// Swaps the caller's filled buffer in, so neither side allocates per frame.
// `format` is the SDL pixel format of the words: RGB888 (0x00RRGGBB) for guest framebuffers, ABGR8888
// (RGBA bytes) for GPU frames.
void publish_pixels(WindowState &state, std::vector<std::uint32_t> &pixels, std::uint32_t width,
                    std::uint32_t height, std::uint32_t format, vcsnx::Rect source = {}) {
    std::lock_guard<std::mutex> guard(state.mutex);
    std::swap(state.pixels, pixels);
    state.width = width;
    state.height = height;
    state.source = source;
    state.pixel_format = format;
    state.frame_pending.store(true, std::memory_order_release);
}

void present_pending_frame(WindowState &state) {
    if (!state.frame_pending.exchange(false, std::memory_order_acq_rel)) return;
    std::uint32_t width = 0u;
    std::uint32_t height = 0u;
    std::uint32_t format = SDL_PIXELFORMAT_RGB888;
    vcsnx::Rect source{};
    {
        std::lock_guard<std::mutex> guard(state.mutex);
        std::swap(state.upload, state.pixels);
        width = state.width;
        height = state.height;
        format = state.pixel_format;
        source = state.source;
    }
    if (width == 0u || height == 0u || state.upload.size() < std::size_t{width} * height) return;
    const int texture_width = static_cast<int>(width);
    const int texture_height = static_cast<int>(height);
    if (state.texture == nullptr || state.texture_width != texture_width || state.texture_height != texture_height ||
        state.texture_format != format) {
        if (state.texture != nullptr) SDL_DestroyTexture(state.texture);
        state.texture = SDL_CreateTexture(state.renderer, format, SDL_TEXTUREACCESS_STREAMING, texture_width,
                                          texture_height);
        state.texture_width = texture_width;
        state.texture_height = texture_height;
        state.texture_format = format;
        if (state.texture == nullptr) {
            std::cerr << "[vcsnx-display] SDL_CreateTexture failed: " << SDL_GetError() << "\n";
            return;
        }
    }
    SDL_UpdateTexture(state.texture, nullptr, state.upload.data(), texture_width * 4);
    int output_w = 0;
    int output_h = 0;
    SDL_GetRendererOutputSize(state.renderer, &output_w, &output_h);
    const vcsnx::Rect box = vcsnx::letterbox(output_w, output_h, width, height);
    const SDL_Rect destination{box.x, box.y, box.w, box.h};
    SDL_SetRenderDrawColor(state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(state.renderer);
    // A menu: its 480 x 272 part of the frame fills the same box (vcsnx::menu_source).
    const SDL_Rect part{source.x, source.y, source.w, source.h};
    SDL_RenderCopy(state.renderer, state.texture, source.w > 0 && source.h > 0 ? &part : nullptr, &destination);
    SDL_RenderPresent(state.renderer);
    vcsnx::note_main_present(trace_ms()); // main_watchdog.hpp
}

// VCSNX's loading screen, drawn instead of the game's frames while the game shows its loading artwork and while it
// waits for the RAM loads (loading_screen.hpp): the artwork over the whole frame, as the game draws it, and a bar
// like the game's at its bottom left.
void present_loading_screen(WindowState &state) {
    // 20 frames a second: at the window's 60 this thread took enough of core 0 from the HD preload's worker there
    // that the loads went from 37 s to 56 s (Switch, 2026-10-14).
    constexpr long long kLoadingFrameMs = 50;
    const long long now = trace_ms();
    if (now - state.loading_presented_ms < kLoadingFrameMs) return;
    state.loading_presented_ms = now;
    const auto [image, version] = vcsnx::loading_screen().image(now);
    if (image && image->width > 0u && (state.loading_texture == nullptr || version != state.loading_version)) {
        if (state.loading_texture != nullptr) SDL_DestroyTexture(state.loading_texture);
        state.loading_texture = SDL_CreateTexture(state.renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC,
                                                  static_cast<int>(image->width), static_cast<int>(image->height));
        if (state.loading_texture != nullptr)
            SDL_UpdateTexture(state.loading_texture, nullptr, image->rgba.data(), static_cast<int>(image->width) * 4);
        state.loading_version = version;
    }
    int output_w = 0;
    int output_h = 0;
    SDL_GetRendererOutputSize(state.renderer, &output_w, &output_h);
    // The same box as the game's own frames (on the Switch they fill the screen): with the logical 512 x 320 frame's
    // shape it had black bands at the sides and looked cut next to the game's (2026-10-14).
    const auto [frame_w, frame_h] = vcsnx::hw_verify_frame_extent();
    const vcsnx::Rect box =
        state.texture_width > 0 && state.texture_height > 0
            ? vcsnx::letterbox(output_w, output_h, static_cast<std::uint32_t>(state.texture_width),
                               static_cast<std::uint32_t>(state.texture_height))
            : vcsnx::letterbox(output_w, output_h, static_cast<std::uint32_t>(frame_w), static_cast<std::uint32_t>(frame_h));
    SDL_SetRenderDrawColor(state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(state.renderer);
    const SDL_Rect destination{box.x, box.y, box.w, box.h};
    if (state.loading_texture != nullptr) SDL_RenderCopy(state.renderer, state.loading_texture, nullptr, &destination);
    // The bar where the game draws its own, white with a pink fill (the author's screenshot, 2026-10-14). With no
    // wait to follow (a later loading screen) a pink stretch goes back and forth so it never looks stuck.
    const int bar_x = box.x + box.w * 51 / 1000;
    const int bar_w = box.w * 238 / 1000;
    const int bar_y = box.y + box.h * 882 / 1000;
    const int bar_h = std::max(6, box.h * 44 / 1000);
    const SDL_Rect track{bar_x, bar_y, bar_w, bar_h};
    SDL_Rect fill{bar_x, bar_y, 0, bar_h};
    if (const float fraction = vcsnx::loading_screen().fraction(now); fraction >= 0.0f) {
        fill.w = static_cast<int>(static_cast<float>(bar_w) * fraction);
    } else {
        constexpr long long kSweepMs = 1600;
        const float phase = static_cast<float>(now % (2 * kSweepMs)) / static_cast<float>(kSweepMs);
        const float at = phase <= 1.0f ? phase : 2.0f - phase;
        fill.w = bar_w / 4;
        fill.x = bar_x + static_cast<int>(at * static_cast<float>(bar_w - fill.w));
    }
    SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 235);
    SDL_RenderFillRect(state.renderer, &track);
    SDL_SetRenderDrawColor(state.renderer, 255, 130, 230, 255);
    SDL_RenderFillRect(state.renderer, &fill);
    SDL_RenderPresent(state.renderer);
    vcsnx::note_main_present(trace_ms()); // main_watchdog.hpp
}

} // namespace

bool display_window_enabled() { return true; }

void display_window_start() {
    WindowState &state = window_state();
    if (state.ready.load(std::memory_order_acquire)) return;
    vcsnx::platform_startup();
    if (!create_window(state)) {
        std::cerr << "[vcsnx-display] no window; continuing headless\n";
        return;
    }
#if defined(VCSNX_HAS_SPLASH)
    // The user's logo plays before the guest thread starts (main.cpp starts it after this returns).
    if (!vcsnx::play_boot_splash(state.renderer)) state.close_requested.store(true, std::memory_order_release);
#endif
    state.ready.store(true, std::memory_order_release);
}

void display_window_set_status(const char *status) {
    if (status == nullptr) return;
    WindowState &state = window_state();
    {
        std::lock_guard<std::mutex> guard(state.mutex);
        state.pending_title = std::string("VCSNX - ") + status;
    }
#if defined(__SWITCH__)
    // No title bar on the console: log the guest's status (vblank, dispatches, pc) as a heartbeat.
    static long long last_trace_ms = -kStatusTraceIntervalMs;
    const long long now = trace_ms();
    if (now - last_trace_ms >= kStatusTraceIntervalMs) {
        last_trace_ms = now;
        std::cerr << "[vcsnx-status] t=" << now << "ms " << status << "\n";
        vcsnx::reapply_cpu_clock_boost(); // docking or a performance-mode change resets the clock
        vcsnx::alloc_diag_tick(now);      // VCSNX_ALLOC_DIAG: heap census every ~10 s
    }
#endif
}

void display_window_set_aspect_lock(bool) noexcept {
    // Every frame is already letterboxed to its own aspect (present_math.hpp), so movies need no lock.
}

void display_window_present(const psprecomp::GuestMemory &memory, const FramebufferDescription &description) {
    WindowState &state = window_state();
    if (!state.ready.load(std::memory_order_acquire)) return;
    std::vector<std::uint8_t> rgb;
    try {
        rgb = decode_framebuffer_rgb(memory, description);
    } catch (const std::exception &) {
        return; // a transient framebuffer pointer outside EDRAM keeps the previous frame on screen
    }
    const std::size_t pixel_count = std::size_t{description.width} * description.height;
    if (rgb.size() < pixel_count * 3u) return;
    static thread_local std::vector<std::uint32_t> packed;
    packed.resize(pixel_count);
    for (std::size_t index = 0; index < pixel_count; ++index) {
        packed[index] = (std::uint32_t{rgb[index * 3u]} << 16u) | (std::uint32_t{rgb[index * 3u + 1u]} << 8u) |
                        std::uint32_t{rgb[index * 3u + 2u]};
    }
    publish_pixels(state, packed, description.width, description.height, SDL_PIXELFORMAT_RGB888);
}

void display_window_present_rgba(std::span<const std::byte> rgba, std::uint32_t width, std::uint32_t height) {
    WindowState &state = window_state();
    if (!state.ready.load(std::memory_order_acquire) || width == 0u || height == 0u) return;
    const std::size_t pixel_count = std::size_t{width} * height;
    if (rgba.size() < pixel_count * 4u) return;
    static thread_local std::vector<std::uint32_t> packed;
    vcsnx::copy_rgba_to_abgr8888(rgba, pixel_count, packed); // no per-pixel swizzle on the guest thread
    vcsnx::alloc_diag_tick(trace_ms()); // VCSNX_ALLOC_DIAG: every ~10 s of frames (the status heartbeat is rare)
    vcsnx::check_main_thread(trace_ms()); // a main thread that stopped presenting is logged (main_watchdog.hpp)
    vcsnx::Rect source{};
    const bool menu_crop = vcsnx::menu_shown().shown(trace_ms());
    if (menu_crop) {
        const auto [logical_width, logical_height] = vcsnx::hw_verify_frame_extent();
        source = vcsnx::menu_source(width, height, logical_width, logical_height);
    }
    static bool last_menu_crop = false;
    if (vcsnx::hud_diag_on() && menu_crop != last_menu_crop) {
        last_menu_crop = menu_crop;
        std::fprintf(stderr, "[hud-diag] t=%lldms menu crop %s\n", trace_ms(), menu_crop ? "on" : "off");
    }
    publish_pixels(state, packed, width, height, SDL_PIXELFORMAT_ABGR8888, source);
}

DisplayWindowSurface display_window_surface() { return {}; }

HostInputState display_window_input() {
    if (!window_state().ready.load(std::memory_order_acquire)) return {};
    vcsnx::PadMappingOptions options;
    options.invert_camera_y = vcs_configuration().controls.invert_camera_y;
    options.driving = vcs_player_in_vehicle();
    const HostInputState input = vcsnx::map_pad_to_psp(vcsnx::pad_source_read(), options);
    if (input.buttons != 0u || input.analog_x != 128u || input.analog_y != 128u || input.camera_x != 0 ||
        input.camera_y != 0 || input.accelerate || input.brake)
        vcs_prompt_note_input(true); // controller prompts (PSP buttons)
    return input;
}

std::uint32_t display_window_buttons() { return display_window_input().buttons; }

void display_window_analog(std::uint8_t &x, std::uint8_t &y) {
    const HostInputState input = display_window_input();
    x = input.analog_x;
    y = input.analog_y;
}

bool display_window_close_requested() {
    return window_state().close_requested.load(std::memory_order_relaxed);
}

bool display_window_debug_dump_requested() { return false; }

void display_window_shutdown() {
    WindowState &state = window_state();
    if (!state.ready.load(std::memory_order_acquire)) return;
    state.shutdown_requested.store(true, std::memory_order_release);
    const auto deadline = std::chrono::steady_clock::now() + kShutdownTimeout;
    while (!state.destroyed.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void display_window_pump_events() {
    WindowState &state = window_state();
    static bool first_pump = true;
    if (first_pump) {
        first_pump = false;
        std::cerr << "[vcsnx-display] t=" << trace_ms() << "ms main thread pumping\n";
    }
    if (state.destroyed.load(std::memory_order_acquire) || !state.ready.load(std::memory_order_acquire)) return;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT ||
            (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE))
            state.close_requested.store(true, std::memory_order_release);
    }
    std::string title;
    {
        std::lock_guard<std::mutex> guard(state.mutex);
        title.swap(state.pending_title);
    }
    if (!title.empty()) SDL_SetWindowTitle(state.window, title.c_str());
    vcsnx::pad_source_pump();
    const long long now = trace_ms();
    const bool loading = vcsnx::loading_screen().active(now);
    static bool was_loading = false;
    if (loading != was_loading) {
        was_loading = loading;
        const long long since = vcsnx::loading_screen().since_artwork_ms(now);
        if (loading)
            std::fprintf(stderr, "[loading-screen] t=%lldms up\n", now);
        else
            std::fprintf(stderr, "[loading-screen] t=%lldms down at the game's own frames, %lld ms after the artwork's last draw "
                                 "(a 200 ms hold went back %lld ms earlier, to the game's loading screen)\n",
                         now, since, std::max(0LL, since - vcsnx::kLoadingArtworkHoldMs));
    }
    if (loading) {
        present_loading_screen(state);
    } else {
        if (state.loading_texture != nullptr) {
            SDL_DestroyTexture(state.loading_texture);
            state.loading_texture = nullptr;
        }
        present_pending_frame(state);
    }
    if (state.shutdown_requested.load(std::memory_order_acquire)) destroy_window(state);
}

} // namespace vcs
