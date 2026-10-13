#include "alloc_diag.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#if defined(__SWITCH__)
#include <malloc.h>
#include <switch.h>
#endif

namespace vcsnx {

void AllocStats::lock() noexcept { mutex_.lock(); }

void AllocStats::unlock() noexcept { mutex_.unlock(); }

void AllocStats::on_alloc(std::uint64_t caller, std::size_t bytes) noexcept {
    const unsigned size_class = alloc_class(bytes);
    lock();
    ++allocs_[size_class];
    bytes_[size_class] += bytes;
    ++live_[size_class];
    if (size_class >= 2u) {
        // Open addressing on the caller; a full table counts the rest as dropped.
        unsigned slot = static_cast<unsigned>((caller >> 2u) * 2654435761u) % kTable;
        bool placed = false;
        for (unsigned probe = 0; probe < kTable; ++probe, slot = (slot + 1u) % kTable) {
            AllocCaller &entry = table_[slot];
            if (entry.address == caller || entry.count == 0u) {
                entry.address = caller;
                ++entry.count;
                entry.bytes += bytes;
                placed = true;
                break;
            }
        }
        if (!placed) ++dropped_;
    }
    unlock();
}

void AllocStats::on_free(std::size_t bytes) noexcept {
    const unsigned size_class = alloc_class(bytes);
    lock();
    ++frees_[size_class];
    --live_[size_class];
    unlock();
}

AllocSnapshot AllocStats::take_window() noexcept {
    AllocSnapshot snapshot;
    lock();
    for (unsigned c = 0; c < kAllocClasses; ++c) {
        snapshot.allocs[c] = allocs_[c];
        snapshot.frees[c] = frees_[c];
        snapshot.bytes[c] = bytes_[c];
        snapshot.live[c] = live_[c];
        allocs_[c] = frees_[c] = bytes_[c] = 0u;
    }
    for (const AllocCaller &entry : table_) {
        if (entry.count == 0u) continue;
        // Keep the kTopCallers biggest by bytes (insertion into a small sorted array).
        unsigned at = snapshot.caller_count;
        while (at > 0u && snapshot.callers[at - 1u].bytes < entry.bytes) --at;
        if (at >= kTopCallers) continue;
        const unsigned last = std::min(snapshot.caller_count, kTopCallers - 1u);
        for (unsigned i = last; i > at; --i) snapshot.callers[i] = snapshot.callers[i - 1u];
        snapshot.callers[at] = entry;
        if (snapshot.caller_count < kTopCallers) ++snapshot.caller_count;
    }
    snapshot.dropped_callers = dropped_;
    for (AllocCaller &entry : table_) entry = AllocCaller{};
    dropped_ = 0u;
    unlock();
    return snapshot;
}

namespace {

class Text {
public:
    Text(char *out, std::size_t capacity) noexcept : out_(out), capacity_(capacity) {
        if (capacity_ != 0u) out_[0] = '\0';
    }
    void add(const char *format, ...) noexcept __attribute__((format(printf, 2, 3))) {
        if (capacity_ == 0u || length_ + 1u >= capacity_) return;
        va_list args;
        va_start(args, format);
        const int wrote = std::vsnprintf(out_ + length_, capacity_ - length_, format, args);
        va_end(args);
        if (wrote > 0) length_ = std::min(length_ + static_cast<std::size_t>(wrote), capacity_ - 1u);
    }
    [[nodiscard]] std::size_t length() const noexcept { return length_; }

private:
    char *out_;
    std::size_t capacity_;
    std::size_t length_{};
};

using ull = unsigned long long;

} // namespace

std::size_t format_alloc_report(const AllocSnapshot &snapshot, std::uint64_t runtime_base, std::uint64_t runtime_size,
                                std::uint64_t arena_mb, std::uint64_t in_use_mb, std::uint64_t free_mb, char *out,
                                std::size_t capacity) noexcept {
    static const char *const names[kAllocClasses] = {"small", "medium", "large", "huge"};
    Text text(out, capacity);
    text.add("[alloc-diag] arena=%llu in_use=%llu free=%llu", static_cast<ull>(arena_mb), static_cast<ull>(in_use_mb),
             static_cast<ull>(free_mb));
    for (unsigned c = 0; c < kAllocClasses; ++c)
        text.add(" %s=+%llu/-%llu live=%lld mb=%.1f", names[c], static_cast<ull>(snapshot.allocs[c]),
                 static_cast<ull>(snapshot.frees[c]), static_cast<long long>(snapshot.live[c]),
                 static_cast<double>(snapshot.bytes[c]) / 1048576.0);
    text.add(" top:");
    for (unsigned i = 0; i < snapshot.caller_count; ++i) {
        const AllocCaller &caller = snapshot.callers[i];
        if (caller.address >= runtime_base && caller.address - runtime_base < runtime_size)
            text.add(" vcsnx+0x%llx", static_cast<ull>(caller.address - runtime_base));
        else
            text.add(" 0x%llx", static_cast<ull>(caller.address));
        text.add(" n=%llu mb=%.1f;", static_cast<ull>(caller.count), static_cast<double>(caller.bytes) / 1048576.0);
    }
    if (snapshot.dropped_callers != 0u) text.add(" dropped=%llu", static_cast<ull>(snapshot.dropped_callers));
    return text.length();
}

namespace {
// Constant-initialised (no guard): malloc reaches it from the first allocation of the process on.
constinit AllocStats g_stats;
} // namespace

AllocStats &alloc_stats() noexcept { return g_stats; }

namespace {
std::atomic<bool> diag_on{false};
} // namespace

thread_local std::uint64_t g_new_caller = 0u;

bool alloc_diag_enabled() noexcept { return diag_on.load(std::memory_order_relaxed); }

void set_alloc_diag(const char *setting) noexcept {
    diag_on.store(setting != nullptr && *setting != '\0' && *setting != '0', std::memory_order_relaxed);
}

void alloc_diag_tick(long long now_ms) noexcept {
    if (!alloc_diag_enabled()) return;
    static long long next_ms = 0;
    if (now_ms < next_ms) return;
    next_ms = now_ms + 10'000;
    const AllocSnapshot snapshot = alloc_stats().take_window();
    std::uint64_t arena = 0u, in_use = 0u, free_bytes = 0u, base = 0u, size = 0u;
#if defined(__SWITCH__)
    const struct mallinfo info = mallinfo();
    arena = static_cast<std::uint64_t>(info.arena) >> 20;
    in_use = static_cast<std::uint64_t>(info.uordblks) >> 20;
    free_bytes = static_cast<std::uint64_t>(info.fordblks) >> 20;
    MemoryInfo region{};
    u32 page = 0;
    if (R_SUCCEEDED(svcQueryMemory(&region, &page, reinterpret_cast<u64>(&alloc_diag_tick)))) {
        base = region.addr;
        size = region.size;
    }
#endif
    char line[1024];
    format_alloc_report(snapshot, base, size, arena, in_use, free_bytes, line, sizeof line);
    std::cerr << line << "\n";
}

} // namespace vcsnx
