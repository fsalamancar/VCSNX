#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace vcsnx {

// Heap census for the out-of-memory crashes of 2026-10-13: mallinfo said 2.4-2.5 GiB free and ~570 MiB in
// use when a 901 KiB or 3.6 MiB request failed, so the free space was in pieces. With VCSNX_ALLOC_DIAG=1 the
// Switch build wraps malloc/free/realloc/calloc/memalign/aligned_alloc/posix_memalign (alloc_wrap.cpp,
// --wrap, which covers mesa and SDL too) and every ~10 s writes "[alloc-diag]": allocations and frees per
// size class in the window, how many blocks of each class are alive, the heap's totals and the callers of
// the big blocks. Blocks alive between big ones that come and go are what splits the heap.

// 0: < 1 KiB, 1: < 64 KiB, 2: < 1 MiB, 3: the rest.
[[nodiscard]] constexpr unsigned alloc_class(std::size_t bytes) noexcept {
    return bytes < 1024u ? 0u : bytes < 65536u ? 1u : bytes < (std::size_t{1} << 20) ? 2u : 3u;
}
inline constexpr unsigned kAllocClasses = 4u;
inline constexpr unsigned kTopCallers = 8u;

struct AllocCaller {
    std::uint64_t address{};
    std::uint64_t count{};
    std::uint64_t bytes{};
};

struct AllocSnapshot {
    std::uint64_t allocs[kAllocClasses]{};
    std::uint64_t frees[kAllocClasses]{};
    std::uint64_t bytes[kAllocClasses]{};
    std::int64_t live[kAllocClasses]{}; // since the start, not per window
    AllocCaller callers[kTopCallers]{};  // blocks of 64 KiB and up, most bytes first
    unsigned caller_count{};
    std::uint64_t dropped_callers{};     // big allocations whose caller did not fit the table
};

// Thread-safe, never allocates (it runs inside malloc).
class AllocStats {
public:
    void on_alloc(std::uint64_t caller, std::size_t bytes) noexcept;
    void on_free(std::size_t bytes) noexcept;
    // The window so far, then a new window.
    [[nodiscard]] AllocSnapshot take_window() noexcept;

private:
    static constexpr unsigned kTable = 256u;
    void lock() noexcept;
    void unlock() noexcept;

    std::mutex mutex_; // not a spin: see block_cache.hpp
    std::uint64_t allocs_[kAllocClasses]{};
    std::uint64_t frees_[kAllocClasses]{};
    std::uint64_t bytes_[kAllocClasses]{};
    std::int64_t live_[kAllocClasses]{};
    AllocCaller table_[kTable]{};
    std::uint64_t dropped_{};
};

// One "[alloc-diag] ..." line (no newline) in `out`, NUL-terminated; callers inside vcsnx.nro's code are
// written as vcsnx+offset. Returns its length.
std::size_t format_alloc_report(const AllocSnapshot &snapshot, std::uint64_t runtime_base, std::uint64_t runtime_size,
                                std::uint64_t arena_mb, std::uint64_t in_use_mb, std::uint64_t free_mb, char *out,
                                std::size_t capacity) noexcept;

// The process's census (alloc_wrap.cpp feeds it) and whether it is on. malloc runs long before
// vcsnx_env.txt is read, so nx_startup switches it on from VCSNX_ALLOC_DIAG once the file is applied.
AllocStats &alloc_stats() noexcept;
[[nodiscard]] bool alloc_diag_enabled() noexcept;
void set_alloc_diag(const char *setting) noexcept;
// VCSNX_BLOCK_CACHE=0 turns off the reuse of big freed blocks (alloc_wrap.cpp, block_cache.hpp); on otherwise.
void set_block_cache(const char *setting) noexcept;
// Set by memory_watch.cpp's operator new around its malloc: the code that asked for the memory.
extern thread_local std::uint64_t g_new_caller;
// Called from the 2 s status heartbeat: writes a report to stderr every ~10 s when enabled.
void alloc_diag_tick(long long now_ms) noexcept;

} // namespace vcsnx
