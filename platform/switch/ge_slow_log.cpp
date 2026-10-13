#include "ge_slow_log.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>

namespace vcsnx {
namespace {
std::atomic<std::uint64_t> compiles{0u};
std::atomic<std::uint64_t> compile_us_total{0u};
std::atomic<std::uint64_t> step_ns[4]{};
} // namespace

void count_pipeline_compile(std::uint64_t compile_us) noexcept {
    compiles.fetch_add(1u, std::memory_order_relaxed);
    compile_us_total.fetch_add(compile_us, std::memory_order_relaxed);
}

PipelineStats pipeline_stats() noexcept {
    return {compiles.load(std::memory_order_relaxed), compile_us_total.load(std::memory_order_relaxed)};
}

void count_texture_step(TextureStep step, std::uint64_t ns) noexcept {
    step_ns[static_cast<unsigned>(step) & 3u].fetch_add(ns, std::memory_order_relaxed);
}

TextureTimes texture_times() noexcept {
    return {step_ns[0].load(std::memory_order_relaxed), step_ns[1].load(std::memory_order_relaxed),
            step_ns[2].load(std::memory_order_relaxed), step_ns[3].load(std::memory_order_relaxed)};
}

__attribute__((weak)) GeBackendCounters ge_backend_counters() noexcept { return {}; }

std::size_t format_slow_list(const SlowListWork &work, char *out, std::size_t capacity) noexcept {
    if (capacity == 0u) return 0u;
    const int wrote = std::snprintf(
        out, capacity,
        "[ge-slow] list_ms=%.1f pipelines=%llu pipeline_ms=%.1f tex_uploads=%llu evictions=%llu flush_wait_ms=%.1f"
        " tex_ms=%.1f decode_ms=%.1f alloc_ms=%.1f objects_ms=%.1f",
        static_cast<double>(work.list_us) / 1000.0, static_cast<unsigned long long>(work.pipelines),
        static_cast<double>(work.pipeline_us) / 1000.0, static_cast<unsigned long long>(work.texture_uploads),
        static_cast<unsigned long long>(work.evictions), static_cast<double>(work.flush_wait_us) / 1000.0,
        static_cast<double>(work.texture_us) / 1000.0, static_cast<double>(work.decode_us) / 1000.0,
        static_cast<double>(work.alloc_us) / 1000.0, static_cast<double>(work.objects_us) / 1000.0);
    return wrote < 0 ? 0u : std::min(static_cast<std::size_t>(wrote), capacity - 1u);
}

} // namespace vcsnx
