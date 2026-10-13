// After a late vblank the frame limiter does not sleep at the next one (the game's light B vblank).
#include "frame_limiter.hpp"
#include "test_support.hpp"

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"on time, every early vblank sleeps", [] {
             vcsnx::LimiterBurst burst;
             for (int i = 0; i < 4; ++i) {
                 require(burst.may_sleep(), "early vblank sleeps");
                 burst.note(false);
             }
         }},
        {"the vblank after a late one does not sleep", [] {
             vcsnx::LimiterBurst burst;
             burst.note(true);   // A: missed its slot, the limiter caught up
             require(!burst.may_sleep(), "B right after it");
             burst.note(false);  // B: early, did not sleep
             require(burst.may_sleep(), "the next early vblank sleeps again");
         }},
        {"a slow host never sleeps", [] {
             vcsnx::LimiterBurst burst;
             for (int frame = 0; frame < 10; ++frame) {
                 burst.note(true);  // A late
                 require(!burst.may_sleep(), "B");
                 burst.note(false);
             }
         }},
    });
}
