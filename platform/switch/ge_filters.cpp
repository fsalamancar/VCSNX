#include "ge_filters.hpp"

namespace vcsnx {
namespace {

constexpr std::uint32_t kBlendAdd = 0u;
constexpr std::uint32_t kBlendFactorFix = 10u;
constexpr std::uint32_t kWhite = 0xFFFFFFu;

} // namespace

bool is_hidden_line_trail(const GeDrawState &draw) noexcept {
    const bool lines = draw.primitive == 1u || draw.primitive == 2u;
    return draw.through && lines && !draw.textured && draw.blend && draw.blend_equation == kBlendAdd &&
           draw.blend_source == kBlendFactorFix && draw.blend_dest == kBlendFactorFix &&
           (draw.blend_fix_source & kWhite) == kWhite && (draw.blend_fix_dest & kWhite) == kWhite;
}

} // namespace vcsnx
