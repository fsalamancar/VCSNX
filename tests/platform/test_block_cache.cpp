// Big blocks freed and asked for again are reused instead of going back to the heap.
#include "block_cache.hpp"
#include "test_support.hpp"

#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>

namespace {

// A fake heap: remembers each block's size; counts what really reaches it.
std::map<void *, std::size_t> sizes;
int real_frees = 0;

void *fake_alloc(std::size_t size, std::size_t alignment) {
    void *pointer = std::aligned_alloc(alignment, (size + alignment - 1u) / alignment * alignment);
    sizes[pointer] = size;
    return pointer;
}
std::size_t fake_usable(void *pointer) { return sizes.at(pointer); }
void fake_free(void *pointer) {
    ++real_frees;
    sizes.erase(pointer);
    std::free(pointer);
}

vcsnx::BlockCache<8> make_cache(std::size_t limit_bytes = std::size_t{64} << 20) {
    return vcsnx::BlockCache<8>(128u << 10, limit_bytes, &fake_usable, &fake_free);
}

} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"a freed big block comes back for the next request of its size", [] {
             auto cache = make_cache();
             void *frame = fake_alloc(3686400u, 16u);
             require(cache.keep(frame), "kept, not freed");
             require(cache.take(3686400u, 16u) == frame, "the same block");
             require(cache.take(3686400u, 16u) == nullptr, "only once");
             fake_free(frame);
         }},
        {"small blocks are not kept", [] {
             auto cache = make_cache();
             void *small = fake_alloc(4096u, 16u);
             require(!cache.keep(small), "left to the heap");
             fake_free(small);
         }},
        {"a request takes a block only of about its own size", [] {
             auto cache = make_cache();
             void *big = fake_alloc(4u << 20, 16u);
             require(cache.keep(big), "kept");
             require(cache.take(1u << 20, 16u) == nullptr, "not a 1 MiB request: it would waste 3 MiB");
             require(cache.take((4u << 20) - 100000u, 16u) == big, "a slightly smaller one fits");
             fake_free(big);
         }},
        {"alignment is honoured", [] {
             auto cache = make_cache();
             void *block = fake_alloc(1u << 20, 16u);
             const bool aligned_4k = (reinterpret_cast<std::uintptr_t>(block) & 0xFFFu) == 0u;
             require(cache.keep(block), "kept");
             void *got = cache.take(1u << 20, 0x1000u);
             require(aligned_4k ? got == block : got == nullptr, "only a block on a 4 KiB boundary serves a 4 KiB request");
             if (got == nullptr) require(cache.take(1u << 20, 16u) == block, "still there for plain requests");
             fake_free(block);
         }},
        {"the cache is bounded and gives the oldest back first", [] {
             real_frees = 0;
             auto cache = make_cache(std::size_t{3} << 20);
             void *a = fake_alloc(1u << 20, 16u);
             void *b = fake_alloc(1u << 20, 16u);
             void *c = fake_alloc(1u << 20, 16u);
             void *d = fake_alloc(1u << 20, 16u);
             require(cache.keep(a) && cache.keep(b) && cache.keep(c), "three fit the 3 MiB limit");
             require(cache.keep(d), "a fourth pushes the oldest out");
             require(real_frees == 1, "the oldest went back to the heap: " + std::to_string(real_frees));
             require(sizes.count(a) == 0u, "and it was a");
             cache.release_all();
             require(real_frees == 4, "release_all gives every block back");
         }},
    });
}
