#pragma once
#include <cstddef>
#include <cstdint>

namespace vcsnx {

// The mixer's 44.1 kHz stereo to the Switch's 48 kHz audout (audio_out.cpp): exactly 147 input frames per
// 160 output frames, so a second of output is a second of the game's audio, with 4-point Catmull-Rom
// interpolation (exact for constants and straight lines). Keeps its state across calls: any block size
// gives the same samples.
class Resampler44To48 {
public:
    // Fills `count` input frames (interleaved stereo int16) at 44.1 kHz.
    using Pull = void (*)(void *user, std::int16_t *frames, std::size_t count);

    // Writes `frames` interleaved stereo frames at 48 kHz to `out`, pulling input as needed.
    void produce(std::int16_t *out, std::size_t frames, Pull pull, void *user);

    // Input frames used so far (pulled ones still waiting in the block do not count). Output frame i sits
    // at input position i * 147 / 160 - 3.
    [[nodiscard]] std::uint64_t consumed() const noexcept { return consumed_; }

private:
    static constexpr std::uint32_t kStep = 147u;  // input frames ...
    static constexpr std::uint32_t kSpan = 160u;  // ... per this many output frames
    static constexpr std::size_t kBlock = 256u;   // input frames pulled at a time

    void advance(Pull pull, void *user);

    std::int32_t history_[4][2]{}; // x[-1], x[0], x[1], x[2] per channel; output lies between x[0] and x[1]
    std::uint32_t phase_{};        // position after x[0], in 1/kSpan of an input frame
    std::int16_t input_[kBlock * 2u]{};
    std::size_t input_used_{kBlock};
    std::uint64_t consumed_{};
};

} // namespace vcsnx
