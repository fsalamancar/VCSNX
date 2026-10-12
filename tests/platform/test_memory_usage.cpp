#include "memory_usage.hpp"
#include "test_support.hpp"

#include <cstring>
#include <memory>

namespace {
constexpr std::size_t kBlockBytes = 64u * 1024u * 1024u;
unsigned char *volatile g_escape = nullptr;

// A 64 MiB heap block, fully written so no page stays untouched. The pointer escapes through a
// volatile: clang may otherwise elide a new/delete pair whose contents it can predict.
std::unique_ptr<unsigned char[]> touched_block() {
    auto block = std::make_unique<unsigned char[]>(kBlockBytes);
    std::memset(block.get(), 0x5A, kBlockBytes);
    g_escape = block.get();
    return block;
}
} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"heap in use grows by a 64 MiB block", [] {
             const std::uint64_t before = vcsnx::memory_in_use_mb();
             const auto block = touched_block();
             const std::uint64_t during = vcsnx::memory_in_use_mb();
             require(during >= before + 60u, "64 MiB allocated and touched shows up as heap in use");
         }},
        {"heap in use falls again when the block is freed", [] {
             auto block = touched_block();
             const std::uint64_t during = vcsnx::memory_in_use_mb();
             block.reset();
             const std::uint64_t after = vcsnx::memory_in_use_mb();
             require(after + 60u <= during, "a current figure, not a peak or the reserved heap");
         }},
        {"total memory is only known on the Switch", [] {
             require(vcsnx::memory_total_mb() == 0u, "Mac reports 0");
         }},
    });
}
