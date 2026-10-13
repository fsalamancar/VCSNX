#pragma once
#include <cstddef>
#include <cstdint>

namespace vcsnx {

// Sound on the Switch through audout at its native 48 kHz, with four 1024-frame buffers always queued.
// SDL's Switch driver kept two audren buffers and played about 40,950 frames per second of real time
// instead of 44,100 ([audio-summary] wall_ms, 2026-10-13): the game, on a correct clock, produced 7% more
// than was played, so the mixer kept dropping samples at its queue cap (the "dirty" sound), on top of the
// driver's own gaps. `fill` mixes `count` frames of interleaved stereo at 44.1 kHz; Resampler44To48 turns
// them into 48 kHz. The thread runs on core 0 above the main thread's priority.
using AudioFill = void (*)(void *user, std::int16_t *frames, std::size_t count);

// Starts the output (Switch); false if audout is not available. Elsewhere always false (SDL is used).
[[nodiscard]] bool start_audio_out(AudioFill fill, void *user) noexcept;
// Stops it and releases audout. Safe to call when it was never started.
void stop_audio_out() noexcept;

} // namespace vcsnx
