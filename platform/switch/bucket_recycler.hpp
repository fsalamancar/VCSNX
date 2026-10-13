#pragma once
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace vcsnx {

// Keeps the Vulkan backend's frame buckets (ge_gpu_backend_vulkan.cpp, FrameBucket) with their memory from
// one frame to the next. They were destroyed at every finish_color_frame() and rebuilt empty, so every
// frame regrew each bucket's vertex vectors by doubling up to several MiB: copies on the GE worker, and
// big blocks freed and asked for again around smaller ones that stay. On the Switch that left 2.4 GiB of
// the heap free but in pieces smaller than a frame's 3.6 MiB vertex vector, and the GE worker's next one
// failed (vcsnx_boot.log 2026-10-13: "out of memory: requested=3620 KiB ... in_use=639 MiB free=2483 MiB").
// At most `Keep` spare buckets are kept, and none whose memory is over `limit_bytes`.
template <typename Bucket, std::size_t Keep = 16>
class BucketRecycler {
public:
    explicit BucketRecycler(std::size_t limit_bytes = std::numeric_limits<std::size_t>::max()) noexcept
        : limit_bytes_(limit_bytes) {}

    // Empties `live`; each bucket is first emptied by `empty` (vectors cleared, memory kept) and kept for
    // take(). `bytes` says how much memory a bucket holds, for the limit.
    template <typename Empty, typename Bytes>
    void recycle(std::vector<Bucket> &live, Empty empty, Bytes bytes) {
        for (Bucket &bucket : live) {
            if (spare_.size() >= Keep || bytes(bucket) > limit_bytes_) continue;
            empty(bucket);
            spare_.push_back(std::move(bucket));
        }
        live.clear();
    }
    template <typename Empty>
    void recycle(std::vector<Bucket> &live, Empty empty) {
        recycle(live, empty, [](const Bucket &) { return std::size_t{0}; });
    }

    // A kept bucket (empty, memory kept) if there is one, otherwise a new one.
    [[nodiscard]] Bucket take() {
        if (spare_.empty()) return Bucket{};
        Bucket bucket = std::move(spare_.back());
        spare_.pop_back();
        return bucket;
    }

private:
    std::size_t limit_bytes_;
    std::vector<Bucket> spare_;
};

} // namespace vcsnx
