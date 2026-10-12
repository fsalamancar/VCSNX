#pragma once
#include <string>

namespace vcsnx {

// Where VCSNative.ini, perf.log and the logs live: VCSNX_DATA_DIR on the Switch
// (sdmc:/switch/vcsnx), the executable's folder on the Mac.
[[nodiscard]] std::string data_directory();

// Once per process, from display_window_start(): installs the stderr tee (perf.log + logs).
void platform_startup();

} // namespace vcsnx
