#include "frame_budget.hpp"

#include <atomic>
#include <cstdio>

namespace vcsnx {

bool bucket_has_room(std::size_t used_bytes, std::size_t add_bytes, std::size_t limit_bytes) noexcept {
    return add_bytes <= limit_bytes && used_bytes <= limit_bytes - add_bytes;
}

void note_bucket_full(const char *kind, std::uint32_t target_address, std::size_t used_bytes,
                      std::size_t add_bytes) noexcept {
    static std::atomic<int> reports{0};
    const int n = reports.fetch_add(1, std::memory_order_relaxed);
    if (n >= 8) return;
    std::fprintf(stderr,
                 "[vulkan] %s geometry for target 0x%06x is full (%zu KiB + %zu KiB) without a vblank: "
                 "draw dropped%s\n",
                 kind, static_cast<unsigned>(target_address), used_bytes >> 10, add_bytes >> 10,
                 n == 7 ? " (further reports suppressed)" : "");
}

} // namespace vcsnx
