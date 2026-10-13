#include "startup_rules.hpp"
#include "test_support.hpp"

#include <string>

namespace {
bool is_ascii(const std::string &text) {
    for (const unsigned char c : text)
        if (c > 0x7Fu) return false;
    return true;
}
} // namespace

int main() {
    using vcsnx::StartupFacts;
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"application mode with data starts", [] {
             StartupFacts facts;
             require(!vcsnx::startup_problem(facts).has_value(), "should start");
         }},
        {"applet mode explains how to launch, with the memory figure", [] {
             StartupFacts facts;
             facts.application_mode = false;
             facts.total_memory_mb = 442;
             const auto message = vcsnx::startup_problem(facts);
             require(message.has_value(), "applet must stop");
             require(message->find("Sphaira") != std::string::npos && message->find(" R ") != std::string::npos, "how to launch");
             require(message->find("442 MB") != std::string::npos, "memory figure");
             require(is_ascii(*message), "libnx console prints ASCII only");
         }},
        {"missing PSP_DATA says where to copy it", [] {
             StartupFacts facts;
             facts.executable = vcsnx::ExecutableState::Missing;
             const auto message = vcsnx::startup_problem(facts);
             require(message && message->find("sdmc:/switch/vcsnx/PSP_DATA") != std::string::npos, "path");
             require(is_ascii(*message), "ascii");
         }},
        {"encrypted executable explains how to add the decrypted one", [] {
             StartupFacts facts;
             facts.executable = vcsnx::ExecutableState::Encrypted;
             const auto message = vcsnx::startup_problem(facts);
             require(message && message->find("encrypted") != std::string::npos, "says encrypted");
             require(message->find("EBOOT_DECRYPTED.ELF") != std::string::npos, "names the decrypted file");
             require(is_ascii(*message), "ascii");
         }},
        {"executable that is not an ELF asks to copy PSP_DATA again", [] {
             StartupFacts facts;
             facts.executable = vcsnx::ExecutableState::NotElf;
             const auto message = vcsnx::startup_problem(facts);
             require(message && message->find("not a PSP ELF") != std::string::npos, "not elf");
             require(is_ascii(*message), "ascii");
         }},
        {"missing boot movies report an incomplete copy", [] {
             StartupFacts facts;
             facts.boot_movies_present = false;
             const auto message = vcsnx::startup_problem(facts);
             require(message && message->find("LOGO.PMF") != std::string::npos, "names the movies");
             require(is_ascii(*message), "ascii");
         }},
        {"executable header classification", [] {
             const unsigned char elf[] = {0x7F, 'E', 'L', 'F', 1};
             const unsigned char psp[] = {'~', 'P', 'S', 'P', 0};
             const unsigned char junk[] = {0, 0, 0, 0};
             require(vcsnx::classify_executable(elf, sizeof(elf)) == vcsnx::ExecutableState::Ok, "elf");
             require(vcsnx::classify_executable(psp, sizeof(psp)) == vcsnx::ExecutableState::Encrypted, "psp");
             require(vcsnx::classify_executable(junk, sizeof(junk)) == vcsnx::ExecutableState::NotElf, "junk");
             require(vcsnx::classify_executable(elf, 2) == vcsnx::ExecutableState::NotElf, "short read");
         }},
        {"Vulkan build defaults: GE on its own core, no raster helper threads, GPU vertex transform", [] {
             // Console runs 3-4: an async GE was slower (the game waits on it every frame) and raster
             // helpers spin on cores 0 and 2 after each job (Plan 2 review, Important 1-2).
             const auto env = vcsnx::default_environment(true);
             const auto value = [&env](const std::string &key) {
                 for (const auto &[k, v] : env) if (k == key) return v;
                 return std::string("<unset>");
             };
             // With the GPU vertex transform the GE (~25 ms) and the game (~28 ms) are comparable, so
             // overlapping them on cores 1 and 2 won (console run 8: 20-27 vs 11-25 vblanks/s).
             require(value("PSPRECOMP_GE_ASYNC") == "1", "GE on its own core");
             require(value("PSPRECOMP_RASTER_THREADS") == "1", "no raster helpers");
             require(value("PSPRECOMP_FRAME_TIME_DIAG") == "1", "perf.log stays on");
             // Plan 3 (H7): the GPU transforms vertices, ~4x less GE time on the console.
             require(value("PSPRECOMP_GE_GPU_HW_TRANSFORM") == "1", "GPU vertex transform on");
         }},
        {"CPU-only build keeps three raster threads and the vendor's GE default", [] {
             const auto env = vcsnx::default_environment(false);
             bool raster3 = false, ge_async = false;
             for (const auto &[k, v] : env) {
                 raster3 = raster3 || (k == "PSPRECOMP_RASTER_THREADS" && v == "3");
                 ge_async = ge_async || k == "PSPRECOMP_GE_ASYNC";
             }
             bool hw_transform = false;
             for (const auto &[k, v] : env) hw_transform = hw_transform || k == "PSPRECOMP_GE_GPU_HW_TRANSFORM";
             require(raster3 && !ge_async && !hw_transform, "software rasterizer still gets its helpers");
         }},
        {"env file: KEY=VALUE lines, comments and blanks skipped, CR and spaces trimmed", [] {
             const auto pairs = vcsnx::parse_env_lines(
                 "# frame dumps\r\nPSPRECOMP_FRAME_DUMP_START = 600\r\n\r\nnot a pair\n=empty\nA=b=c\n");
             require(pairs.size() == 2u, "pair count");
             require(pairs[0].first == "PSPRECOMP_FRAME_DUMP_START" && pairs[0].second == "600", "first pair");
             require(pairs[1].first == "A" && pairs[1].second == "b=c", "value keeps later '='");
         }},
        {"the game module messages are ASCII and send the player to the Builder", [] {
             const std::string messages[] = {vcsnx::module_missing_message(), vcsnx::module_mismatch_message(),
                                             vcsnx::module_load_message("missing import: memcpy")};
             for (const std::string &message : messages) {
                 require(message.find("VCSNX Builder") != std::string::npos, "names the Builder");
                 for (const char c : message) require(static_cast<unsigned char>(c) < 128, "ASCII only");
             }
             require(messages[0].find("vcs_game.so") != std::string::npos, "says which file is missing");
             require(messages[2].find("missing import: memcpy") != std::string::npos, "includes the loader's reason");
             require(vcsnx::module_load_message("caf\xc3\xa9").find("caf??") != std::string::npos, "non-ASCII replaced");
         }},
    });
}
