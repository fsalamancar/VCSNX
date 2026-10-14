#pragma once
#include <cstdint>
#include <span>

namespace vcsnx {

// Car reflections. Every frame VCS shrinks the last world frame into a 64x64 RGB565 map at the start of
// the back buffer (VRAM 0x04000000 or 0x04044000, stride 64): 11 through-mode sprites, each a 32 x 320
// strip of the world target (0x04088000) onto 6 x 64 pixels, then 4 blended sprites from a texture in
// RAM. The paint and glass of every car get an environment-mapped pass on that map (Mac [uvgen] log).
// With Vulkan the world exists only on the GPU and the CPU rasterizer is off, so the map was whatever
// VRAM held: rainbow noise that changed every frame (user photos). The sprites are drawn here instead,
// from the frame the backend reads back every vblank; the small target is rasterized on the CPU, so the
// blended sprites land on it; the cars then sample a real map, one frame late as on the PSP.

// The last frame the GPU drew, read back to the host: `width` x `height` RGBA8 pixels covering the guest's
// logical frame of `logical_width` x `logical_height`. Empty (rgba == nullptr) before the first frame.
struct HostFrameView {
    const std::uint8_t *rgba{};
    std::uint32_t width{}, height{};
    float logical_width{}, logical_height{};
};

// One through-mode sprite: the texture rectangle (u0, v0)-(u1, v1) onto the target rectangle
// (x0, y0)-(x1, y1), the corners as the two vertices give them.
struct ReflectionSprite {
    float u0, v0, x0, y0, u1, v1, x1, y1;
};

// The small target in guest memory.
struct ReflectionTarget {
    std::uint8_t *pixels{};
    std::uint32_t stride{};  // pixels per row
    std::uint32_t rows{};    // rows that may be written
    std::uint32_t format{};  // GE framebuffer format: 0 RGB565
    int clip_x1{}, clip_y1{};  // scissor's bottom-right corner, inclusive
};

// Draws the sprites into the target, each target pixel the average of the frame it covers (black without a
// frame). False, writing nothing, for a format other than RGB565.
[[nodiscard]] bool draw_reflection_sprites(std::span<const ReflectionSprite> sprites, const HostFrameView &frame,
                                           const ReflectionTarget &target) noexcept;

// A draw, as far as is_reflection_map_build needs it.
struct ReflectionMapDraw {
    bool through{};
    std::uint32_t primitive{};  // 6: sprites
    bool textured{};
    std::uint32_t target_stride{};
    std::uint32_t texture_address{};
    std::uint32_t gpu_frame_target{};  // the target the backend presents (0 if none)
};

// Whether a draw builds the reflection map from the world: textured through-mode sprites into a small
// target (cpu_rasterizes_target) sampling the target the GPU presents.
[[nodiscard]] bool is_reflection_map_build(const ReflectionMapDraw &draw) noexcept;

// Whether draws into a target of this stride are rasterized on the CPU even when the GPU draws the
// rest: targets at most 64 pixels wide (the reflection map) cost next to nothing and are sampled back.
[[nodiscard]] bool cpu_rasterizes_target(std::uint32_t stride) noexcept;

// The pass that puts the map on the cars' paint and glass (Mac [uvgen] log): environment-mapped UVs, the
// 64-wide map in VRAM, fixed blend 0x15 / 0xEA, i.e. 8% reflection over 92% paint. The author wanted 20%
// (2026-10-13); only this pass is changed.
struct CarPassDraw {
    std::uint32_t uv_generation{};        // GE 0xC0 & 3
    std::uint32_t texture_address{};      // level 0
    std::uint32_t texture_buffer_width{}; // GE 0xA8 & 0x7FF
    std::uint32_t blend_mode{};           // GE 0xDF: factors and equation
    std::uint32_t fix_source{}, fix_dest{};  // GE 0xE0 / 0xE1, 24-bit
};
[[nodiscard]] bool is_car_reflection_pass(const CarPassDraw &draw) noexcept;

struct ReflectionBlend {
    std::uint32_t source{}, dest{};  // 24-bit fixed blend colours
};
// The fixed factors for `strength` (0-1) of reflection: round(strength * 255) per channel and its complement.
[[nodiscard]] ReflectionBlend car_reflection_blend(float strength) noexcept;
// VCSNX_CAR_REFLECTION's value, 0-1 (0.08 is the game's own); 0.20 when unset or not a number.
[[nodiscard]] float parse_car_reflection(const char *text) noexcept;
// parse_car_reflection(getenv("VCSNX_CAR_REFLECTION")), read once.
[[nodiscard]] float car_reflection_strength() noexcept;

} // namespace vcsnx
