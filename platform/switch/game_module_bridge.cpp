// On the Switch the game's translated code lives in vcs_game.so: main.cpp's call to
// register_generated_functions lands here and goes on into the module that nx_startup loaded (spec §2.4).
#include "game_module.hpp"
#include "psprecomp/common.hpp"
#include "psprecomp/runtime.hpp"

namespace vcsnx {
namespace {
void (*g_module_entry)(psprecomp::Runtime *) = nullptr;
} // namespace

void set_game_module_entry(void *entry) noexcept {
    g_module_entry = reinterpret_cast<void (*)(psprecomp::Runtime *)>(entry);
}
} // namespace vcsnx

namespace psprecomp {
void register_generated_functions(Runtime &runtime) {
    if (vcsnx::g_module_entry == nullptr) throw Error("vcs_game.so is not loaded");
    vcsnx::g_module_entry(&runtime);
}
} // namespace psprecomp
