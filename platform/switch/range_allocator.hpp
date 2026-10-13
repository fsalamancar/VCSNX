#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace vcsnx {

// The stutter when new objects appear (2026-10-13, 99b5cb3): of the 1.84 ms each new texture costs on the GE
// worker, 1.36 ms is its own vkAllocateMemory + bind (nvmap/nvdrv work on the Switch). Texture images are bound
// instead to ranges of a few big GPU memory blocks; this hands out those ranges. Pure bookkeeping: the caller
// owns the blocks (one VkDeviceMemory each) and adds one when nothing fits. Not thread-safe: the Vulkan
// backend calls it under its own lock.
struct Range {
    std::uint32_t block{};
    std::uint64_t offset{};
    std::uint64_t size{};
};

class RangeAllocator {
public:
    explicit RangeAllocator(std::uint64_t block_bytes) noexcept : block_bytes_(block_bytes) {}

    [[nodiscard]] std::uint64_t block_bytes() const noexcept { return block_bytes_; }
    [[nodiscard]] std::size_t block_count() const noexcept { return blocks_.size(); }
    // Adds an empty block of block_bytes(); returns its index.
    std::uint32_t add_block();
    // First fit: `bytes` at a multiple of `alignment` (a power of two) in any block. Empty when no block has room
    // (the caller adds one and asks again) or `bytes` is larger than a block.
    [[nodiscard]] std::optional<Range> take(std::uint64_t bytes, std::uint64_t alignment);
    // A range from take(); it merges with the free space next to it.
    void give_back(const Range &range);
    [[nodiscard]] std::uint64_t free_bytes() const noexcept;

private:
    std::uint64_t block_bytes_;
    // Per block: free ranges by offset -> size.
    std::vector<std::map<std::uint64_t, std::uint64_t>> blocks_;
};

} // namespace vcsnx
