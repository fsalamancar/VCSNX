#include "audio_out.hpp"

#if defined(__SWITCH__)
#include "audio_resample.hpp"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <switch.h>

namespace vcsnx {
namespace {

constexpr std::size_t kFrames = 1024u;                       // 21.3 ms at 48 kHz
constexpr std::size_t kBuffers = 4u;                         // 85 ms queued at most
constexpr std::size_t kBytes = kFrames * 2u * sizeof(std::int16_t); // 4 KiB: audout's 0x1000 alignment
constexpr int kCore = 0;
constexpr int kPriority = 0x2B; // above the main thread (0x2C): an empty device is audible at once

struct Output {
    AudioOutBuffer buffers[kBuffers]{};
    void *data[kBuffers]{};
    Resampler44To48 resampler;
    AudioFill fill{};
    void *user{};
    Thread thread{};
    std::atomic<bool> running{false};
    bool started{};
};
Output output;

void fill_buffer(AudioOutBuffer &buffer) {
    output.resampler.produce(static_cast<std::int16_t *>(buffer.buffer), kFrames, output.fill, output.user);
    buffer.data_size = kBytes;
    buffer.data_offset = 0u;
}

void output_main(void *) {
    while (output.running.load(std::memory_order_acquire)) {
        AudioOutBuffer *released = nullptr;
        u32 count = 0u;
        if (R_FAILED(audoutWaitPlayFinish(&released, &count, 100'000'000ull)) || released == nullptr) continue;
        // Played buffers come back one at a time; refill and queue each straight away.
        fill_buffer(*released);
        audoutAppendAudioOutBuffer(released);
    }
}

void release_buffers() {
    for (void *&data : output.data) {
        std::free(data);
        data = nullptr;
    }
}

} // namespace

bool start_audio_out(AudioFill fill, void *user) noexcept {
    if (output.started || fill == nullptr) return output.started;
    if (R_FAILED(audoutInitialize())) return false;
    if (R_FAILED(audoutStartAudioOut())) {
        audoutExit();
        return false;
    }
    output.fill = fill;
    output.user = user;
    for (std::size_t i = 0; i < kBuffers; ++i) {
        output.data[i] = std::aligned_alloc(0x1000, kBytes);
        if (output.data[i] == nullptr) {
            release_buffers();
            audoutStopAudioOut();
            audoutExit();
            return false;
        }
        std::memset(output.data[i], 0, kBytes);
        AudioOutBuffer &buffer = output.buffers[i];
        buffer.next = nullptr;
        buffer.buffer = output.data[i];
        buffer.buffer_size = kBytes;
        fill_buffer(buffer);
        audoutAppendAudioOutBuffer(&buffer);
    }
    output.running.store(true, std::memory_order_release);
    if (R_FAILED(threadCreate(&output.thread, &output_main, nullptr, nullptr, 0x10000, kPriority, kCore)) ||
        R_FAILED(threadStart(&output.thread))) {
        output.running.store(false, std::memory_order_release);
        audoutStopAudioOut();
        audoutExit();
        release_buffers();
        return false;
    }
    output.started = true;
    return true;
}

void stop_audio_out() noexcept {
    if (!output.started) return;
    output.running.store(false, std::memory_order_release);
    threadWaitForExit(&output.thread); // its wait times out within 100 ms
    threadClose(&output.thread);
    audoutStopAudioOut();
    audoutExit();
    release_buffers();
    output.started = false;
}

} // namespace vcsnx

#else

namespace vcsnx {

bool start_audio_out(AudioFill, void *) noexcept { return false; }
void stop_audio_out() noexcept {}

} // namespace vcsnx

#endif
