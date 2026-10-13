// Switch-only startup (spec §5, §7): nxlink stdio, diagnostics defaults, optional vcsnx_env.txt,
// and the pre-main checks that stop with a readable message instead of a silent exit.

#include "alloc_diag.hpp"
#include "cpu_clock.hpp"
#include "ram_files.hpp"
#include "crash_log.hpp"
#include "log_files.hpp"
#include "memory_usage.hpp"
#include "memory_watch.hpp"
#include "startup_rules.hpp"
#include "vcs_bootstrap_paths.hpp"
#include "vcsnx_platform.hpp"
#if defined(VCSNX_GAME_MODULE)
#include "game_module.hpp"
extern "C" const vcsnx::RawImport vcsnx_module_imports[];
#endif

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

#if defined(VCSNX_GAME_MODULE)
// Where vcs_game.so landed, logged once stderr goes to the SD: a crash report's addresses outside vcsnx.nro
// are base + an offset in vcs_game.so.
std::uint64_t game_module_base = 0;
std::uint64_t game_module_size = 0;

// Loads sdmc:/switch/vcsnx/vcs_game.so before main(), while the libnx console can still show a message.
void load_game_module_or_exit(const std::string &data) {
    const std::string path = data + "/vcs_game.so";
    if (!file_exists(path)) show_message_and_exit(vcsnx::module_missing_message());
    std::string error;
    constexpr std::string_view kExports[] = {"vcsnx_register_generated_functions", "vcsnx_module_abi"};
    const auto module = vcsnx::load_module(path, vcsnx::module_imports(vcsnx_module_imports), kExports, error);
    if (!module) show_message_and_exit(vcsnx::module_load_message(error));
    if (std::string_view(static_cast<const char *>(module->symbols[1])) != VCSNX_MODULE_ABI)
        show_message_and_exit(vcsnx::module_mismatch_message());
    vcsnx::set_game_module_entry(module->symbols[0]);
    game_module_base = module->base;
    game_module_size = module->size;
}
#endif

// Runs during static initialisation, before main() and before any vendor code reads the
// environment (the vendor reads PSPRECOMP_* lazily on first use).
struct StartupChecks {
    StartupChecks() {
        const std::string data = vcsnx::data_directory();
        for (const auto &[key, value] : vcsnx::parse_env_lines(read_text_file(data + "/vcsnx_env.txt")))
            setenv(key.c_str(), value.c_str(), 1);
        vcsnx::set_alloc_diag(std::getenv("VCSNX_ALLOC_DIAG")); // heap census (alloc_diag.hpp), off by default
        vcsnx::set_block_cache(std::getenv("VCSNX_BLOCK_CACHE")); // big-block reuse (block_cache.hpp), on by default
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
            // The city's streaming archives into RAM in the background (ram_files.hpp); VCSNX_RAM_STREAMING=0
            // keeps reading them from the SD.
            if (vcsnx::ram_streaming_enabled(std::getenv("VCSNX_RAM_STREAMING"))) {
                const std::filesystem::path rundata = paths.game_root / "PSP_GAME" / "USRDIR" / "RUNDATA";
                vcsnx::ram_files().start({rundata / "MAINLA.IMG", rundata / "BEACH.IMG", rundata / "GTA3PSP.IMG"});
            }
        } catch (const std::exception &) {
            facts.executable = vcsnx::ExecutableState::Missing;
        }
        if (const auto problem = vcsnx::startup_problem(facts)) show_message_and_exit(*problem);
#if defined(VCSNX_GAME_MODULE)
        load_game_module_or_exit(data);
#endif

        // Without nxlink nothing reads stdout/stderr, and the stderr tee only starts with the
        // window; everything printed before that (bootstrap errors, "VCSNative error: ...")
        // would be lost. Send both to the SD, unbuffered, until the process exits.
        if (__nxlink_host.s_addr == 0) {
            const std::string boot_log = data + "/vcsnx_boot.log";
            vcsnx::keep_previous_log(boot_log); // the last session's, for a crash or hang
            if (std::FILE *truncate = std::fopen(boot_log.c_str(), "w")) std::fclose(truncate);
            // Both streams append, so neither overwrites what the other wrote.
            if (std::freopen(boot_log.c_str(), "a", stderr) != nullptr) std::setvbuf(stderr, nullptr, _IONBF, 0);
            if (std::freopen(boot_log.c_str(), "a", stdout) != nullptr) std::setvbuf(stdout, nullptr, _IONBF, 0);
        }
        vcsnx::log_memory_layout();
#if defined(VCSNX_GAME_MODULE)
        vcsnx::set_crash_log_context(data.c_str(), game_module_base, game_module_size); // vcsnx_crash.log
        std::fprintf(stderr, "[vcsnx-module] vcs_game.so base=0x%llx size=%llu KiB\n",
                     static_cast<unsigned long long>(game_module_base), static_cast<unsigned long long>(game_module_size >> 10));
#endif
        // Opt-in CPU clock (VCSNX_CPU_MHZ in vcsnx_env.txt, e.g. 1785); restored at exit.
        vcsnx::apply_cpu_clock_boost();
    }
};

const StartupChecks g_startup_checks;

} // namespace
