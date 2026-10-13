#include "audio_resample.hpp"

#include <algorithm>
#include <cmath>

namespace vcsnx {

void Resampler44To48::advance(Pull pull, void *user) {
    if (input_used_ == kBlock) {
        pull(user, input_, kBlock);
        input_used_ = 0u;
    }
    for (int channel = 0; channel < 2; ++channel) {
        history_[0][channel] = history_[1][channel];
        history_[1][channel] = history_[2][channel];
        history_[2][channel] = history_[3][channel];
        history_[3][channel] = input_[input_used_ * 2u + static_cast<std::size_t>(channel)];
    }
    ++input_used_;
    ++consumed_;
}

void Resampler44To48::produce(std::int16_t *out, std::size_t frames, Pull pull, void *user) {
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float t = static_cast<float>(phase_) / static_cast<float>(kSpan);
        for (int channel = 0; channel < 2; ++channel) {
            const float a = static_cast<float>(history_[0][channel]);
            const float b = static_cast<float>(history_[1][channel]);
            const float c = static_cast<float>(history_[2][channel]);
            const float d = static_cast<float>(history_[3][channel]);
            // Catmull-Rom between b (t = 0) and c (t = 1).
            const float value =
                b + 0.5f * t * (c - a + t * (2.0f * a - 5.0f * b + 4.0f * c - d + t * (3.0f * (b - c) + d - a)));
            out[frame * 2u + static_cast<std::size_t>(channel)] =
                static_cast<std::int16_t>(std::clamp(std::lround(value), -32768L, 32767L));
        }
        phase_ += kStep;
        while (phase_ >= kSpan) {
            phase_ -= kSpan;
            advance(pull, user);
        }
    }
}

} // namespace vcsnx
