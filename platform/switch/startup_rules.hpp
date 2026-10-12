#pragma once
// Pure startup rules (spec §5): when the game must not start, and the dev env file format.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vcsnx {

// What the vendor's bootstrap would load from PSP_DATA (vcs::resolve_bootstrap_paths picks it).
enum class ExecutableState {
    Ok,        // a decrypted PSP ELF
    Missing,   // no candidate file at all
    Encrypted, // the UMD's EBOOT.BIN ("~PSP" header): the vendor cannot run it
    NotElf,    // damaged or incomplete copy
};

struct StartupFacts {
    bool application_mode{true}; // false in applet mode (launched from the Album)
    std::uint64_t total_memory_mb{};
    ExecutableState executable{ExecutableState::Ok};
    bool boot_movies_present{true}; // LOGO.PMF and TITLES.PMF, required by validate_vcs_game_root
};

// Classifies the first bytes of the chosen executable.
[[nodiscard]] ExecutableState classify_executable(const unsigned char *head, std::size_t size) noexcept;

// The ASCII message to show before exiting, or nullopt when the game can start.
// (The libnx console only renders ASCII.)
[[nodiscard]] std::optional<std::string> startup_problem(const StartupFacts &facts);

// Parses sdmc:/switch/vcsnx/vcsnx_env.txt: one KEY=VALUE per line, '#' comments and blank lines
// skipped, spaces and CR trimmed, lines without '=' or with an empty key ignored.
[[nodiscard]] std::vector<std::pair<std::string, std::string>> parse_env_lines(std::string_view text);

// Environment defaults nx_startup.cpp applies with setenv(..., 0), so vcsnx_env.txt still wins.
// With the Vulkan backend: the GPU vertex transform on (Plan 3), display lists on their own core
// beside the game (with the GPU transform both cost about the same, so they overlap), and no raster
// helper threads (they spin on cores 0 and 2 after every job while the CPU rasterizer is idle).
// The CPU-only build keeps 3 raster threads.
[[nodiscard]] std::vector<std::pair<std::string, std::string>> default_environment(bool vulkan_backend);

} // namespace vcsnx
