// psprecomp::Runtime::set_game_root() must not abort startup when the root cannot be
// canonicalized: on the Switch, newlib fails weakly_canonical("sdmc:/...") with EINVAL. A folder
// without search permission makes the Mac fail the same call, so the behaviour is testable here.
#include "psprecomp/runtime.hpp"
#include "test_support.hpp"

#include <chrono>
#include <filesystem>
#include <string>
#include <sys/stat.h>

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"uncanonicalizable game root is kept, lexically normalized", [] {
             namespace fs = std::filesystem;
             const fs::path base = fs::temp_directory_path() /
                 ("vcsnx_root_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
             const fs::path locked = base / "locked";
             fs::create_directories(locked);
             ::chmod(locked.c_str(), 0); // status() below it now fails with EACCES
             const fs::path requested = locked / "a" / ".." / "PSP_DATA";
             std::string failure;
             psprecomp::Runtime runtime(32u * 1024u * 1024u);
             try {
                 runtime.set_game_root(requested);
             } catch (const std::exception &error) {
                 failure = error.what();
             }
             ::chmod(locked.c_str(), 0700);
             fs::remove_all(base);
             require(failure.empty(), "set_game_root threw: " + failure);
             require(runtime.game_root() == requested.lexically_normal(),
                     "kept root " + runtime.game_root().string());
         }},
    });
}
