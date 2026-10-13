#include "log_files.hpp"

#include <cstdio>

namespace vcsnx {

std::string previous_log_path(const std::string &path) {
    const std::size_t slash = path.find_last_of('/');
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return path + ".prev";
    return path.substr(0, dot) + ".prev" + path.substr(dot);
}

void keep_previous_log(const std::string &path) {
    std::FILE *current = std::fopen(path.c_str(), "rb");
    if (current == nullptr) return;
    std::fclose(current);
    const std::string previous = previous_log_path(path);
    std::remove(previous.c_str()); // rename does not replace an existing file on the SD card's FAT
    std::rename(path.c_str(), previous.c_str());
}

} // namespace vcsnx
