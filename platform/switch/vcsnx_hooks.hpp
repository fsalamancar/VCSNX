#pragma once
// Entry points VCSNX adds to the vendored PSPRecomp host (UPSTREAM.md, "Cambios locales").
// Vendor code reaches these only under #if defined(VCSNX).

#include <cstddef>
#include <functional>
#include <string>

#include <pthread.h>

namespace vcsnx {

// Backlog bug 1: perf.log's LoadingWatch saw 30 s of loading screen (stderr_tee.cpp asks); the guest thread
// takes the request at its next vblank and dumps the game's scheduler (vcs_profile.cpp).
void request_scheduler_dump() noexcept;
[[nodiscard]] bool take_scheduler_dump_request() noexcept;
// Appends `text` to vcsnx_stall.log in the data folder in one write (the dump's long import history).
void write_stall_report(const std::string &text) noexcept;

enum class ThreadRole {
    Guest,        // the PSP interpreter thread started by main.cpp
    RasterWorker, // ge_renderer.cpp RowWorkerPool helpers; index runs 1..N-1
    GeWorker,     // vcs_profile.cpp's asynchronous GE display-list worker (PSPRECOMP_GE_ASYNC)
};

// Core a thread should run on. A Switch application owns cores 0-2: core 0 keeps the main
// thread (SDL events and presentation), the guest gets core 1 to itself, and raster workers
// take core 2 first and then wrap onto core 0. The GE worker (only with PSPRECOMP_GE_ASYNC=1) takes
// core 2; with more than one raster thread it shares that core with raster helper 1, which spins
// after every job, so the Vulkan defaults (startup_rules.hpp) keep one raster thread and no async GE.
[[nodiscard]] int core_for_thread(ThreadRole role, unsigned index) noexcept;

// Pins the calling thread to core_for_thread(role, index) on the Switch; no-op elsewhere.
void pin_current_thread(ThreadRole role, unsigned index) noexcept;

// A thread with its own stack size, pinned as (role, index) before its body runs. Movable and
// joinable like std::thread, which cannot set a stack size (libnx gives it 128 KiB). The
// destructor joins instead of calling std::terminate.
class PinnedThread {
public:
    PinnedThread() = default;
    PinnedThread(ThreadRole role, unsigned index, std::size_t stack_bytes, std::function<void()> body);
    PinnedThread(PinnedThread &&other) noexcept;
    PinnedThread &operator=(PinnedThread &&other) noexcept;
    PinnedThread(const PinnedThread &) = delete;
    PinnedThread &operator=(const PinnedThread &) = delete;
    ~PinnedThread();

    [[nodiscard]] bool joinable() const noexcept { return joinable_; }
    void join();

private:
    pthread_t thread_{};
    bool joinable_{false};
};

// The guest thread: a 16 MiB stack (the Windows build links a 64 MB main stack; default
// secondary-thread stacks are far smaller) and pinned as ThreadRole::Guest. Same shape as the
// std::thread it replaces in main.cpp: construct to start, join() to wait.
class GuestThread : public PinnedThread {
public:
    explicit GuestThread(std::function<void()> body);
};

// The vendor's GE worker (vcs_profile.cpp, ge_async_start_worker): a 4 MiB stack, eight times the
// macOS default the fork validated it with, pinned as ThreadRole::GeWorker.
[[nodiscard]] PinnedThread start_ge_worker(std::function<void()> body);

} // namespace vcsnx
