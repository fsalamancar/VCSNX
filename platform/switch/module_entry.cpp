// The game module's entry points (spec §2.2), compiled into vcs_game.so with the translated code: the
// runtime's loader finds them by name. Our code, nothing of the game.
#include "psprecomp/runtime.hpp"

extern "C" __attribute__((visibility("default"))) void vcsnx_register_generated_functions(psprecomp::Runtime *runtime) {
    psprecomp::register_generated_functions(*runtime);
}

extern "C" __attribute__((visibility("default"))) const char vcsnx_module_abi[] = VCSNX_MODULE_ABI;
