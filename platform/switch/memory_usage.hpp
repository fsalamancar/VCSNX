#pragma once
#include <cstdint>

namespace vcsnx {

// Heap currently in use by malloc/new in this process (Switch: mallinfo().uordblks; Mac: all malloc
// zones). libnx reserves the whole heap at start, so the kernel's used-memory figure stays at the
// reservation; this is what the game and Mesa actually hold. 0 when unavailable.
[[nodiscard]] std::uint64_t memory_in_use_mb() noexcept;

// Switch: memory available to this process in the current mode (application vs applet).
// Mac: 0 (not meaningful).
[[nodiscard]] std::uint64_t memory_total_mb() noexcept;

} // namespace vcsnx
