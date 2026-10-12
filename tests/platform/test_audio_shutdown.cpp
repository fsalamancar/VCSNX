// The SDL audio branch mixes on a static submit worker thread.
// 1. install_profile() calls audio_output_shutdown() at boot, before the device exists; audio
//    submitted afterwards must still be processed (a latched stop flag silenced the game).
// 2. After the final audio_output_shutdown() the process must exit cleanly (a joinable
//    std::thread left in that static object calls std::terminate during exit).
#include "audio_output.hpp"
#include "test_support.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

// "[audio-summary] ... ch0[queued=.. underrun=.. trimmed=.. pushed=N ..." -> N, or -1.
long long pushed_frames_on_channel0(const std::string &log) {
    const std::size_t channel = log.find(" ch0[");
    if (channel == std::string::npos) return -1;
    const std::size_t key = log.find("pushed=", channel);
    if (key == std::string::npos) return -1;
    return std::stoll(log.substr(key + 7u));
}

} // namespace

int main() {
    // SDL's dummy device consumes audio in real time without a speaker: the test stays silent and
    // runs on machines with no audio device.
    setenv("SDL_AUDIODRIVER", "dummy", 0);
    setenv("PSPRECOMP_AUDIO", "1", 1);
    setenv("PSPRECOMP_AUDIO_SUMMARY", "1", 1);
    using vcsnx_test::require;
    const int status = vcsnx_test::run({
        {"audio still flows after the boot-time reset shutdown", [] {
             vcs::audio_output_shutdown(); // what install_profile() does before any audio exists
             std::vector<std::int16_t> tone(2u * 512u);
             for (std::size_t i = 0; i < tone.size(); ++i) tone[i] = static_cast<std::int16_t>((i % 64u) * 256);
             for (int chunk = 0; chunk < 10; ++chunk) {
                 vcs::audio_output_submit(tone, 512u, true, 0x8000u, 0x8000u, 44100u, 0u, 0u);
                 std::this_thread::sleep_for(std::chrono::milliseconds(20));
             }
             std::this_thread::sleep_for(std::chrono::milliseconds(300));
             std::ostringstream captured;
             std::streambuf *previous = std::cerr.rdbuf(captured.rdbuf());
             vcs::audio_output_advance(0u);
             std::cerr.rdbuf(previous);
             const long long pushed = pushed_frames_on_channel0(captured.str());
             require(pushed >= 4096, "only " + std::to_string(pushed) + " of 5120 submitted frames reached the mixer: " +
                                         captured.str());
         }},
        {"final shutdown then clean exit", [] {
             vcs::audio_output_shutdown();
         }},
    });
    return status; // static destructors run after this: a joinable worker would abort here
}
