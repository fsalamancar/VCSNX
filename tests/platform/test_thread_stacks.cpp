// Backlog bugs 1 and 3: after mission 2 the game could not create its "sfx bank load thread"
// (sceKernelCreateThread -> 0x80020190, no memory) and waited forever on a black or loading screen.
// Every thread that ended with sceKernelExitDeleteThread had kept its stack.
#include "psprecomp/runtime.hpp"
#include "test_support.hpp"
#include "vcs_profile.hpp"

#include <cstring>
#include <string>

namespace {
using vcsnx_test::require;

constexpr std::uint32_t kCreateThread = 0x446D8DE6u;
constexpr std::uint32_t kStartThread = 0xF475845Du;
constexpr std::uint32_t kExitDeleteThread = 0x809CE29Bu;
constexpr std::uint32_t kDeleteThread = 0x9FA03CD3u;
constexpr std::uint32_t kMaxFreeMemSize = 0xA291F107u;
constexpr std::uint32_t kName = 0x08800000u;
constexpr std::uint32_t kEntry = 0x08800100u;

struct Game {
    psprecomp::Runtime runtime{64u * 1024u * 1024u};
    psprecomp::AllegrexContext ctx{};

    Game() {
        vcs::install_profile(runtime, 0x08A00000u);
        const char name[] = "sfx bank load thread";
        for (std::size_t i = 0; i < sizeof name; ++i) runtime.memory().store8(kName + static_cast<std::uint32_t>(i), static_cast<std::uint8_t>(name[i]));
    }

    std::uint32_t call(const char *library, std::uint32_t nid, std::uint32_t a0 = 0, std::uint32_t a1 = 0,
                       std::uint32_t a2 = 0, std::uint32_t a3 = 0) {
        ctx.gpr[4] = a0;
        ctx.gpr[5] = a1;
        ctx.gpr[6] = a2;
        ctx.gpr[7] = a3;
        ctx.gpr[8] = 0u; // attributes
        ctx.gpr[9] = 0u; // options
        runtime.invoke_import(library, nid, ctx);
        return ctx.gpr[2];
    }

    std::uint32_t free_memory() { return call("SysMemUserForUser", kMaxFreeMemSize); }

    // The game's pattern: create a higher-priority worker, start it (it takes the CPU), let it end itself.
    std::uint32_t run_one_worker() {
        const std::uint32_t uid = call("ThreadManForUser", kCreateThread, kName, kEntry, 0x11u, 0x800u);
        require(static_cast<std::int32_t>(uid) > 0, "CreateThread failed: " + std::to_string(uid));
        call("ThreadManForUser", kStartThread, uid);
        call("ThreadManForUser", kExitDeleteThread, 0u);
        return uid;
    }
};
} // namespace

int main() {
    return vcsnx_test::run({
        {"a thread that ends with ExitDeleteThread gives its stack back", [] {
             Game game;
             const std::uint32_t before = game.free_memory();
             for (int i = 0; i < 1000; ++i) (void)game.run_one_worker();
             const std::uint32_t after = game.free_memory();
             require(after == before, "free memory " + std::to_string(before) + " -> " + std::to_string(after));
         }},
        {"and is gone, as on the PSP", [] {
             Game game;
             const std::uint32_t uid = game.run_one_worker();
             require(game.call("ThreadManForUser", kDeleteThread, uid) == 0x80020198u, "the thread must be unknown after ExitDeleteThread");
         }},
    });
}
