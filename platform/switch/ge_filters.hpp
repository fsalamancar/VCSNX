#pragma once
#include <cstdint>

namespace vcsnx {

// The GE state of one draw, as far as the filters below need it.
struct GeDrawState {
    bool through{};
    std::uint32_t primitive{};  // PSP primitive: 0 points, 1 lines, 2 line strip, 3+ triangles/sprites
    bool textured{};
    bool blend{};
    std::uint32_t blend_equation{}, blend_source{}, blend_dest{};
    std::uint32_t blend_fix_source{}, blend_fix_dest{};
};

// Every frame VCS adds a red trail that follows the player to the world target: 2D line strips, no
// texture, additive blend with both factors fixed at white (Mac [line-diag]). The world drawn over it
// hides it on the PSP; here it showed alone on black when pausing (user photo), as a scribble that grew
// with the distance travelled. Such draws are skipped.
[[nodiscard]] bool is_hidden_line_trail(const GeDrawState &draw) noexcept;

} // namespace vcsnx
