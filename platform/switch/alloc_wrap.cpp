// The C heap entry points, wrapped at link time (-Wl,--wrap=..., Switch build only) so mesa, SDL and libnx go
// through them too: big freed blocks are kept for reuse (block_cache.hpp), and VCSNX_ALLOC_DIAG counts the
// traffic (alloc_diag.hpp; off, one relaxed load per call).
#if defined(__SWITCH__)
#include "alloc_diag.hpp"
#include "block_cache.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <malloc.h>

extern "C" {
void *__real_malloc(std::size_t size);
void __real_free(void *pointer);
void *__real_calloc(std::size_t count, std::size_t size);
void *__real_realloc(void *pointer, std::size_t size);
void *__real_memalign(std::size_t alignment, std::size_t size);
void *__real_aligned_alloc(std::size_t alignment, std::size_t size);
int __real_posix_memalign(void **out, std::size_t alignment, std::size_t size);
}

namespace {

std::size_t usable_size(void *pointer) { return malloc_usable_size(pointer); }

// 32 blocks of 128 KiB and up, 128 MiB at most (block_cache.hpp). Constant-initialised: malloc reaches it from
// the first allocation of the process.
constinit vcsnx::BlockCache<32> big_blocks(std::size_t{128} << 10, std::size_t{128} << 20, &usable_size, &__real_free);
std::atomic<bool> cache_on{true};

void *take_kept(std::size_t size, std::size_t alignment) noexcept {
    return cache_on.load(std::memory_order_relaxed) ? big_blocks.take(size, alignment) : nullptr;
}

// A request the heap refused once more, after the kept blocks went back to it.
template <typename Request>
void *with_retry(Request request) {
    void *pointer = request();
    if (pointer == nullptr) {
        big_blocks.release_all();
        pointer = request();
    }
    return pointer;
}

inline void note_alloc(void *pointer, void *return_address) noexcept {
    if (pointer == nullptr || !vcsnx::alloc_diag_enabled()) return;
    const std::uint64_t from_new = vcsnx::g_new_caller;
    vcsnx::alloc_stats().on_alloc(from_new != 0u ? from_new : reinterpret_cast<std::uint64_t>(return_address),
                                  malloc_usable_size(pointer));
}

inline void note_free(void *pointer) noexcept {
    if (pointer == nullptr || !vcsnx::alloc_diag_enabled()) return;
    vcsnx::alloc_stats().on_free(malloc_usable_size(pointer));
}

} // namespace

extern "C" {

void *__wrap_malloc(std::size_t size) {
    void *pointer = take_kept(size, 16u);
    if (pointer == nullptr) pointer = with_retry([size] { return __real_malloc(size); });
    note_alloc(pointer, __builtin_return_address(0));
    return pointer;
}

void __wrap_free(void *pointer) {
    if (pointer == nullptr) return;
    note_free(pointer);
    if (cache_on.load(std::memory_order_relaxed) && big_blocks.keep(pointer)) return;
    __real_free(pointer);
}

void *__wrap_calloc(std::size_t count, std::size_t size) {
    void *pointer = with_retry([count, size] { return __real_calloc(count, size); });
    note_alloc(pointer, __builtin_return_address(0));
    return pointer;
}

void *__wrap_realloc(void *pointer, std::size_t size) {
    note_free(pointer);
    void *moved = with_retry([pointer, size] { return __real_realloc(pointer, size); });
    if (moved != nullptr) {
        note_alloc(moved, __builtin_return_address(0));
    } else if (pointer != nullptr && size != 0u && vcsnx::alloc_diag_enabled()) {
        vcsnx::alloc_stats().on_alloc(reinterpret_cast<std::uint64_t>(__builtin_return_address(0)),
                                      malloc_usable_size(pointer)); // failed: the old block is still alive
    }
    return moved;
}

void *__wrap_memalign(std::size_t alignment, std::size_t size) {
    void *pointer = take_kept(size, alignment);
    if (pointer == nullptr) pointer = with_retry([alignment, size] { return __real_memalign(alignment, size); });
    note_alloc(pointer, __builtin_return_address(0));
    return pointer;
}

void *__wrap_aligned_alloc(std::size_t alignment, std::size_t size) {
    void *pointer = take_kept(size, alignment);
    if (pointer == nullptr)
        pointer = with_retry([alignment, size] { return __real_aligned_alloc(alignment, size); });
    note_alloc(pointer, __builtin_return_address(0));
    return pointer;
}

int __wrap_posix_memalign(void **out, std::size_t alignment, std::size_t size) {
    if (void *kept = take_kept(size, alignment)) {
        *out = kept;
        note_alloc(kept, __builtin_return_address(0));
        return 0;
    }
    int result = __real_posix_memalign(out, alignment, size);
    if (result != 0) {
        big_blocks.release_all();
        result = __real_posix_memalign(out, alignment, size);
    }
    if (result == 0) note_alloc(*out, __builtin_return_address(0));
    return result;
}

} // extern "C"

namespace vcsnx {

void set_block_cache(const char *setting) noexcept {
    const bool on = !(setting != nullptr && setting[0] == '0' && setting[1] == '\0');
    cache_on.store(on, std::memory_order_relaxed);
    if (!on) big_blocks.release_all();
}

} // namespace vcsnx
#else
#include "alloc_diag.hpp"

namespace vcsnx {
void set_block_cache(const char *) noexcept {}
} // namespace vcsnx
#endif
