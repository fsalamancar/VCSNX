#include "memory_usage.hpp"

#if defined(__SWITCH__)
#include <malloc.h>
#include <switch.h>
#elif defined(__APPLE__)
#include <malloc/malloc.h>
#endif

namespace vcsnx {
namespace {
constexpr std::uint64_t kMiB = 1024u * 1024u;
} // namespace

std::uint64_t memory_in_use_mb() noexcept {
#if defined(__SWITCH__)
    const struct mallinfo info = mallinfo();
    return static_cast<std::uint64_t>(info.uordblks) / kMiB;
#elif defined(__APPLE__)
    malloc_statistics_t stats{};
    malloc_zone_statistics(nullptr, &stats); // nullptr = every zone
    return static_cast<std::uint64_t>(stats.size_in_use) / kMiB;
#else
    return 0u;
#endif
}

std::uint64_t memory_total_mb() noexcept {
#if defined(__SWITCH__)
    u64 total = 0;
    if (R_FAILED(svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0))) return 0u;
    return total / kMiB;
#else
    return 0u;
#endif
}

} // namespace vcsnx
