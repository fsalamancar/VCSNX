#include "memory_watch.hpp"
#include "test_support.hpp"

#include <string>

namespace {
using vcsnx_test::require;
constexpr std::size_t kMiB = std::size_t{1} << 20;
constexpr vcsnx::HeapSnapshot kHeap{3200 * kMiB, 3150 * kMiB, 3100 * kMiB, 50 * kMiB};
constexpr std::uint64_t kBase = 0x0f40a18000u;
constexpr std::uint64_t kSize = 0x6000000u;
} // namespace

int main() {
    return vcsnx_test::run({
        {"an out-of-memory report says what was asked, what the heap holds and who asked", [] {
             char text[256];
             vcsnx::format_allocation_report(text, sizeof text, "out of memory", 16 * kMiB, kHeap, kBase + 0x4154fb8u, kBase, kSize);
             require(std::string(text) == "[vcsnx-memory] out of memory: requested=16384 KiB heap=3200 MiB arena=3150 MiB "
                                          "in_use=3100 MiB free=50 MiB caller=vcsnx+0x4154fb8\n", "text: " + std::string(text));
         }},
        {"a caller outside vcsnx.nro (vcs_game.so) is shown as an address", [] {
             char text[256];
             vcsnx::format_allocation_report(text, sizeof text, "huge request", 200 * kMiB, kHeap, 0x2a00001234u, kBase, kSize);
             require(std::string(text).find(" caller=0x2a00001234\n") != std::string::npos, "text: " + std::string(text));
         }},
        {"a report never writes past its buffer", [] {
             char text[24];
             vcsnx::format_allocation_report(text, sizeof text, "out of memory", 1, kHeap, 0, kBase, kSize);
             require(std::string(text).size() == sizeof text - 1, "truncated to the buffer");
         }},
        {"only requests of 128 MiB or more count as huge", [] {
             require(!vcsnx::is_huge_request(128 * kMiB - 1), "just under");
             require(vcsnx::is_huge_request(128 * kMiB), "at the limit");
         }},
    });
}
