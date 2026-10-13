// The GE timeline keeps short windows of guest/GE events and prints them once each window ends.
#include "ge_timeline.hpp"
#include "test_support.hpp"

#include <string>
#include <vector>

namespace {

std::int64_t fake_now = 0;
std::int64_t fake_clock() noexcept { return fake_now; }

} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"off records nothing and prints nothing", [] {
             vcsnx::GeTimeline timeline(0u, 12u, &fake_clock);
             for (std::uint64_t vblank = 1u; vblank < 100u; ++vblank) {
                 require(timeline.vblank(vblank).empty(), "no lines");
                 timeline.note('q', 1u);
             }
         }},
        {"a window covers its vblanks and prints at its end", [] {
             vcsnx::GeTimeline timeline(100u, 3u, &fake_clock);
             fake_now = 1000;
             require(timeline.vblank(99u).empty(), "before the window");
             timeline.note('q', 7u);  // outside: dropped
             fake_now = 2000;
             require(timeline.vblank(100u).empty(), "window opens");
             timeline.note('V', 100u);
             fake_now = 2500;
             timeline.note('x', 0u, 480);
             fake_now = 3000;
             require(timeline.vblank(101u).empty(), "still open");
             timeline.note('V', 101u);
             require(timeline.vblank(102u).empty(), "still open");
             fake_now = 4000;
             const std::vector<std::string> lines = timeline.vblank(103u);
             require(lines.size() == 3u, "three events, got " + std::to_string(lines.size()));
             require(lines[0] == "[ge-tl] t=0 V a=100 b=0", lines[0]);
             require(lines[1] == "[ge-tl] t=500 x a=0 b=480", lines[1]);
             require(lines[2] == "[ge-tl] t=1000 V a=101 b=0", lines[2]);
             timeline.note('q', 1u);
             require(timeline.vblank(104u).empty(), "closed again");
         }},
        {"windows repeat every `every` vblanks", [] {
             vcsnx::GeTimeline timeline(10u, 2u, &fake_clock);
             int printed = 0;
             for (std::uint64_t vblank = 1u; vblank <= 45u; ++vblank) {
                 if (!timeline.vblank(vblank).empty()) ++printed;
                 timeline.note('V', static_cast<std::uint32_t>(vblank));
             }
             require(printed == 4, "windows at 10, 20, 30 and 40: " + std::to_string(printed));
         }},
        {"the setting is a vblank count", [] {
             require(vcsnx::ge_timeline_every(nullptr) == 0u, "unset");
             require(vcsnx::ge_timeline_every("") == 0u, "empty");
             require(vcsnx::ge_timeline_every("900") == 900u, "900");
             require(vcsnx::ge_timeline_every("abc") == 0u, "not a number");
             require(vcsnx::ge_timeline_every("5") == 0u, "shorter than a window");
         }},
    });
}
