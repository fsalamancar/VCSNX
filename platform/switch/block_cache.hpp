#pragma once
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace vcsnx {

// Keeps big freed heap blocks for the next request of about the same size and alignment, instead of giving
// them back to newlib. The VCSNX_ALLOC_DIAG census of 2026-10-13 (5 minutes, a long pause, then out of
// memory with 2.6 GiB "free"): mesa's nouveau_bo_new asked for a ~3 MiB staging buffer for every frame SDL
// uploaded (5524 times, 16 GiB), texture decodes for ~330 KiB 23,669 times, the guest framebuffer decode for
// 391 KiB 1048 times, while 437,272 small blocks stayed alive between them. Big blocks going back and forth
// left holes that small ones then split, until no 492 KiB piece was left. A block kept here never makes a
// hole. Used by alloc_wrap.cpp (Switch); `free` and `usable` are the real heap's.
template <std::size_t Slots>
class BlockCache {
public:
    using Usable = std::size_t (*)(void *);
    using Free = void (*)(void *);

    constexpr BlockCache(std::size_t min_bytes, std::size_t limit_bytes, Usable usable, Free free) noexcept
        : min_bytes_(min_bytes), limit_bytes_(limit_bytes), usable_(usable), free_(free) {}

    // A kept block of at least `bytes` and at most an eighth more, on an `alignment` boundary; or nullptr.
    [[nodiscard]] void *take(std::size_t bytes, std::size_t alignment) noexcept {
        if (bytes < min_bytes_) return nullptr;
        lock();
        void *found = nullptr;
        for (std::size_t i = 0; i < count_; ++i) {
            Slot &slot = slots_[i];
            if (slot.bytes < bytes || slot.bytes > bytes + bytes / 8u) continue;
            if (alignment > 1u && (reinterpret_cast<std::uintptr_t>(slot.pointer) % alignment) != 0u) continue;
            found = slot.pointer;
            kept_bytes_ -= slot.bytes;
            for (std::size_t j = i + 1u; j < count_; ++j) slots_[j - 1u] = slots_[j];
            --count_;
            break;
        }
        unlock();
        return found;
    }

    // Keeps `pointer` if it is big enough; false leaves it to the caller to free. The oldest blocks go back to
    // the heap to stay within the slot count and the byte limit.
    [[nodiscard]] bool keep(void *pointer) noexcept {
        const std::size_t bytes = usable_(pointer);
        if (bytes < min_bytes_ || bytes > limit_bytes_) return false;
        void *evicted[Slots];
        std::size_t evicted_count = 0u;
        lock();
        while (count_ > 0u && (count_ == Slots || kept_bytes_ + bytes > limit_bytes_)) {
            evicted[evicted_count++] = slots_[0].pointer;
            kept_bytes_ -= slots_[0].bytes;
            for (std::size_t j = 1u; j < count_; ++j) slots_[j - 1u] = slots_[j];
            --count_;
        }
        slots_[count_++] = Slot{pointer, bytes};
        kept_bytes_ += bytes;
        unlock();
        for (std::size_t i = 0; i < evicted_count; ++i) free_(evicted[i]);
        return true;
    }

    // Gives every kept block back (memory is short: a failed allocation retries after this).
    void release_all() noexcept {
        void *all[Slots];
        std::size_t n = 0u;
        lock();
        for (std::size_t i = 0; i < count_; ++i) all[n++] = slots_[i].pointer;
        count_ = 0u;
        kept_bytes_ = 0u;
        unlock();
        for (std::size_t i = 0; i < n; ++i) free_(all[i]);
    }

private:
    struct Slot {
        void *pointer;
        std::size_t bytes;
    };
    // A kernel-arbitrated mutex, not a spin: on Horizon a spinner never gives its core back, and a holder
    // preempted by it on the same core never ran again (main thread frozen in the pause, 2026-10-13).
    void lock() noexcept { mutex_.lock(); }
    void unlock() noexcept { mutex_.unlock(); }

    std::size_t min_bytes_;
    std::size_t limit_bytes_;
    Usable usable_;
    Free free_;
    std::mutex mutex_;
    Slot slots_[Slots]{};
    std::size_t count_{};
    std::size_t kept_bytes_{};
};

} // namespace vcsnx
