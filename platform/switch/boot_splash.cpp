#include "boot_splash.hpp"

#include <algorithm>
#include <cmath>

namespace vcsnx {
namespace {

constexpr float kLogoScreenShare = 0.54f; // logo width / screen width
constexpr float kPi = 3.14159265f;

// Build-up, in seconds and logo pixels (tuned on the previews the user chose; tools/make_splash_sound.py
// times the sound to the same moments).
constexpr float kSlideSeconds = 0.6f, kSlideDistance = 150.0f;
constexpr float kDropStart = 1.15f, kDropStagger = 0.08f, kDropSeconds = 0.35f, kDropDistance = 35.0f;

// The neon tube: flashes (start, end, brightness) before it stays on.
struct Flash {
    float start, end, level;
};
constexpr Flash kNeonFlashes[] = {
    {0.70f, 0.75f, 1.0f}, {0.80f, 0.83f, 0.55f}, {0.90f, 0.97f, 1.0f}, {1.00f, 1.02f, 0.3f}, {1.06f, kSplashGlitchStart, 1.0f},
};

// Glow: the lit tube glows a little, and the glow swells once while the logo holds.
constexpr float kNeonGlow = 0.35f, kSwellStart = 1.8f, kSwellSeconds = 1.0f, kSwellPeak = 0.45f;

// Glitch: red, green and blue copies drift apart; on a few frames the bands tear sideways.
constexpr int kGlitchBands = 6;
constexpr float kGlitchSplit = 14.0f;
struct Tear {
    float start, end;
    float offsets[kGlitchBands]; // logo pixels, per band from the top
};
constexpr Tear kTears[] = {
    {2.83f, 2.87f, {-22.0f, 14.0f, -8.0f, 26.0f, -16.0f, 9.0f}},
    {2.93f, 2.96f, {18.0f, -12.0f, 24.0f, -6.0f, 10.0f, -26.0f}},
    {3.00f, 3.03f, {-10.0f, 22.0f, -18.0f, 12.0f, -24.0f, 6.0f}},
};

// Fade-out: the sharp logo gives way to the blurred one while both fade and the logo grows a little.
constexpr float kFadeOutGrowth = 0.03f, kBlurPeak = 1.2f;
constexpr float kInvisible = 0.003f;

float ease(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

class Composer {
public:
    Composer(float logo_width, float logo_height, SplashScreen screen, std::vector<SplashDraw> &out)
        : logo_width_(logo_width), logo_height_(logo_height), screen_(screen),
          scale_(kLogoScreenShare * screen.width / logo_width), out_(out) {}

    // Draws source (logo pixels) moved by dx/dy logo pixels, the logo grown by zoom about its centre.
    void add(SplashLayer layer, SplashRect source, float intensity, float dx = 0.0f, float dy = 0.0f,
             float zoom = 1.0f, SplashTint tint = {}) {
        if (intensity <= kInvisible || source.y1 <= source.y0) return;
        const float s = scale_ * zoom;
        const auto x = [&](float v) { return screen_.width * 0.5f + (v - logo_width_ * 0.5f) * s + dx * scale_; };
        const auto y = [&](float v) { return screen_.height * 0.5f + (v - logo_height_ * 0.5f) * s + dy * scale_; };
        out_.push_back({layer, source, {x(source.x0), y(source.y0), x(source.x1), y(source.y1)},
                        std::min(intensity, 1.0f), tint});
    }

    [[nodiscard]] SplashRect whole() const noexcept { return {0.0f, 0.0f, logo_width_, logo_height_}; }

private:
    float logo_width_, logo_height_;
    SplashScreen screen_;
    float scale_;
    std::vector<SplashDraw> &out_;
};

float neon_level(float t) noexcept {
    for (const Flash &flash : kNeonFlashes)
        if (t >= flash.start && t < flash.end) return flash.level;
    return 0.0f;
}

void add_glitch(Composer &composer, float t) {
    const float u = (t - kSplashGlitchStart) / (kSplashFadeOutStart - kSplashGlitchStart);
    const float split = kGlitchSplit * (0.4f + 0.6f * std::fabs(std::sin(31.0f * u))) * (1.0f - 0.5f * u);
    const Tear *tear = nullptr;
    for (const Tear &window : kTears)
        if (t >= window.start && t < window.end) tear = &window;
    const SplashRect whole = composer.whole();
    for (int band = 0; band < kGlitchBands; ++band) {
        const SplashRect slice{whole.x0, whole.y1 * static_cast<float>(band) / kGlitchBands, whole.x1,
                               whole.y1 * static_cast<float>(band + 1) / kGlitchBands};
        const float shift = tear != nullptr ? tear->offsets[band] : 0.0f;
        composer.add(SplashLayer::Sharp, slice, 1.0f, shift - split, 0.0f, 1.0f, {1.0f, 0.0f, 0.0f});
        composer.add(SplashLayer::Sharp, slice, 1.0f, shift, 0.0f, 1.0f, {0.0f, 1.0f, 0.0f});
        composer.add(SplashLayer::Sharp, slice, 1.0f, shift + split, 0.0f, 1.0f, {0.0f, 0.0f, 1.0f});
    }
}

void add_piece(Composer &composer, const SplashPiece &piece, float t) {
    switch (piece.role) {
    case SplashRole::SlideLeft:
    case SplashRole::SlideRight: {
        const float p = ease(t / kSlideSeconds);
        const float side = piece.role == SplashRole::SlideLeft ? -1.0f : 1.0f;
        composer.add(SplashLayer::Sharp, piece.box, p, side * kSlideDistance * (1.0f - p));
        break;
    }
    case SplashRole::Neon:
        composer.add(SplashLayer::Sharp, piece.box, neon_level(t));
        break;
    case SplashRole::Drop: {
        const float p = ease((t - kDropStart - kDropStagger * static_cast<float>(piece.order)) / kDropSeconds);
        composer.add(SplashLayer::Sharp, piece.box, p, 0.0f, -kDropDistance * (1.0f - p));
        break;
    }
    }
}

} // namespace

std::vector<SplashDraw> splash_frame(float t, std::span<const SplashPiece> pieces, float logo_width,
                                     float logo_height, SplashScreen screen) {
    std::vector<SplashDraw> draws;
    if (t < 0.0f || t >= kSplashSeconds || logo_width <= 0.0f || logo_height <= 0.0f) return draws;
    Composer composer(logo_width, logo_height, screen, draws);
    if (t >= kSplashFadeOutStart) {
        const float p = ease((t - kSplashFadeOutStart) / (kSplashSeconds - kSplashFadeOutStart));
        const float zoom = 1.0f + kFadeOutGrowth * p;
        composer.add(SplashLayer::Sharp, composer.whole(), (1.0f - p) * (1.0f - p), 0.0f, 0.0f, zoom);
        composer.add(SplashLayer::Blurred, composer.whole(), kBlurPeak * (1.0f - p) * p, 0.0f, 0.0f, zoom);
        return draws;
    }
    if (t >= kSplashGlitchStart) {
        add_glitch(composer, t);
        return draws;
    }
    for (const SplashPiece &piece : pieces) add_piece(composer, piece, t);
    float glow = kNeonGlow * neon_level(t);
    if (t > kSwellStart && t < kSwellStart + kSwellSeconds)
        glow += kSwellPeak * std::sin(kPi * (t - kSwellStart) / kSwellSeconds);
    composer.add(SplashLayer::Glow, composer.whole(), glow);
    return draws;
}

float splash_skip(float t) noexcept { return std::max(t, kSplashFadeOutStart); }

void splash_fade_out(std::span<std::int16_t> samples, int channels) noexcept {
    if (channels <= 0) return;
    const std::size_t frames = samples.size() / static_cast<std::size_t>(channels);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float gain = frames > 1 ? 1.0f - static_cast<float>(frame) / static_cast<float>(frames - 1) : 0.0f;
        for (int channel = 0; channel < channels; ++channel) {
            std::int16_t &sample = samples[frame * static_cast<std::size_t>(channels) + static_cast<std::size_t>(channel)];
            sample = static_cast<std::int16_t>(std::lround(static_cast<float>(sample) * gain));
        }
    }
}

} // namespace vcsnx
