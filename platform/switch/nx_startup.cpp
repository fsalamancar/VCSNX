// Switch-only startup (spec §5, §7): nxlink stdio, diagnostics defaults, optional vcsnx_env.txt,
// and the pre-main checks that stop with a readable message instead of a silent exit.

#include "cpu_clock.hpp"
#include "memory_usage.hpp"
#include "startup_rules.hpp"
#include "vcs_bootstrap_paths.hpp"
#include "vcsnx_platform.hpp"

#include <netinet/in.h>
#include <switch.h>
#include <sys/stat.h>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

extern "C" void userAppInit(void) {
    if (R_SUCCEEDED(socketInitializeDefault()) && __nxlink_host.s_addr != 0) nxlinkStdio();
}

extern "C" void userAppExit(void) {
    socketExit();
}

namespace {

bool file_exists(const std::string &path) {
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

std::string read_text_file(const std::string &path) {
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) return {};
    std::string text;
    char buffer[512];
    std::size_t count = 0;
    while ((count = std::fread(buffer, 1, sizeof(buffer), file)) > 0) text.append(buffer, count);
    std::fclose(file);
    return text;
}

[[noreturn]] void show_message_and_exit(const std::string &message) {
    consoleInit(nullptr);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    std::printf("\n%s\n", message.c_str());
    while (appletMainLoop()) {
        padUpdate(&pad);
        if ((padGetButtonsDown(&pad) & HidNpadButton_Plus) != 0u) break;
        consoleUpdate(nullptr);
    }
    consoleExit(nullptr);
    std::exit(0);
}

// Runs during static initialisation, before main() and before any vendor code reads the
// environment (the vendor reads PSPRECOMP_* lazily on first use).
struct StartupChecks {
    StartupChecks() {
        const std::string data = vcsnx::data_directory();
        for (const auto &[key, value] : vcsnx::parse_env_lines(read_text_file(data + "/vcsnx_env.txt")))
            setenv(key.c_str(), value.c_str(), 1);
#if defined(VCSNX_GPU_VULKAN)
        constexpr bool kVulkanBackend = true;
#else
        constexpr bool kVulkanBackend = false;
#endif
        for (const auto &[key, value] : vcsnx::default_environment(kVulkanBackend))
            setenv(key.c_str(), value.c_str(), 0);
        // The fork writes generated files (the Switch-button ENGLISH.GXT) under
        // std::filesystem::temp_directory_path(), which reads TMPDIR and needs an existing folder.
        const std::string temp = data + "/tmp";
        mkdir(temp.c_str(), 0777);
        setenv("TMPDIR", temp.c_str(), 0);

        vcsnx::StartupFacts facts;
        const AppletType type = appletGetAppletType();
        facts.application_mode = type == AppletType_Application || type == AppletType_SystemApplication;
        facts.total_memory_mb = vcsnx::memory_total_mb();
        // Ask the vendor's own bootstrap which file it will load, so this check can never disagree
        // with what main() then does (review finding: a separate file list missed the encrypted
        // SYSDIR/EBOOT.BIN, partial copies and the boot movies).
        facts.executable = vcsnx::ExecutableState::Missing;
        try {
            const char *const argv0[] = {"vcsnx"};
            const vcs::BootstrapPaths paths = vcs::resolve_bootstrap_paths(1, argv0, data);
            unsigned char head[4]{};
            std::size_t head_size = 0;
            if (std::FILE *elf = std::fopen(paths.psp_executable.string().c_str(), "rb")) {
                head_size = std::fread(head, 1, sizeof(head), elf);
                std::fclose(elf);
                facts.executable = vcsnx::classify_executable(head, head_size);
            }
            const std::string movies = paths.game_root.string() + "/PSP_GAME/USRDIR/RUNDATA/PSP/MOVIES/";
            facts.boot_movies_present = file_exists(movies + "LOGO.PMF") && file_exists(movies + "TITLES.PMF");
        } catch (const std::exception &) {
            facts.executable = vcsnx::ExecutableState::Missing;
        }
        if (const auto problem = vcsnx::startup_problem(facts)) show_message_and_exit(*problem);

        // Without nxlink nothing reads stdout/stderr, and the stderr tee only starts with the
        // window; everything printed before that (bootstrap errors, "VCSNative error: ...")
        // would be lost. Send both to the SD, unbuffered, until the process exits.
        if (__nxlink_host.s_addr == 0) {
            const std::string boot_log = data + "/vcsnx_boot.log";
            if (std::FILE *truncate = std::fopen(boot_log.c_str(), "w")) std::fclose(truncate);
            // Both streams append, so neither overwrites what the other wrote.
            if (std::freopen(boot_log.c_str(), "a", stderr) != nullptr) std::setvbuf(stderr, nullptr, _IONBF, 0);
            if (std::freopen(boot_log.c_str(), "a", stdout) != nullptr) std::setvbuf(stdout, nullptr, _IONBF, 0);
        }
        // Opt-in CPU clock (VCSNX_CPU_MHZ in vcsnx_env.txt, e.g. 1785); restored at exit.
        vcsnx::apply_cpu_clock_boost();
    }
};

const StartupChecks g_startup_checks;

} // namespace
