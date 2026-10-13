// The present-phase summary averages each phase over a 10 s window; "build" is what finish_color_frame
// spends outside the fence wait and the readback copy.
#include "present_phases.hpp"
#include "test_support.hpp"

#include <string>

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"one summary per 10 s window with per-frame averages", [] {
             vcsnx::PresentPhases phases;
             require(!phases.take_report_if_due(0).has_value(), "starts the window");
             phases.add({2.0, 3.0, 10.0, 8.0, 1.0});
             phases.add({4.0, 3.0, 14.0, 12.0, 3.0});
             phases.add_deferred();
             require(!phases.take_report_if_due(9'999).has_value(), "not yet");
             const auto line = phases.take_report_if_due(10'000);
             require(line.has_value(), "due at 10 s");
             require(*line == "[present-phases] frames=2 fence=3.00 readback=3.00 build=6.00 gpu=10.00 record=2.00 "
                              "total_max=14.00 deferred=1 since_submit=0.00 ready=0%",
                     "got: " + line.value_or(""));
         }},
        {"the GPU wait is deferred only on a vblank with nothing new while the GPU is busy", [] {
             require(vcsnx::defer_previous_frame(true, false, false), "pending, nothing new, GPU busy: defer");
             require(!vcsnx::defer_previous_frame(true, true, false), "new geometry to submit: wait");
             require(!vcsnx::defer_previous_frame(true, false, true), "GPU already done: collect now");
             require(!vcsnx::defer_previous_frame(false, false, false), "nothing in flight");
         }},
        {"how long after the submit the wait began, and how often the GPU was already done", [] {
             // Switch: the fence wait (9-14 ms) matched the GPU time although a whole guest frame had
             // passed since the submit. These two say whether the GPU started late.
             vcsnx::PresentPhases phases;
             (void)phases.take_report_if_due(0);
             vcsnx::PresentPhaseSample late{};
             late.since_submit_ms = 30.0;
             late.ready = false;
             vcsnx::PresentPhaseSample done{};
             done.since_submit_ms = 20.0;
             done.ready = true;
             phases.add(late);
             phases.add(done);
             phases.add(late);
             phases.add(late);
             const auto line = phases.take_report_if_due(10'000);
             require(line.has_value() && line->find(" since_submit=27.50 ready=25%") != std::string::npos,
                     "got: " + line.value_or(""));
         }},
        {"the game's copy of the world into the displayed buffer is not new drawing", [] {
             // Mac [finish-timeline]: after each frame the game composes it into the displayed buffer
             // (0x178000) with 64 strips that sample the world target (0x088000); that bucket is never
             // drawn here, yet the Switch waited ~18 ms per game frame for the GPU because of it.
             require(!vcsnx::batch_draws(false, 0x088000u, 0x178000u), "a strip copying another target");
             require(vcsnx::batch_draws(false, 0u, 0x088000u), "world geometry");
             require(!vcsnx::batch_draws(true, 0u, 0x088000u), "a clear");
             require(vcsnx::batch_draws(false, 0x088000u, 0x088000u), "a draw sampling its own target");
         }},
        {"the window restarts after a report", [] {
             vcsnx::PresentPhases phases;
             (void)phases.take_report_if_due(0);
             phases.add({1.0, 1.0, 3.0, 2.0, 0.5});
             (void)phases.take_report_if_due(10'000);
             const auto empty = phases.take_report_if_due(20'000);
             require(empty.has_value() && empty->rfind("[present-phases] frames=0 ", 0) == 0u,
                     "a window without frames reports zero: " + empty.value_or(""));
         }},
    });
}
