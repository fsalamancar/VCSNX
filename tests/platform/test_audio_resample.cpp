// The mixer's 44.1 kHz stereo becomes the Switch's 48 kHz: exactly 147 frames in per 160 out, no clicks.
#include "audio_resample.hpp"
#include "test_support.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {

// A 44.1 kHz source: frame n is `value(n)` on both channels; counts how many frames were pulled.
struct Source {
    std::uint64_t next{};
    double (*value)(std::uint64_t){};
};

void pull(void *user, std::int16_t *frames, std::size_t count) {
    auto &source = *static_cast<Source *>(user);
    for (std::size_t i = 0; i < count; ++i) {
        const auto sample = static_cast<std::int16_t>(std::lround(source.value(source.next++)));
        frames[i * 2u] = sample;
        frames[i * 2u + 1u] = sample;
    }
}

constexpr double kPi = 3.14159265358979323846;

double dc(std::uint64_t) { return 1000.0; }
double ramp(std::uint64_t n) { return static_cast<double>(n % 20000u); }
double sine_1k_at(double n) { return 12000.0 * std::sin(2.0 * kPi * 1000.0 * n / 44100.0); }
double sine_1k(std::uint64_t n) { return sine_1k_at(static_cast<double>(n)); }

std::vector<std::int16_t> run(double (*value)(std::uint64_t), std::size_t frames, std::size_t block,
                              std::uint64_t *consumed = nullptr) {
    vcsnx::Resampler44To48 resampler;
    Source source{0u, value};
    std::vector<std::int16_t> out(frames * 2u);
    for (std::size_t done = 0; done < frames;) {
        const std::size_t now = std::min(block, frames - done);
        resampler.produce(out.data() + done * 2u, now, &pull, &source);
        done += now;
    }
    if (consumed != nullptr) *consumed = resampler.consumed();
    return out;
}

} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"48000 frames out take 44100 in", [] {
             std::uint64_t consumed = 0u;
             (void)run(&dc, 48000u, 1024u, &consumed);
             require(consumed == 44100u, "one second in: " + std::to_string(consumed));
             (void)run(&dc, 96000u, 1024u, &consumed);
             require(consumed == 88200u, "two seconds in: " + std::to_string(consumed));
         }},
        {"a constant stays constant", [] {
             const auto out = run(&dc, 4800u, 1024u);
             for (std::size_t i = 200u; i < out.size(); ++i) require(out[i] == 1000, "dc at " + std::to_string(i));
         }},
        {"the block size does not change a sample", [] {
             const auto big = run(&sine_1k, 10000u, 1024u);
             const auto small = run(&sine_1k, 10000u, 37u);
             require(big == small, "same output in 1024- and 37-frame blocks");
         }},
        {"a 1 kHz tone comes out as 1 kHz at 48 kHz", [] {
             const auto out = run(&sine_1k, 48000u, 1024u);
             // Output frame i is the input at i * 147 / 160 - 3 (resampler header).
             double worst = 0.0;
             for (std::size_t i = 100u; i < 48000u; ++i) {
                 const double input_position = static_cast<double>(i) * 147.0 / 160.0 - 3.0;
                 worst = std::max(worst, std::fabs(out[i * 2u] - sine_1k_at(input_position)));
             }
             require(worst < 40.0, "error " + std::to_string(worst) + " of 12000 (0.33%)");
         }},
        {"both channels are carried", [] {
             vcsnx::Resampler44To48 resampler;
             const auto left_only = [](void *, std::int16_t *frames, std::size_t count) {
                 for (std::size_t i = 0; i < count; ++i) {
                     frames[i * 2u] = 5000;
                     frames[i * 2u + 1u] = -5000;
                 }
             };
             std::vector<std::int16_t> out(2000u * 2u);
             resampler.produce(out.data(), 2000u, left_only, nullptr);
             require(out[1990u * 2u] == 5000 && out[1990u * 2u + 1u] == -5000, "left and right apart");
         }},
    });
}
