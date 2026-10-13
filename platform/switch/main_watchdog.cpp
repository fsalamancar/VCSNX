#include "main_watchdog.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <iostream>

#if defined(__SWITCH__)
#include <switch.h>
#endif

namespace vcsnx {
namespace {

constinit StallWatch main_watch(3000);

#if defined(__SWITCH__)
// "vcsnx+0x..." for an address in vcsnx.nro's code, the raw address otherwise.
void add_address(char *out, std::size_t capacity, std::size_t &length, std::uint64_t address, std::uint64_t base,
                 std::uint64_t size) {
    if (length + 1u >= capacity) return;
    const int wrote = address >= base && address - base < size
                          ? std::snprintf(out + length, capacity - length, " vcsnx+0x%llx",
                                          static_cast<unsigned long long>(address - base))
                          : std::snprintf(out + length, capacity - length, " 0x%llx",
                                          static_cast<unsigned long long>(address));
    if (wrote > 0) length = std::min(length + static_cast<std::size_t>(wrote), capacity - 1u);
}

bool readable(std::uint64_t address, std::size_t bytes) {
    MemoryInfo info{};
    u32 page = 0;
    if (R_FAILED(svcQueryMemory(&info, &page, address))) return false;
    return (info.perm & Perm_R) != 0u && address + bytes <= info.addr + info.size;
}

void report_main_thread(long long stalled_ms) {
    const Handle main = envGetMainThreadHandle();
    if (R_FAILED(svcSetThreadActivity(main, ThreadActivity_Paused))) {
        std::cerr << "[vcsnx-watchdog] main thread silent " << stalled_ms << " ms; could not pause it\n";
        return;
    }
    ThreadContext context{};
    const Result got = svcGetThreadContext3(&context, main);
    // Read its frame records while it stays paused: the stack cannot change under us.
    std::uint64_t frames[12]{};
    unsigned count = 0u;
    if (R_SUCCEEDED(got)) {
        std::uint64_t fp = context.fp;
        while (count < 12u && fp != 0u && (fp & 0xFu) == 0u && readable(fp, 16u)) {
            const auto *record = reinterpret_cast<const std::uint64_t *>(fp);
            frames[count++] = record[1];
            if (record[0] <= fp) break;
            fp = record[0];
        }
    }
    svcSetThreadActivity(main, ThreadActivity_Runnable);
    if (R_FAILED(got)) {
        std::cerr << "[vcsnx-watchdog] main thread silent " << stalled_ms << " ms; no context\n";
        return;
    }
    MemoryInfo code{};
    u32 page = 0;
    svcQueryMemory(&code, &page, reinterpret_cast<u64>(&report_main_thread));
    char line[512];
    std::size_t length = static_cast<std::size_t>(
        std::snprintf(line, sizeof line, "[vcsnx-watchdog] main thread silent %lld ms: pc", stalled_ms));
    add_address(line, sizeof line, length, context.pc.x, code.addr, code.size);
    length += static_cast<std::size_t>(std::snprintf(line + length, sizeof line - length, " lr"));
    add_address(line, sizeof line, length, context.lr, code.addr, code.size);
    length += static_cast<std::size_t>(std::snprintf(line + length, sizeof line - length, " frames:"));
    for (unsigned i = 0; i < count; ++i) add_address(line, sizeof line, length, frames[i], code.addr, code.size);
    std::cerr << line << "\n";
}
#endif

} // namespace

void note_main_present(long long now_ms) noexcept { main_watch.presented(now_ms); }

void check_main_thread(long long now_ms) noexcept {
    if (!main_watch.should_report(now_ms)) return;
#if defined(__SWITCH__)
    report_main_thread(3000);
#else
    std::cerr << "[vcsnx-watchdog] main thread silent for 3 s\n";
#endif
}

} // namespace vcsnx
