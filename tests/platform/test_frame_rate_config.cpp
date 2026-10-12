// [Timing] FrameRate: the fork locks it to 60 (its unlocked frame-limiter patch). VCSNX also accepts
// 30, the PSP's own rate, where the game keeps its original limiter and renders every other vblank:
// half the GE and guest work per second on the Switch.
#include "test_support.hpp"
#include "vcs_config.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace {
std::uint32_t frame_rate_for(const std::string &value) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "vcsnx_frame_rate_test";
    std::filesystem::create_directories(root);
    const std::filesystem::path path = root / "VCSNative.ini";
    {
        std::ofstream output(path, std::ios::trunc);
        output << "[Timing]\nFrameRate=" << value << "\n";
    }
    return vcs::load_vcs_configuration(path).timing.frame_rate;
}
} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"FrameRate=30 keeps the PSP's own 30 FPS", [] { require(frame_rate_for("30") == 30u, "30"); }},
        {"FrameRate=60 stays 60", [] { require(frame_rate_for("60") == 60u, "60"); }},
        {"any other value falls back to 60", [] {
             require(frame_rate_for("45") == 60u, "45");
             require(frame_rate_for("fast") == 60u, "text");
         }},
    });
}
