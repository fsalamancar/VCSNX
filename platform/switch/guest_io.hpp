#pragma once
#include <cstddef>
#include <cstdint>
#include <istream>

namespace vcsnx {

// Reads up to `size` bytes from `input` into the game's memory at `destination` and returns how many
// arrived; the stream stops where a plain read would. Horizon lends the whole pages of a read's
// destination to the FS service for the length of the call, and no other thread of the process may
// touch them meanwhile. sceIoRead read files (the world streaming among them) straight into the game's
// RAM, and the GE worker, drawing on another core, faulted on a palette in those pages
// (vcsnx_crash.log 2026-10-13: fault page attr=IpcMapped, perm=0, ipc=1; twice in one day). The data
// goes through a private page-aligned buffer instead, then a copy.
std::size_t read_into_guest(std::istream &input, std::uint8_t *destination, std::size_t size);

} // namespace vcsnx
