#include "copy_present.hpp"

#include <algorithm>

namespace vcsnx {

void RenderTargetHistory::note(std::uint32_t address) noexcept {
    if (contains(address)) return;
    addresses_[next_] = address;
    next_ = (next_ + 1) % kCapacity;
    count_ = std::min(count_ + 1, kCapacity);
}

bool RenderTargetHistory::contains(std::uint32_t address) const noexcept {
    return std::find(addresses_.begin(), addresses_.begin() + static_cast<std::ptrdiff_t>(count_), address) !=
           addresses_.begin() + static_cast<std::ptrdiff_t>(count_);
}

CopyOnlyPresent decide_copy_only_present(bool source_drawn_this_vblank) noexcept {
    return source_drawn_this_vblank ? CopyOnlyPresent::PresentSource : CopyOnlyPresent::Hold;
}

} // namespace vcsnx
