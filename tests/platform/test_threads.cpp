#include "test_support.hpp"
#include "vcsnx_hooks.hpp"

#include <atomic>
#include <cstddef>
#include <utility>

int main() {
    using vcsnx::ThreadRole;
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"the loading screen's wait runs above the HD preload's workers, then gets its own priority back", [] {
             // Switch, 2026-10-14: at the game thread's own 0x3B, the same as the workers', the one sharing its core
             // held it, and the loading screen's bar and artworks froze. Horizon runs lower numbers first.
             require(vcsnx::kLoadingWaitPriority < 0x3B, "above the workers' 0x3B");
             { const vcsnx::ScopedThreadPriority raised(vcsnx::kLoadingWaitPriority); }  // no-op off the Switch
         }},
        {"guest thread owns core 1", [] {
             require(vcsnx::core_for_thread(ThreadRole::Guest, 0u) == 1, "guest core");
         }},
        {"raster workers alternate cores 2 and 0", [] {
             require(vcsnx::core_for_thread(ThreadRole::RasterWorker, 1u) == 2, "worker 1");
             require(vcsnx::core_for_thread(ThreadRole::RasterWorker, 2u) == 0, "worker 2");
             require(vcsnx::core_for_thread(ThreadRole::RasterWorker, 3u) == 2, "worker 3");
         }},
        {"guest thread runs its body and joins", [] {
             std::atomic<int> value{0};
             vcsnx::GuestThread thread([&value] { value.store(42); });
             thread.join();
             require(value.load() == 42, "body did not run before join returned");
         }},
        {"guest thread has room for an 8 MiB frame", [] {
             // A default secondary-thread stack (512 KiB on macOS) would crash here.
             std::atomic<bool> ran{false};
             vcsnx::GuestThread thread([&ran] {
                 // Touch every page from the top down, as a deep call chain would, so a small
                 // stack hits its guard page instead of skipping over it.
                 volatile char frame[8u * 1024u * 1024u];
                 for (std::size_t offset = sizeof(frame); offset >= 4096u; offset -= 4096u)
                     frame[offset - 1u] = 1;
                 frame[0] = 1;
                 ran.store(frame[0] == 1 && frame[sizeof(frame) - 1u] == 1);
             });
             thread.join();
             require(ran.load(), "large frame was not usable");
         }},
        {"GE worker owns core 2", [] {
             // Shared with raster helper 1 when PSPRECOMP_RASTER_THREADS > 1 (the Vulkan default is 1).
             require(vcsnx::core_for_thread(ThreadRole::GeWorker, 0u) == 2, "GE worker core");
         }},
        {"pinned thread can be moved and joined like std::thread", [] {
             std::atomic<int> value{0};
             vcsnx::PinnedThread empty;
             require(!empty.joinable(), "default-constructed thread is not joinable");
             vcsnx::PinnedThread started(ThreadRole::GeWorker, 0u, 1024u * 1024u, [&value] { value.store(7); });
             require(started.joinable(), "started thread is joinable");
             empty = std::move(started);
             require(!started.joinable() && empty.joinable(), "the handle moves");
             empty.join();
             require(!empty.joinable() && value.load() == 7, "joined after the body ran");
         }},
        {"GE worker has room for a 3 MiB frame", [] {
             // libnx gives a std::thread 128 KiB, macOS 512 KiB; the vendor's GE worker gets 4 MiB.
             std::atomic<bool> ran{false};
             vcsnx::PinnedThread thread = vcsnx::start_ge_worker([&ran] {
                 volatile char frame[3u * 1024u * 1024u];
                 for (std::size_t offset = sizeof(frame); offset >= 4096u; offset -= 4096u)
                     frame[offset - 1u] = 1;
                 frame[0] = 1;
                 ran.store(frame[0] == 1 && frame[sizeof(frame) - 1u] == 1);
             });
             thread.join();
             require(ran.load(), "large frame was not usable");
         }},
    });
}
