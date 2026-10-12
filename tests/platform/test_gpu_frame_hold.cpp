#include "gpu_frame_hold.hpp"
#include "test_support.hpp"

#include <cstdint>

namespace {
// Runs `vblanks` vblanks with the same inputs; returns the final holding state.
bool run_vblanks(bool &holding, std::uint64_t &idle, int vblanks, bool ready, bool in_flight) {
    for (int i = 0; i < vblanks; ++i) vcsnx::step_gpu_frame_hold(holding, idle, ready, in_flight);
    return holding;
}
} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"a new GPU frame starts the hold", [] {
             bool holding = false;
             std::uint64_t idle = 7u;
             vcsnx::step_gpu_frame_hold(holding, idle, true, false);
             require(holding && idle == 0u, "holding with a fresh idle count");
         }},
        {"an idle GE releases the hold after 4 vblanks, as upstream does", [] {
             bool holding = false;
             std::uint64_t idle = 0u;
             vcsnx::step_gpu_frame_hold(holding, idle, true, false);
             require(run_vblanks(holding, idle, 4, false, false), "4 idle vblanks still hold");
             require(!run_vblanks(holding, idle, 1, false, false), "the 5th idle vblank hands back to software");
         }},
        {"lists in flight keep the hold however long the GE takes", [] {
             // A Switch frame takes 43-134 ms on the GE worker: 3-8 vblanks with nothing new.
             bool holding = false;
             std::uint64_t idle = 0u;
             vcsnx::step_gpu_frame_hold(holding, idle, true, false);
             require(run_vblanks(holding, idle, 10, false, true), "busy GE holds for 10 vblanks");
             require(idle == 0u, "busy vblanks are not idle");
             require(run_vblanks(holding, idle, 4, false, false), "then the usual 4 idle vblanks");
             require(!run_vblanks(holding, idle, 1, false, false), "and release on the 5th");
         }},
        {"only a new GPU frame is published to the window", [] {
             // The window keeps showing a held frame; publishing it again every vblank cost a
             // 921,600-pixel swizzle on the guest thread and a 3.6 MB upload (Plan 2 review).
             require(vcsnx::gpu_frame_needs_publish(true), "new frame");
             require(!vcsnx::gpu_frame_needs_publish(false), "held frame");
         }},
        {"nothing to hold before the first GPU frame", [] {
             bool holding = false;
             std::uint64_t idle = 0u;
             require(!run_vblanks(holding, idle, 3, false, true), "software keeps the window");
         }},
    });
}
