// Texture memory is carved out of a few big GPU blocks instead of one allocation per texture.
#include "range_allocator.hpp"
#include "test_support.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {
constexpr std::uint64_t kKiB = 1024u;

bool overlap(const vcsnx::Range &a, const vcsnx::Range &b) {
    return a.block == b.block && a.offset < b.offset + b.size && b.offset < a.offset + a.size;
}
} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"no room until a block is added", [] {
             vcsnx::RangeAllocator ranges(1024u * kKiB);
             require(!ranges.take(1000u, 256u).has_value(), "no block yet");
             require(ranges.add_block() == 0u, "first block is 0");
             const std::optional<vcsnx::Range> got = ranges.take(1000u, 256u);
             require(got.has_value(), "fits in the new block");
             require(got->block == 0u && got->offset == 0u && got->size == 1000u, "at its start");
         }},
        {"ranges are aligned and never overlap", [] {
             vcsnx::RangeAllocator ranges(1024u * kKiB);
             ranges.add_block();
             std::vector<vcsnx::Range> taken;
             for (const std::uint64_t bytes : {1000u, 5000u, 300u, 70000u, 4096u}) {
                 const std::optional<vcsnx::Range> got = ranges.take(bytes, 4096u);
                 require(got.has_value(), "fits");
                 require(got->offset % 4096u == 0u, "aligned: " + std::to_string(got->offset));
                 require(got->size == bytes, "exact size");
                 for (const vcsnx::Range &other : taken) require(!overlap(*got, other), "no overlap");
                 taken.push_back(*got);
             }
         }},
        {"a request larger than a block never fits", [] {
             vcsnx::RangeAllocator ranges(64u * kKiB);
             ranges.add_block();
             require(!ranges.take(64u * kKiB + 1u, 256u).has_value(), "too big");
             require(ranges.take(64u * kKiB, 256u).has_value(), "exactly a block fits");
         }},
        {"a full block spills into the next one", [] {
             vcsnx::RangeAllocator ranges(64u * kKiB);
             ranges.add_block();
             require(ranges.take(30u * kKiB, 4096u).has_value(), "first");
             require(ranges.take(30u * kKiB, 4096u).has_value(), "second");
             require(!ranges.take(30u * kKiB, 4096u).has_value(), "third does not fit");
             require(ranges.add_block() == 1u, "second block");
             const std::optional<vcsnx::Range> third = ranges.take(30u * kKiB, 4096u);
             require(third.has_value() && third->block == 1u && third->offset == 0u, "third in block 1");
         }},
        {"freed space is reused before the block grows", [] {
             vcsnx::RangeAllocator ranges(256u * kKiB);
             ranges.add_block();
             const vcsnx::Range a = *ranges.take(20u * kKiB, 4096u);
             const vcsnx::Range b = *ranges.take(20u * kKiB, 4096u);
             ranges.give_back(a);
             const vcsnx::Range again = *ranges.take(20u * kKiB, 4096u);
             require(again.block == a.block && again.offset == a.offset, "a's place");
             require(!overlap(again, b), "b untouched");
         }},
        {"returned ranges merge back into a whole block", [] {
             vcsnx::RangeAllocator ranges(256u * kKiB);
             ranges.add_block();
             std::vector<vcsnx::Range> taken;
             for (int i = 0; i < 8; ++i) taken.push_back(*ranges.take(32u * kKiB, 4096u));
             require(!ranges.take(4096u, 4096u).has_value(), "full");
             for (const int i : {3, 0, 7, 1, 5, 2, 6, 4}) ranges.give_back(taken[static_cast<std::size_t>(i)]);
             require(ranges.free_bytes() == 256u * kKiB, "all free");
             const std::optional<vcsnx::Range> whole = ranges.take(256u * kKiB, 4096u);
             require(whole.has_value() && whole->offset == 0u, "one range again");
         }},
    });
}
