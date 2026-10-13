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

} // namespace vcsnx
