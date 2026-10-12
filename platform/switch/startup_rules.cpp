#include "startup_rules.hpp"

namespace vcsnx {
namespace {

std::string_view trim(std::string_view text) {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) return {};
    const auto last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1u);
}

} // namespace

std::optional<std::string> startup_problem(const StartupFacts &facts) {
    if (!facts.application_mode) {
        return "VCSNX needs the full console memory (application mode).\n"
               "Open it from its Sphaira forwarder on the HOME menu, or open\n"
               "hbmenu/Sphaira by holding R while starting a game.\n"
               "Memory available in this mode: " + std::to_string(facts.total_memory_mb) + " MB.\n\n"
               "Press + to exit.";
    }
    switch (facts.executable) {
    case ExecutableState::Ok:
        break;
    case ExecutableState::Missing:
        return "Game data not found.\n"
               "Copy the PSP_DATA folder of your copy of the game to\n"
               "sdmc:/switch/vcsnx/PSP_DATA (see tools/prepare_sd.sh).\n\n"
               "Press + to exit.";
    case ExecutableState::Encrypted:
        return "The game executable in sdmc:/switch/vcsnx/PSP_DATA is encrypted\n"
               "(the UMD's EBOOT.BIN). Add a decrypted copy as\n"
               "PSP_DATA/PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF.\n\n"
               "Press + to exit.";
    case ExecutableState::NotElf:
        return "The game executable in sdmc:/switch/vcsnx/PSP_DATA is not a PSP ELF\n"
               "file (damaged or incomplete copy). Copy PSP_DATA again.\n\n"
               "Press + to exit.";
    }
    if (!facts.boot_movies_present) {
        return "PSP_DATA is incomplete: PSP_GAME/USRDIR/RUNDATA/PSP/MOVIES/\n"
               "LOGO.PMF or TITLES.PMF is missing. Copy the whole PSP_DATA folder again.\n\n"
               "Press + to exit.";
    }
    return std::nullopt;
}

ExecutableState classify_executable(const unsigned char *head, std::size_t size) noexcept {
    if (head == nullptr || size < 4u) return ExecutableState::NotElf;
    if (head[0] == 0x7Fu && head[1] == 'E' && head[2] == 'L' && head[3] == 'F') return ExecutableState::Ok;
    if (head[0] == '~' && head[1] == 'P' && head[2] == 'S' && head[3] == 'P') return ExecutableState::Encrypted;
    return ExecutableState::NotElf;
}

std::vector<std::pair<std::string, std::string>> parse_env_lines(std::string_view text) {
    std::vector<std::pair<std::string, std::string>> pairs;
    while (!text.empty()) {
        const std::size_t newline = text.find('\n');
        const std::string_view line = trim(text.substr(0, newline));
        text = newline == std::string_view::npos ? std::string_view{} : text.substr(newline + 1u);
        if (line.empty() || line.front() == '#') continue;
        const std::size_t equals = line.find('=');
        if (equals == std::string_view::npos) continue;
        const std::string_view key = trim(line.substr(0, equals));
        if (key.empty()) continue;
        pairs.emplace_back(std::string(key), std::string(trim(line.substr(equals + 1u))));
    }
    return pairs;
}

std::vector<std::pair<std::string, std::string>> default_environment(bool vulkan_backend) {
    std::vector<std::pair<std::string, std::string>> env{{"PSPRECOMP_FRAME_TIME_DIAG", "1"}};
    if (vulkan_backend) {
        // With the GPU vertex transform the GE and the game cost about the same, so the GE runs on
        // its own core (2) beside the game (core 1); console run 8: 20-27 vs 11-25 vblanks/s.
        env.emplace_back("PSPRECOMP_GE_ASYNC", "1");
        env.emplace_back("PSPRECOMP_RASTER_THREADS", "1");
        // Plan 3 (H7): the GPU transforms, clips and assembles 3D vertices (~4x less GE time).
        env.emplace_back("PSPRECOMP_GE_GPU_HW_TRANSFORM", "1");
    } else {
        env.emplace_back("PSPRECOMP_RASTER_THREADS", "3");
    }
    return env;
}

} // namespace vcsnx
