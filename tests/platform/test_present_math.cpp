#include "present_math.hpp"

#include <array>
#include <cstddef>
#include <vector>
#include "test_support.hpp"

namespace {
bool same(vcsnx::Rect a, vcsnx::Rect b) { return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; }
} // namespace

int main() {
    using vcsnx::letterbox;
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"handheld 1280x720 keeps 480x272 proportions, centred", [] {
             require(same(letterbox(1280, 720, 480u, 272u), {5, 0, 1270, 720}), "handheld");
         }},
        {"docked 1920x1080", [] {
             require(same(letterbox(1920, 1080, 480u, 272u), {7, 0, 1905, 1080}), "docked");
         }},
        {"exact 2x fills the output", [] {
             require(same(letterbox(960, 544, 480u, 272u), {0, 0, 960, 544}), "2x");
         }},
        {"taller output gets bars top and bottom", [] {
             require(same(letterbox(480, 480, 480u, 272u), {0, 104, 480, 272}), "tall");
         }},
        {"RGBA readback bytes copy straight into SDL ABGR8888 words", [] {
             // ABGR8888 keeps R in the low byte on little-endian, i.e. the bytes in RGBA order, so
             // the GPU readback needs a copy, not a per-pixel swizzle on the guest thread.
             const std::array<std::byte, 8> rgba{std::byte{0x11}, std::byte{0x22}, std::byte{0x33}, std::byte{0x44},
                                                 std::byte{0xAA}, std::byte{0xBB}, std::byte{0xCC}, std::byte{0xDD}};
             std::vector<std::uint32_t> words;
             vcsnx::copy_rgba_to_abgr8888(rgba, 2u, words);
             require(words.size() == 2u && words[0] == 0x44332211u && words[1] == 0xDDCCBBAAu, "words");
         }},
        {"empty sizes give an empty rectangle", [] {
             require(same(letterbox(0, 720, 480u, 272u), {0, 0, 0, 0}), "no output");
             require(same(letterbox(1280, 720, 0u, 272u), {0, 0, 0, 0}), "no source");
         }},
    });
}
