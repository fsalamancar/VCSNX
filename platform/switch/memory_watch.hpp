#pragma once
// Out-of-memory evidence (backlog bug 2: the GE worker's operator new failed and the game closed). On the
// Switch, operator new is replaced so a failing or huge request logs its size, the heap's state and the
// caller to vcsnx_boot.log before the usual std::bad_alloc.

#include <cstddef>
#include <cstdint>

namespace vcsnx {

struct HeapSnapshot {
    std::size_t heap_bytes{};   // the whole newlib heap libnx set up
    std::size_t arena_bytes{};  // what malloc has taken from it so far
    std::size_t in_use_bytes{};
    std::size_t free_bytes{};   // free inside the arena
};

inline constexpr std::size_t kHugeRequestBytes = std::size_t{128} << 20;

[[nodiscard]] constexpr bool is_huge_request(std::size_t bytes) noexcept { return bytes >= kHugeRequestBytes; }

// Writes one log line into `out` (always NUL-terminated, never allocates: it runs when memory is gone). A
// caller inside [runtime_base, runtime_base + runtime_size) is shown as vcsnx+offset for addr2line on
// vcsnx.elf; anything else (vcs_game.so) as an address.
void format_allocation_report(char *out, std::size_t capacity, const char *what, std::size_t requested,
                              const HeapSnapshot &heap, std::uint64_t caller, std::uint64_t runtime_base,
                              std::uint64_t runtime_size) noexcept;

#if defined(__SWITCH__)
// Logs vcsnx.nro's base address and the heap's size once at startup.
void log_memory_layout() noexcept;
#endif

} // namespace vcsnx
