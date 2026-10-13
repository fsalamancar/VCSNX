#pragma once
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace vcsnx {

// The stutter when driving fast (author, 2026-10-13): GE lists of 130-214 ms (perf.log ge_ms_max) as new objects
// pop in. A list over 50 ms is logged as "[ge-slow]" with what the backend did during it: Vulkan pipelines
// compiled (NVK compiles each new pipeline on the GE worker, and nothing is cached), texture uploads,
// evictions and the time waited for uploads to flush. Every compile is logged too ("[vulkan] pipeline").

struct PipelineStats {
    std::uint64_t count{};
    std::uint64_t total_us{};
};

// Any thread (the GE worker compiles them).
void count_pipeline_compile(std::uint64_t compile_us) noexcept;
[[nodiscard]] PipelineStats pipeline_stats() noexcept;

// 2026-10-13 (ba612dd): no pipelines in the slow lists; their time grows ~1.7 ms per new texture. Where it goes:
// Block = the whole decode + upload of a new texture (GE renderer), Decode = PSP texels to RGBA8 (all levels),
// Alloc = vkAllocateMemory + vkBindImageMemory, Objects = vkCreateImage/ImageView and its descriptor set.
enum class TextureStep : unsigned { Block, Decode, Alloc, Objects };
struct TextureTimes {
    std::uint64_t block_ns{};
    std::uint64_t decode_ns{};
    std::uint64_t alloc_ns{};
    std::uint64_t objects_ns{};
};
// Any thread.
void count_texture_step(TextureStep step, std::uint64_t ns) noexcept;
[[nodiscard]] TextureTimes texture_times() noexcept;

// Counts the time from its construction to the end of its scope as `step`.
class StepTimer {
public:
    explicit StepTimer(TextureStep step) noexcept : step_(step), begin_(std::chrono::steady_clock::now()) {}
    ~StepTimer() {
        count_texture_step(step_, static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                      std::chrono::steady_clock::now() - begin_).count()));
    }
    StepTimer(const StepTimer &) = delete;
    StepTimer &operator=(const StepTimer &) = delete;

private:
    TextureStep step_;
    std::chrono::steady_clock::time_point begin_;
};

struct SlowListWork {
    std::uint64_t list_us{};
    std::uint64_t pipelines{};
    std::uint64_t pipeline_us{};
    std::uint64_t texture_uploads{};
    std::uint64_t evictions{};
    std::uint64_t flush_wait_us{};
    std::uint64_t texture_us{};
    std::uint64_t decode_us{};
    std::uint64_t alloc_us{};
    std::uint64_t objects_us{};
};

// The backend counters a slow list is compared on (the Vulkan backend defines it; elsewhere all zero).
struct GeBackendCounters {
    std::uint64_t texture_uploads{};
    std::uint64_t evictions{};
    std::uint64_t flush_wait_ns{};
};
[[nodiscard]] GeBackendCounters ge_backend_counters() noexcept;

[[nodiscard]] constexpr bool is_slow_list(std::uint64_t list_us) noexcept { return list_us >= 50'000u; }
// One "[ge-slow] ..." line (no newline), NUL-terminated in `out`; returns its length.
std::size_t format_slow_list(const SlowListWork &work, char *out, std::size_t capacity) noexcept;

} // namespace vcsnx
