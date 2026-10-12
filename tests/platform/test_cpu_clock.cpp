#include "cpu_clock.hpp"
#include "test_support.hpp"

int main() {
    using vcsnx::cpu_clock_target_hz;
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"unset or empty leaves the system clock alone", [] {
             // The user's own clock tools (sys-clk) would fight a default boost.
             require(cpu_clock_target_hz(nullptr) == 0u, "unset");
             require(cpu_clock_target_hz("") == 0u, "empty");
         }},
        {"0 or off leaves the system clock alone", [] {
             require(cpu_clock_target_hz("0") == 0u, "0");
             require(cpu_clock_target_hz("off") == 0u, "off");
             require(cpu_clock_target_hz("OFF") == 0u, "OFF");
         }},
        {"a listed Switch CPU rate in MHz is used as is", [] {
             require(cpu_clock_target_hz("1020") == 1'020'000'000u, "stock 1020");
             require(cpu_clock_target_hz("1428") == 1'428'000'000u, "1428");
             require(cpu_clock_target_hz(" 1785 ") == 1'785'000'000u, "spaces are ignored");
         }},
        {"anything else, overclocks included, leaves the clock alone", [] {
             require(cpu_clock_target_hz("1963") == 0u, "Mariko overclock is not offered");
             require(cpu_clock_target_hz("1800") == 0u, "not a Switch rate");
             require(cpu_clock_target_hz("fast") == 0u, "not a number");
             require(cpu_clock_target_hz("1785MHz") == 0u, "trailing text");
         }},
    });
}
