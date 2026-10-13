#include "range_allocator.hpp"

#include <iterator>

namespace vcsnx {

std::uint32_t RangeAllocator::add_block() {
    blocks_.emplace_back();
    blocks_.back().emplace(0u, block_bytes_);
    return static_cast<std::uint32_t>(blocks_.size() - 1u);
}

std::optional<Range> RangeAllocator::take(std::uint64_t bytes, std::uint64_t alignment) {
    if (bytes == 0u || bytes > block_bytes_) return std::nullopt;
    if (alignment == 0u) alignment = 1u;
    for (std::size_t block = 0u; block < blocks_.size(); ++block) {
        auto &free = blocks_[block];
        for (auto it = free.begin(); it != free.end(); ++it) {
            const std::uint64_t start = it->first;
            const std::uint64_t end = start + it->second;
            const std::uint64_t aligned = (start + alignment - 1u) & ~(alignment - 1u);
            if (aligned + bytes > end) continue;
            free.erase(it);
            if (aligned > start) free.emplace(start, aligned - start);
            if (aligned + bytes < end) free.emplace(aligned + bytes, end - (aligned + bytes));
            return Range{static_cast<std::uint32_t>(block), aligned, bytes};
        }
    }
    return std::nullopt;
}

void RangeAllocator::give_back(const Range &range) {
    if (range.block >= blocks_.size() || range.size == 0u) return;
    auto &free = blocks_[range.block];
    std::uint64_t start = range.offset;
    std::uint64_t end = range.offset + range.size;
    auto next = free.lower_bound(start);
    if (next != free.begin()) {
        const auto previous = std::prev(next);
        if (previous->first + previous->second == start) {
            start = previous->first;
            free.erase(previous);
        }
    }
    if (next != free.end() && next->first == end) {
        end = next->first + next->second;
        free.erase(next);
    }
    free.emplace(start, end - start);
}

std::uint64_t RangeAllocator::free_bytes() const noexcept {
    std::uint64_t total = 0u;
    for (const auto &free : blocks_)
        for (const auto &[offset, size] : free) total += size;
    return total;
}

} // namespace vcsnx
