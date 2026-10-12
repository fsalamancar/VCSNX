// [Display] HudScale (VCSNX): how big the in-game HUD is, 0.5 to 1. 1 is the game's own size and the
// default; anything outside the range keeps 1.
#include "test_support.hpp"
#include "vcs_config.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace {
float hud_scale_for(const std::string &line) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "vcsnx_hud_scale_test";
    std::filesystem::create_directories(root);
    const std::filesystem::path path = root / "VCSNative.ini";
    {
        std::ofstream output(path, std::ios::trunc);
        output << "[Display]\n" << line << "\n";
    }
    return vcs::load_vcs_configuration(path).display.hud_scale;
}
} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"HudScale=0.75 makes the HUD three quarters size", [] { require(hud_scale_for("HudScale=0.75") == 0.75f, "0.75"); }},
        {"without the key the HUD keeps the game's size", [] { require(hud_scale_for("Enabled=true") == 1.0f, "default"); }},
        {"values outside 0.5-1 keep the game's size", [] {
             require(hud_scale_for("HudScale=0.2") == 1.0f, "0.2");
             require(hud_scale_for("HudScale=1.5") == 1.0f, "1.5");
             require(hud_scale_for("HudScale=small") == 1.0f, "text");
         }},
    });
}
