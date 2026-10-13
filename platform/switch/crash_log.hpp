#pragma once
#include <cstddef>
#include <cstdint>

namespace vcsnx {

// Crash evidence (2026-10-13: the GE worker read a game buffer whose page the kernel no longer let it
// read; the Atmosphère report says where, not what the page had become). On the Switch a libnx user
// exception handler (crash_log.cpp) writes <data dir>/vcsnx_crash.log with the registers, the call
// chain and svcQueryMemory of the faulting page, its neighbour and the stack, then ends the process
// (Atmosphère still writes a report, a user break). It writes through the FS service: no malloc, no stdio.

// A region as svcQueryMemory describes it (MemoryInfo); known=false if the query failed.
struct CrashRegion {
    std::uint64_t base{};
    std::uint64_t size{};
    std::uint32_t type{};
    std::uint32_t attr{};
    std::uint32_t perm{};
    std::uint32_t ipc_refs{};
    std::uint32_t device_refs{};
    bool known{};
};

struct CrashFacts {
    std::uint32_t error_desc{}; // ThreadExceptionDesc
    std::uint32_t esr{};
    std::uint64_t far{};
    std::uint64_t pc{}, lr{}, sp{}, fp{};
    std::uint64_t x[29]{};
    std::uint64_t runtime_base{}, runtime_size{}; // vcsnx.nro's code
    std::uint64_t module_base{}, module_size{};   // vcs_game.so
    std::uint64_t thread_id{};
    CrashRegion fault_page{}, page_before{}, stack{};
    std::uint64_t frames[16]{}; // return addresses from the frame-pointer chain
    std::uint32_t frame_count{};
};

// The report as text in `out`, always NUL-terminated inside `capacity`; returns its length. No allocation.
std::size_t format_crash_report(const CrashFacts &facts, char *out, std::size_t capacity) noexcept;

// Switch: where vcs_game.so is and the folder for vcsnx_crash.log ("sdmc:/switch/vcsnx"). Call once, early.
// Elsewhere: nothing.
void set_crash_log_context(const char *data_dir, std::uint64_t module_base, std::uint64_t module_size) noexcept;

} // namespace vcsnx
