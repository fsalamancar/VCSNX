// modprobe.nro: proves on the console that the game-module loader works before the game depends on it
// (spec §6.2): loads sdmc:/switch/vcsnx/modtest.so, calls it, and throws both ways across it. Prints PASS
// or FAIL per check and writes sdmc:/switch/vcsnx/modprobe.log.
#include "game_module.hpp"

#include <switch.h>

#include <array>
#include <cstdarg>
#include <cstdio>
#include <stdexcept>
#include <string>

extern "C" const vcsnx::RawImport vcsnx_module_imports[];

int modprobe_host_add(int a, int b) { return a + b; }
void modprobe_host_throw(int code) { throw std::runtime_error("from the runtime " + std::to_string(code)); }

namespace {
std::FILE *g_log = nullptr;
int g_failures = 0;

void say(const char *format, ...) {
    va_list args;
    va_start(args, format);
    char line[512];
    std::vsnprintf(line, sizeof line, format, args);
    va_end(args);
    std::printf("%s\n", line);
    if (g_log != nullptr) {
        std::fprintf(g_log, "%s\n", line);
        std::fflush(g_log); // a crash later in the probe must not lose the lines before it
    }
    consoleUpdate(nullptr);
}

void check(bool ok, const std::string &what) {
    say("%s %s", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++g_failures;
}
} // namespace

int main() {
    consoleInit(nullptr);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    g_log = std::fopen("sdmc:/switch/vcsnx/modprobe.log", "w");
    say("VCSNX module probe");
    std::string error;
    constexpr std::array<std::string_view, 5> exports{"modtest_add", "modtest_catch_host_throw", "modtest_throw", "modtest_abi",
                                                      "modtest_cleanup_through"};
    const auto imports = vcsnx::module_imports(vcsnx_module_imports);
    const auto module = vcsnx::load_module("sdmc:/switch/vcsnx/modtest.so", imports, exports, error);
    check(module.has_value(), module ? "load modtest.so" : "load modtest.so: " + error);
    if (module) {
        say("base 0x%llx, %llu KB, %zu imports", static_cast<unsigned long long>(module->base),
            static_cast<unsigned long long>(module->size >> 10), imports.size());
        check(reinterpret_cast<int (*)(int, int)>(module->symbols[0])(2, 3) == 10, "module calls the runtime");
        check(reinterpret_cast<int (*)()>(module->symbols[1])() == 101, "runtime exception caught in the module, destructor run");
        bool caught = false;
        try {
            reinterpret_cast<void (*)()>(module->symbols[2])();
        } catch (const std::runtime_error &e) {
            caught = std::string(e.what()) == "from the module";
        }
        check(caught, "module exception caught in the runtime");
        check(std::string(static_cast<const char *>(module->symbols[3])) == "modprobe-1", "module data read");
        int cleanups = 0;
        bool through = false;
        try {
            reinterpret_cast<void (*)(int *)>(module->symbols[4])(&cleanups);
        } catch (const std::runtime_error &) {
            through = cleanups == 1;
        }
        check(through, "runtime exception through a module cleanup, caught in the runtime");
    }
    if (g_failures == 0) say("ALL PASS");
    else say("%d FAILED", g_failures);
    if (g_log != nullptr) std::fclose(g_log);
    say("Press + to exit.");
    while (appletMainLoop()) {
        padUpdate(&pad);
        if ((padGetButtonsDown(&pad) & HidNpadButton_Plus) != 0u) break;
        consoleUpdate(nullptr);
    }
    consoleExit(nullptr);
    return 0;
}
