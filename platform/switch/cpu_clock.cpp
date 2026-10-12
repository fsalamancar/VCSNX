#include "cpu_clock.hpp"

#include <array>
#include <cctype>
#include <cstdlib>
#include <string_view>

#if defined(__SWITCH__)
#include <cstdio>

#include <switch.h>

#include "trace_clock.hpp"
#endif

namespace vcsnx {
namespace {

constexpr std::uint32_t kMHz = 1'000'000u;
// The Switch's CPU rates from 612 MHz up to the stock maximum (pcv's table; 1020 is the default).
constexpr std::array<std::uint32_t, 11> kRatesMHz{612u, 714u, 816u, 918u, 1020u, 1122u,
                                                  1224u, 1326u, 1428u, 1581u, 1785u};

std::string_view trimmed(std::string_view text) noexcept {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
    return text;
}

bool equals_ignoring_case(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    return true;
}

} // namespace

std::uint32_t cpu_clock_target_hz(const char *setting) noexcept {
    const std::string_view text = trimmed(setting == nullptr ? std::string_view{} : std::string_view{setting});
    if (text.empty() || text == "0" || equals_ignoring_case(text, "off")) return 0u;
    std::uint32_t mhz = 0u;
    for (const char c : text) {
        if (c < '0' || c > '9' || mhz > 100'000u) return 0u;
        mhz = mhz * 10u + static_cast<std::uint32_t>(c - '0');
    }
    for (const std::uint32_t rate : kRatesMHz)
        if (rate == mhz) return mhz * kMHz;
    return 0u;
}

#if defined(__SWITCH__)
namespace {

// Holds the boost for the whole run and puts the previous rate back at exit. clkrst replaced
// pcv's clock calls in system version 8.0.0.
class CpuClockBoost {
public:
    explicit CpuClockBoost(std::uint32_t target_hz) : target_hz_(target_hz) {
        use_clkrst_ = hosversionAtLeast(8, 0, 0);
        Result result = 0;
        if (use_clkrst_) {
            result = clkrstInitialize();
            if (R_SUCCEEDED(result)) {
                result = clkrstOpenSession(&session_, PcvModuleId_CpuBus, 3);
                if (R_FAILED(result)) clkrstExit();
            }
        } else {
            result = pcvInitialize();
        }
        if (R_FAILED(result)) {
            std::fprintf(stderr, "[vcsnx-clock] t=%lldms cpu clock service unavailable result=0x%x\n",
                         vcsnx::trace_ms(), static_cast<unsigned>(result));
            return;
        }
        open_ = true;
        previous_hz_ = read();
        result = write(target_hz_);
        std::fprintf(stderr, "[vcsnx-clock] t=%lldms cpu previous=%u MHz target=%u MHz now=%u MHz result=0x%x\n",
                     vcsnx::trace_ms(), previous_hz_ / kMHz, target_hz_ / kMHz, read() / kMHz,
                     static_cast<unsigned>(result));
    }
    CpuClockBoost(const CpuClockBoost &) = delete;
    CpuClockBoost &operator=(const CpuClockBoost &) = delete;

    ~CpuClockBoost() {
        if (!open_) return;
        if (previous_hz_ != 0u) (void)write(previous_hz_);
        if (use_clkrst_) {
            clkrstCloseSession(&session_);
            clkrstExit();
        } else {
            pcvExit();
        }
    }

    void reapply() noexcept {
        if (open_ && read() != target_hz_) (void)write(target_hz_);
    }

private:
    std::uint32_t read() noexcept {
        u32 hz = 0;
        const Result result = use_clkrst_ ? clkrstGetClockRate(&session_, &hz)
                                          : pcvGetClockRate(PcvModule_CpuBus, &hz);
        return R_SUCCEEDED(result) ? hz : 0u;
    }
    Result write(std::uint32_t hz) noexcept {
        return use_clkrst_ ? clkrstSetClockRate(&session_, hz) : pcvSetClockRate(PcvModule_CpuBus, hz);
    }

    std::uint32_t target_hz_;
    std::uint32_t previous_hz_{};
    bool use_clkrst_{};
    bool open_{};
    ClkrstSession session_{};
};

CpuClockBoost *g_boost = nullptr;

} // namespace

void apply_cpu_clock_boost() noexcept {
    if (g_boost != nullptr) return;
    const std::uint32_t target = cpu_clock_target_hz(std::getenv("VCSNX_CPU_MHZ"));
    if (target == 0u) {
        std::fprintf(stderr, "[vcsnx-clock] t=%lldms cpu clock left to the system (VCSNX_CPU_MHZ)\n",
                     vcsnx::trace_ms());
        return;
    }
    static CpuClockBoost boost(target); // destroyed at exit, which restores the previous rate
    g_boost = &boost;
}

void reapply_cpu_clock_boost() noexcept {
    if (g_boost != nullptr) g_boost->reapply();
}
#else
void apply_cpu_clock_boost() noexcept {}
void reapply_cpu_clock_boost() noexcept {}
#endif

} // namespace vcsnx
