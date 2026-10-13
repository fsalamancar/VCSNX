#include "log_files.hpp"
#include "test_support.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <unistd.h>

namespace {
using vcsnx_test::require;
namespace fs = std::filesystem;

fs::path temp_dir(const char *name) {
    const fs::path dir = fs::temp_directory_path() / ("vcsnx-log-files-" + std::string(name) + "-" + std::to_string(::getpid()));
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

void write(const fs::path &path, const std::string &text) { std::ofstream(path) << text; }

std::string read(const fs::path &path) {
    std::ifstream in(path);
    return std::string(std::istreambuf_iterator<char>(in), {});
}
} // namespace

int main() {
    return vcsnx_test::run({
        {"the last session's log is kept as .prev.log", [] {
             const fs::path dir = temp_dir("keep");
             write(dir / "vcsnx_boot.log", "the session that crashed");
             vcsnx::keep_previous_log((dir / "vcsnx_boot.log").string());
             require(read(dir / "vcsnx_boot.prev.log") == "the session that crashed", "kept content");
             require(!fs::exists(dir / "vcsnx_boot.log"), "the current log is moved away");
         }},
        {"an older .prev.log is replaced, and a missing log keeps it", [] {
             const fs::path dir = temp_dir("replace");
             write(dir / "a.prev.log", "older");
             write(dir / "a.log", "newer");
             vcsnx::keep_previous_log((dir / "a.log").string());
             require(read(dir / "a.prev.log") == "newer", "replaced by the newer session");
             vcsnx::keep_previous_log((dir / "a.log").string());
             require(read(dir / "a.prev.log") == "newer", "nothing to keep must not wipe the kept log");
         }},
        {"the kept name goes before .log", [] {
             require(vcsnx::previous_log_path("sdmc:/switch/vcsnx/vcsnx_stderr.log") ==
                         "sdmc:/switch/vcsnx/vcsnx_stderr.prev.log", "stderr log");
             require(vcsnx::previous_log_path("x/perf") == "x/perf.prev", "no extension");
         }},
    });
}
