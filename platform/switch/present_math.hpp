#pragma once
#include <cstdint>
#include <vector>
#include <span>
#include <cstring>
#include <cstddef>

namespace vcsnx {

struct Rect {
    int x{};
    int y{};
    int w{};
    int h{};
};

// Largest rectangle with the source's aspect ratio that fits the output, centred; the rest stays
// black. Integer math, so docked/handheld switches always land on the same pixels.
[[nodiscard]] constexpr Rect letterbox(int output_w, int output_h, std::uint32_t source_w,
                                       std::uint32_t source_h) noexcept {
    if (output_w <= 0 || output_h <= 0 || source_w == 0u || source_h == 0u) return {};
    const long long ow = output_w;
    const long long oh = output_h;
    const long long sw = source_w;
    const long long sh = source_h;
    Rect rect{};
    if (ow * sh > oh * sw) { // output is wider: bars left and right
        rect.h = output_h;
        rect.w = static_cast<int>(oh * sw / sh);
    } else {                 // output is taller (or equal): bars top and bottom
        rect.w = output_w;
        rect.h = static_cast<int>(ow * sh / sw);
    }
    rect.x = (output_w - rect.w) / 2;
    rect.y = (output_h - rect.h) / 2;
    return rect;
}

// The pause and start menus are laid out in the PSP's 480 x 272 screen at the top left of the guest's
// logical frame, which is 512 x 320 in VCS and is shown whole, so they left a gap at the right and at the
// bottom (user photo; Mac dump: the menu bar ends at 94% of the width and 85% of the height). While a menu
// is shown, this part of the frame (`frame_w` x `frame_h` pixels for `logical_w` x `logical_h`) is what
// fills the picture.
inline constexpr float kPspScreenWidth = 480.0f;
inline constexpr float kPspScreenHeight = 272.0f;

[[nodiscard]] constexpr Rect menu_source(std::uint32_t frame_w, std::uint32_t frame_h, float logical_w,
                                         float logical_h) noexcept {
    if (frame_w == 0u || frame_h == 0u || logical_w <= 0.0f || logical_h <= 0.0f) return {};
    const float part_x = kPspScreenWidth < logical_w ? kPspScreenWidth / logical_w : 1.0f;
    const float part_y = kPspScreenHeight < logical_h ? kPspScreenHeight / logical_h : 1.0f;
    return {0, 0, static_cast<int>(static_cast<float>(frame_w) * part_x + 0.5f),
            static_cast<int>(static_cast<float>(frame_h) * part_y + 0.5f)};
}

// The GPU frame arrives as RGBA bytes. SDL_PIXELFORMAT_ABGR8888 stores R in the low byte on
// little-endian, which is that same byte order, so the frame is copied as is instead of swizzled
// pixel by pixel on the guest thread. `words` is resized to pixel_count.
inline void copy_rgba_to_abgr8888(std::span<const std::byte> rgba, std::size_t pixel_count,
                                  std::vector<std::uint32_t> &words) {
    words.resize(pixel_count);
    if (rgba.size() >= pixel_count * 4u) std::memcpy(words.data(), rgba.data(), pixel_count * 4u);
}

} // namespace vcsnx
