#pragma once
// Keeps the previous session's log (vcsnx_boot.log -> vcsnx_boot.prev.log) before a new session truncates
// it, so the log of a session that crashed or hung survives opening VCSNX again (backlog bugs 1-3).

#include <string>

namespace vcsnx {

// "dir/vcsnx_stderr.log" -> "dir/vcsnx_stderr.prev.log"; a name without an extension gets ".prev".
[[nodiscard]] std::string previous_log_path(const std::string &path);

// Moves `path` to previous_log_path(path), replacing an older one; does nothing if `path` does not exist.
void keep_previous_log(const std::string &path);

} // namespace vcsnx
