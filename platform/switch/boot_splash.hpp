#pragma once
// The boot splash: the port author's "studios bAlo" logo, shown before the game starts. The outer
// pieces slide in, the green ones light up like a neon tube, the small letters drop in, the green
// glows, the logo glitches like a VHS tape, then blurs and fades to black (previews the user chose).
// This is the timeline only, without SDL; boot_splash_sdl.cpp draws it and plays its sound.

#include <cstdint>
#include <span>
#include <vector>

namespace vcsnx {

// How a logo piece enters. tools/make_splash.py assigns the roles when it cuts the logo.
enum class SplashRole : std::uint8_t { SlideLeft, SlideRight, Neon, Drop };

// The splash image stacks three layers of the logo, top to bottom: sharp, blurred, green glow only.
enum class SplashLayer : std::uint8_t { Sharp, Blurred, Glow };

struct SplashRect {
    float x0{}, y0{}, x1{}, y1{};
};

struct SplashPiece {
    SplashRole role{};
    std::uint8_t order{}; // Drop pieces fall in this order
    SplashRect box;       // in logo pixels
};

// Colour multiplier of a draw: the glitch draws red, green and blue copies that add up to white.
struct SplashTint {
    float r{1.0f}, g{1.0f}, b{1.0f};
};

struct SplashScreen {
    float width{}, height{};
};

struct SplashDraw {
    SplashLayer layer{};
    SplashRect source;  // logo pixels within the layer
    SplashRect target;  // screen pixels
    float intensity{};  // 0..1: multiplies the colour, the layers add up on black
    SplashTint tint;
};

inline constexpr float kSplashSeconds = 3.85f;
inline constexpr float kSplashGlitchStart = 2.80f;
inline constexpr float kSplashFadeOutStart = 3.05f;

// The draws for t seconds into the splash, the logo centred at 54% of the screen width. Empty once
// the splash is over.
[[nodiscard]] std::vector<SplashDraw> splash_frame(float t, std::span<const SplashPiece> pieces, float logo_width,
                                                   float logo_height, SplashScreen screen);

// Where a press of A or + jumps to: the fade-out.
[[nodiscard]] float splash_skip(float t) noexcept;

// Fades interleaved 16-bit samples linearly from full volume to silence: the end of the splash sound
// when the player skips.
void splash_fade_out(std::span<std::int16_t> samples, int channels) noexcept;

} // namespace vcsnx
