#include "vcsnx_hooks.hpp"

#include "trace_clock.hpp"

#include <cstddef>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#if defined(__SWITCH__)
#include <switch.h>
#endif

namespace vcsnx {
namespace {

constexpr std::size_t kGuestStackBytes = 16u * 1024u * 1024u;
constexpr std::size_t kGeWorkerStackBytes = 4u * 1024u * 1024u;

struct ThreadStart {
    ThreadRole role;
    unsigned index;
    std::function<void()> body;
};

void *thread_entry(void *raw) {
    std::unique_ptr<ThreadStart> start(static_cast<ThreadStart *>(raw));
    pin_current_thread(start->role, start->index);
    start->body();
    return nullptr;
}

const char *role_name(ThreadRole role) noexcept {
    switch (role) {
    case ThreadRole::Guest: return "guest";
    case ThreadRole::RasterWorker: return "raster";
    case ThreadRole::GeWorker: return "ge-worker";
    }
    return "?";
}

} // namespace

int core_for_thread(ThreadRole role, unsigned index) noexcept {
    switch (role) {
    case ThreadRole::Guest:
        return 1;
    case ThreadRole::RasterWorker:
        return (index % 2u == 1u) ? 2 : 0;
    case ThreadRole::GeWorker:
        return 2;
    }
    return 0;
}

ScopedThreadPriority::ScopedThreadPriority(int priority) noexcept {
#if defined(__SWITCH__)
    s32 current = -1;
    if (R_SUCCEEDED(svcGetThreadPriority(&current, CUR_THREAD_HANDLE)) &&
        R_SUCCEEDED(svcSetThreadPriority(CUR_THREAD_HANDLE, static_cast<u32>(priority))))
        previous_ = current;
#else
    (void)priority;
#endif
}

ScopedThreadPriority::~ScopedThreadPriority() {
#if defined(__SWITCH__)
    if (previous_ >= 0) svcSetThreadPriority(CUR_THREAD_HANDLE, static_cast<u32>(previous_));
#endif
}

void pin_current_thread(ThreadRole role, unsigned index) noexcept {
#if defined(__SWITCH__)
    const int core = core_for_thread(role, index);
    const Result result = svcSetThreadCoreMask(CUR_THREAD_HANDLE, core, 1u << core);
    s32 priority = -1;
    (void)svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
    // fprintf: no iostream locking from threads the guest may be waiting on.
    std::fprintf(stderr, "[vcsnx-threads] t=%lldms %s %u pin core=%d result=0x%x now_on=%u priority=0x%x\n",
                 vcsnx::trace_ms(), role_name(role), index, core,
                 static_cast<unsigned>(result), static_cast<unsigned>(svcGetCurrentProcessorNumber()),
                 static_cast<unsigned>(priority));
#else
    (void)role;
    (void)index;
    (void)role_name;
#endif
}

PinnedThread::PinnedThread(ThreadRole role, unsigned index, std::size_t stack_bytes,
                           std::function<void()> body) {
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, stack_bytes);
    auto *start = new ThreadStart{role, index, std::move(body)};
    const int result = pthread_create(&thread_, &attributes, &thread_entry, start);
    pthread_attr_destroy(&attributes);
    if (result != 0) {
        delete start;
        throw std::runtime_error(std::string("pthread_create failed for the ") + role_name(role) + " thread");
    }
    joinable_ = true;
#if defined(__SWITCH__)
    s32 priority = -1;
    (void)svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
    std::fprintf(stderr, "[vcsnx-threads] t=%lldms creator back from pthread_create (%s) on core %u priority=0x%x\n",
                 vcsnx::trace_ms(), role_name(role), static_cast<unsigned>(svcGetCurrentProcessorNumber()),
                 static_cast<unsigned>(priority));
#endif
}

PinnedThread::PinnedThread(PinnedThread &&other) noexcept
    : thread_(other.thread_), joinable_(std::exchange(other.joinable_, false)) {}

PinnedThread &PinnedThread::operator=(PinnedThread &&other) noexcept {
    if (this != &other) {
        join(); // a running thread is never dropped (std::thread would call std::terminate)
        thread_ = other.thread_;
        joinable_ = std::exchange(other.joinable_, false);
    }
    return *this;
}

PinnedThread::~PinnedThread() {
    join(); // never leave a running thread behind
}

void PinnedThread::join() {
    if (!joinable_) return;
    pthread_join(thread_, nullptr);
    joinable_ = false;
}

GuestThread::GuestThread(std::function<void()> body)
    : PinnedThread(ThreadRole::Guest, 0u, kGuestStackBytes, std::move(body)) {}

PinnedThread start_ge_worker(std::function<void()> body) {
    return PinnedThread(ThreadRole::GeWorker, 0u, kGeWorkerStackBytes, std::move(body));
}

} // namespace vcsnx
