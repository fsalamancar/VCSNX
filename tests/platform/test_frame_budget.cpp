// A GE frame bucket stops collecting geometry at its budget instead of growing host memory without
// bound (the Switch crash of 2026-10-11: operator new failed in the GE thread).
#include "frame_budget.hpp"
#include "test_support.hpp"

#include <cstddef>
#include <limits>

int main() {
    using vcsnx::bucket_has_room;
    using vcsnx_test::require;
    constexpr std::size_t kLimit = vcsnx::kHwBucketBytes;
    return vcsnx_test::run({
        {"geometry under the budget is accepted", [] {
             require(bucket_has_room(0u, 4u << 20, kLimit), "4 MiB into an empty 8 MiB bucket");
             require(bucket_has_room(4u << 20, 4u << 20, kLimit), "exactly full");
         }},
        {"geometry past the budget is refused", [] {
             require(!bucket_has_room((4u << 20) + 1u, 4u << 20, kLimit), "one byte over");
             require(!bucket_has_room(0u, kLimit + 1u, kLimit), "a single draw bigger than the bucket");
         }},
        {"huge sizes cannot wrap around into a yes", [] {
             const std::size_t huge = std::numeric_limits<std::size_t>::max();
             require(!bucket_has_room(huge, 1u, kLimit), "used already past the limit");
             require(!bucket_has_room(1u, huge, kLimit), "an absurd draw");
         }},
        {"the screen-space bucket has the larger ceiling", [] {
             require(vcsnx::kScreenBucketBytes > vcsnx::kHwBucketBytes, "64 MiB vs 8 MiB");
         }},
    });
}
