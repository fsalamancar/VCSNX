#include "reflection_map.hpp"

#include <algorithm>
#include <cmath>

namespace vcsnx {
namespace {

constexpr std::uint32_t kFormat565 = 0u;
constexpr std::uint32_t kSprites = 6u;
constexpr std::uint32_t kSmallTargetStride = 64u;
// Samples per axis averaged into one target pixel: a pixel of the map covers about 5 x 5 pixels of the
// 512 x 320 frame (13 x 11 of a 1280 x 720 one), and a single sample shimmered as the camera moved.
constexpr int kSamples = 4;

struct Rgb {
    unsigned r, g, b;
};

Rgb average(const HostFrameView &frame, float u0, float v0, float u1, float v1) noexcept {
    if (frame.rgba == nullptr || frame.width == 0u || frame.height == 0u || frame.logical_width <= 0.0f ||
        frame.logical_height <= 0.0f)
        return {0u, 0u, 0u};
    const float to_x = static_cast<float>(frame.width) / frame.logical_width;
    const float to_y = static_cast<float>(frame.height) / frame.logical_height;
    unsigned r = 0u, g = 0u, b = 0u;
    for (int j = 0; j < kSamples; ++j) {
        const float v = v0 + (v1 - v0) * (static_cast<float>(j) + 0.5f) / kSamples;
        const auto y = static_cast<std::uint32_t>(std::clamp(v * to_y, 0.0f, static_cast<float>(frame.height - 1u)));
        for (int i = 0; i < kSamples; ++i) {
            const float u = u0 + (u1 - u0) * (static_cast<float>(i) + 0.5f) / kSamples;
            const auto x = static_cast<std::uint32_t>(std::clamp(u * to_x, 0.0f, static_cast<float>(frame.width - 1u)));
            const std::uint8_t *p = frame.rgba + (static_cast<std::size_t>(y) * frame.width + x) * 4u;
            r += p[0];
            g += p[1];
            b += p[2];
        }
    }
    constexpr unsigned count = kSamples * kSamples;
    return {r / count, g / count, b / count};
}

void draw_sprite(const ReflectionSprite &sprite, const HostFrameView &frame, const ReflectionTarget &target) noexcept {
    const float x_span = sprite.x1 - sprite.x0;
    const float y_span = sprite.y1 - sprite.y0;
    if (x_span == 0.0f || y_span == 0.0f) return;
    // A sprite covers [left, right) x [top, bottom), clipped to the scissor, the stride and the rows.
    const int left = std::max(0, static_cast<int>(std::ceil(std::min(sprite.x0, sprite.x1))));
    const int top = std::max(0, static_cast<int>(std::ceil(std::min(sprite.y0, sprite.y1))));
    const int right = std::min({static_cast<int>(std::ceil(std::max(sprite.x0, sprite.x1))), target.clip_x1 + 1,
                                static_cast<int>(target.stride)});
    const int bottom = std::min({static_cast<int>(std::ceil(std::max(sprite.y0, sprite.y1))), target.clip_y1 + 1,
                                 static_cast<int>(target.rows)});
    const float du = (sprite.u1 - sprite.u0) / x_span;
    const float dv = (sprite.v1 - sprite.v0) / y_span;
    for (int y = top; y < bottom; ++y) {
        const float v0 = sprite.v0 + (static_cast<float>(y) - sprite.y0) * dv;
        for (int x = left; x < right; ++x) {
            const float u0 = sprite.u0 + (static_cast<float>(x) - sprite.x0) * du;
            const Rgb c = average(frame, u0, v0, u0 + du, v0 + dv);
            const auto value = static_cast<std::uint16_t>((c.r >> 3u) | ((c.g >> 2u) << 5u) | ((c.b >> 3u) << 11u));
            std::uint8_t *out = target.pixels + (static_cast<std::size_t>(y) * target.stride + x) * 2u;
            out[0] = static_cast<std::uint8_t>(value & 0xFFu);
            out[1] = static_cast<std::uint8_t>(value >> 8u);
        }
    }
}

} // namespace

bool draw_reflection_sprites(std::span<const ReflectionSprite> sprites, const HostFrameView &frame,
                             const ReflectionTarget &target) noexcept {
    if (target.format != kFormat565 || target.pixels == nullptr) return false;
    for (const ReflectionSprite &sprite : sprites) draw_sprite(sprite, frame, target);
    return true;
}

bool is_reflection_map_build(const ReflectionMapDraw &draw) noexcept {
    constexpr std::uint32_t kVramOffsetMask = 0x001FFFF0u;
    return draw.through && draw.primitive == kSprites && draw.textured && cpu_rasterizes_target(draw.target_stride) &&
           draw.gpu_frame_target != 0u &&
           (draw.texture_address & kVramOffsetMask) == (draw.gpu_frame_target & kVramOffsetMask);
}

bool cpu_rasterizes_target(std::uint32_t stride) noexcept {
    return stride != 0u && stride <= kSmallTargetStride;
}

} // namespace vcsnx
