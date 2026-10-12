// The media decoders hand paths to FFmpeg's avformat_open_input(), which reads "name:" at the
// start of a path as a URL protocol. Switch paths look like "sdmc:/switch/...", so every movie
// failed with "Protocol not found" and the game waited forever for its logo video. A folder
// named "vcsnxdev:" reproduces the same path shape on the Mac.
#include "test_support.hpp"
#include "vcs_media_decoder.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

void put_le(std::ofstream &out, std::uint32_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) out.put(static_cast<char>((value >> (8 * i)) & 0xFFu));
}

// 0.1 s of 44.1 kHz stereo silence as a minimal RIFF/WAVE file.
void write_wav(const std::filesystem::path &path) {
    constexpr std::uint32_t kFrames = 4410u;
    constexpr std::uint32_t kDataBytes = kFrames * 4u;
    std::ofstream out(path, std::ios::binary);
    out.write("RIFF", 4); put_le(out, 36u + kDataBytes, 4); out.write("WAVE", 4);
    out.write("fmt ", 4); put_le(out, 16u, 4); put_le(out, 1u, 2); put_le(out, 2u, 2);
    put_le(out, 44100u, 4); put_le(out, 44100u * 4u, 4); put_le(out, 4u, 2); put_le(out, 16u, 2);
    out.write("data", 4); put_le(out, kDataBytes, 4);
    const std::vector<char> silence(kDataBytes, 0);
    out.write(silence.data(), static_cast<std::streamsize>(silence.size()));
}

} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"a path whose first component ends in ':' opens as a file", [] {
             namespace fs = std::filesystem;
             const fs::path base = fs::temp_directory_path() /
                 ("vcsnx_media_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
             fs::create_directories(base / "vcsnxdev:");
             write_wav(base / "vcsnxdev:" / "tone.wav");
             const fs::path previous = fs::current_path();
             fs::current_path(base);
             vcs::AudioStreamDecoder decoder;
             const bool opened = decoder.open("vcsnxdev:/tone.wav", 44100u, 2u, 0u);
             decoder.close();
             fs::current_path(previous);
             fs::remove_all(base);
             require(opened, "AudioStreamDecoder could not open vcsnxdev:/tone.wav");
         }},
    });
}
