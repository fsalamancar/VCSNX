// Frame buckets keep their memory from one frame to the next instead of being rebuilt every frame.
#include "bucket_recycler.hpp"
#include "test_support.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace {

struct FakeBucket {
    std::uint32_t address{};
    std::vector<int> vertices;
    std::vector<int> batches;
};

void empty_bucket(FakeBucket &bucket) {
    bucket.address = 0u;
    bucket.vertices.clear();
    bucket.batches.clear();
}

} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"a recycled bucket comes back empty with its memory", [] {
             vcsnx::BucketRecycler<FakeBucket> recycler;
             std::vector<FakeBucket> live;
             live.push_back(recycler.take());
             live.back().address = 0x88000u;
             live.back().vertices.assign(100000u, 7);
             const int *storage = live.back().vertices.data();
             recycler.recycle(live, &empty_bucket);
             require(live.empty(), "the frame's buckets are gone");
             FakeBucket next = recycler.take();
             require(next.address == 0u && next.vertices.empty() && next.batches.empty(), "empty");
             require(next.vertices.capacity() >= 100000u, "capacity kept");
             require(next.vertices.data() == storage, "the same memory, not a new allocation");
         }},
        {"with nothing recycled a fresh bucket is made", [] {
             vcsnx::BucketRecycler<FakeBucket> recycler;
             const FakeBucket bucket = recycler.take();
             require(bucket.vertices.capacity() == 0u, "fresh");
         }},
        {"only so many spare buckets are kept", [] {
             vcsnx::BucketRecycler<FakeBucket, 4> recycler;
             std::vector<FakeBucket> live(10u);
             for (FakeBucket &bucket : live) bucket.vertices.assign(10u, 1);
             recycler.recycle(live, &empty_bucket);
             require(live.empty(), "all buckets leave the frame");
             int with_memory = 0;
             for (int i = 0; i < 10; ++i)
                 if (recycler.take().vertices.capacity() != 0u) ++with_memory;
             require(with_memory == 4, "four kept, got " + std::to_string(with_memory));
         }},
        {"a bucket that grew past the limit gives its memory back", [] {
             vcsnx::BucketRecycler<FakeBucket, 16> recycler(1000u);
             std::vector<FakeBucket> live(1u);
             live[0].vertices.assign(5000u, 1);
             recycler.recycle(live, &empty_bucket, [](const FakeBucket &bucket) {
                 return bucket.vertices.capacity() * sizeof(int);
             });
             require(recycler.take().vertices.capacity() == 0u, "an oversized bucket is not kept");
         }},
    });
}
