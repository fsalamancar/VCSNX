// The allocation census counts the heap's traffic by size and names who asks for the big blocks.
#include "alloc_diag.hpp"
#include "test_support.hpp"

#include <string>

namespace {
bool has(const std::string &text, const std::string &part) { return text.find(part) != std::string::npos; }
} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"sizes land in their class", [] {
             require(vcsnx::alloc_class(16u) == 0u && vcsnx::alloc_class(1023u) == 0u, "small");
             require(vcsnx::alloc_class(1024u) == 1u && vcsnx::alloc_class(65535u) == 1u, "medium");
             require(vcsnx::alloc_class(65536u) == 2u && vcsnx::alloc_class((1u << 20) - 1u) == 2u, "large");
             require(vcsnx::alloc_class(1u << 20) == 3u, "huge");
         }},
        {"a window counts allocations and frees per class, and what stays alive", [] {
             vcsnx::AllocStats stats;
             for (int i = 0; i < 10; ++i) stats.on_alloc(0x1000u, 32u);
             for (int i = 0; i < 4; ++i) stats.on_free(32u);
             stats.on_alloc(0x2000u, 2u << 20);
             const vcsnx::AllocSnapshot snapshot = stats.take_window();
             require(snapshot.allocs[0] == 10u && snapshot.frees[0] == 4u, "small traffic");
             require(snapshot.live[0] == 6, "six small blocks alive");
             require(snapshot.allocs[3] == 1u && snapshot.live[3] == 1, "one huge block");
             const vcsnx::AllocSnapshot next = stats.take_window();
             require(next.allocs[0] == 0u && next.live[0] == 6, "the window restarts; live counts carry on");
         }},
        {"big blocks are tallied by caller, biggest first", [] {
             vcsnx::AllocStats stats;
             for (int i = 0; i < 3; ++i) stats.on_alloc(0x5000u, 3u << 20);
             stats.on_alloc(0x6000u, 100u << 10);
             stats.on_alloc(0x7000u, 512u); // small: not tallied by caller
             const vcsnx::AllocSnapshot snapshot = stats.take_window();
             require(snapshot.caller_count >= 2u, "two big callers");
             require(snapshot.callers[0].address == 0x5000u && snapshot.callers[0].count == 3u, "the biggest first");
             require(snapshot.callers[0].bytes == (9ull << 20), "its bytes");
             char out[1024];
             const std::size_t length = vcsnx::format_alloc_report(snapshot, 0x4000u, 0x10000u, 0u, 0u, 0u, out, sizeof out);
             const std::string text(out, length);
             require(has(text, "vcsnx+0x1000 n=3 mb=9.0"), "callers relative to vcsnx: " + text);
         }},
        {"the caller table is bounded", [] {
             vcsnx::AllocStats stats;
             for (std::uint64_t caller = 1u; caller <= 5000u; ++caller) stats.on_alloc(caller * 16u, 70000u);
             const vcsnx::AllocSnapshot snapshot = stats.take_window();
             require(snapshot.dropped_callers > 0u, "callers past the table are counted, not stored");
             require(snapshot.allocs[2] == 5000u, "every allocation still counted");
         }},
    });
}
