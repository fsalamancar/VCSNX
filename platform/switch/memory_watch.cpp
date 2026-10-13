#include "memory_watch.hpp"

#include <cstdio>

#if defined(__SWITCH__)
#include <atomic>
#include <cstdlib>
#include <malloc.h>
#include <new>
#include <switch.h>
#endif

namespace vcsnx {

void format_allocation_report(char *out, std::size_t capacity, const char *what, std::size_t requested,
                              const HeapSnapshot &heap, std::uint64_t caller, std::uint64_t runtime_base,
                              std::uint64_t runtime_size) noexcept {
    if (capacity == 0u) return;
    constexpr std::size_t kMiB = std::size_t{1} << 20;
    char where[40];
    if (caller >= runtime_base && caller - runtime_base < runtime_size)
        std::snprintf(where, sizeof where, "vcsnx+0x%llx", static_cast<unsigned long long>(caller - runtime_base));
    else
        std::snprintf(where, sizeof where, "0x%llx", static_cast<unsigned long long>(caller));
    std::snprintf(out, capacity,
                  "[vcsnx-memory] %s: requested=%zu KiB heap=%zu MiB arena=%zu MiB in_use=%zu MiB free=%zu MiB caller=%s\n",
                  what, requested >> 10, heap.heap_bytes / kMiB, heap.arena_bytes / kMiB, heap.in_use_bytes / kMiB,
                  heap.free_bytes / kMiB, where);
}

#if defined(__SWITCH__)
namespace {
extern "C" char *fake_heap_start;
extern "C" char *fake_heap_end;

struct Region {
    std::uint64_t base{};
    std::uint64_t size{};
};

// The code region this function lives in: vcsnx.nro's text, starting at the NRO's base.
Region runtime_region() noexcept {
    MemoryInfo info{};
    u32 page{};
    if (R_FAILED(svcQueryMemory(&info, &page, reinterpret_cast<u64>(&runtime_region)))) return {};
    return {info.addr, info.size};
}

HeapSnapshot heap_snapshot() noexcept {
    const struct mallinfo info = mallinfo();
    return {static_cast<std::size_t>(fake_heap_end - fake_heap_start), static_cast<std::size_t>(info.arena),
            static_cast<std::size_t>(info.uordblks), static_cast<std::size_t>(info.fordblks)};
}

thread_local bool reporting = false;
std::atomic<int> huge_reports{0};

void report(const char *what, std::size_t size, void *caller) noexcept {
    if (reporting) return; // mallinfo or stdio asking for memory while memory is gone
    reporting = true;
    const Region runtime = runtime_region();
    char text[256];
    format_allocation_report(text, sizeof text, what, size, heap_snapshot(), reinterpret_cast<std::uint64_t>(caller),
                             runtime.base, runtime.size);
    std::fputs(text, stderr);
    reporting = false;
}

void *allocate(std::size_t size, void *caller) {
    void *memory = std::malloc(size == 0u ? 1u : size);
    if (memory == nullptr) {
        report("out of memory", size, caller);
        throw std::bad_alloc();
    }
    if (is_huge_request(size) && huge_reports.fetch_add(1, std::memory_order_relaxed) < 16) report("huge request", size, caller);
    return memory;
}
} // namespace

void log_memory_layout() noexcept {
    const Region runtime = runtime_region();
    u64 total = 0, used = 0;
    svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    std::fprintf(stderr, "[vcsnx-memory] vcsnx base=0x%llx text=%llu KiB heap=%zu MiB process total=%llu MiB used=%llu MiB\n",
                 static_cast<unsigned long long>(runtime.base), static_cast<unsigned long long>(runtime.size >> 10),
                 static_cast<std::size_t>(fake_heap_end - fake_heap_start) >> 20, static_cast<unsigned long long>(total >> 20),
                 static_cast<unsigned long long>(used >> 20));
}
#endif

} // namespace vcsnx

#if defined(__SWITCH__)
void *operator new(std::size_t size) { return vcsnx::allocate(size, __builtin_return_address(0)); }
void *operator new[](std::size_t size) { return vcsnx::allocate(size, __builtin_return_address(0)); }
#endif
