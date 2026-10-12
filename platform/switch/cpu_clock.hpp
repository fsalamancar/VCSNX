#pragma once
#include <cstdint>

namespace vcsnx {

// CPU clock VCSNX asks the Switch for, from VCSNX_CPU_MHZ (set in vcsnx_env.txt): a Switch CPU
// rate in MHz from 612 to 1785 (the stock "boost mode" rate) -> it. Unset, empty, "0", "off" and
// anything else (typos, overclocks above 1785) -> 0: leave the clock to the system and to the
// user's own tools (sys-clk), which a default boost re-applied every 2 s would fight.
[[nodiscard]] std::uint32_t cpu_clock_target_hz(const char *setting) noexcept;

// Switch: applies cpu_clock_target_hz(getenv("VCSNX_CPU_MHZ")) and restores the previous rate at
// exit. No-op elsewhere.
void apply_cpu_clock_boost() noexcept;

// Switch: sets the boost again if the system changed the clock (docking, performance mode).
// Cheap enough for the 2 s status heartbeat. No-op elsewhere or when no boost is active.
void reapply_cpu_clock_boost() noexcept;

} // namespace vcsnx
