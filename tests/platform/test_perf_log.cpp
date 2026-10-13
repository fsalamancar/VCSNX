#include "perf_log.hpp"
#include "test_support.hpp"

#include <string>

namespace {
using namespace vcsnx;
using vcsnx_test::require;

FrameTimeSample frame(std::uint64_t frame_us) {
    return FrameTimeSample{frame_us, 10000u, 1000u, 20000u};
}
} // namespace

int main() {
    return vcsnx_test::run({
        {"parses the vendor's frame-time line", [] {
             const auto sample = parse_frame_time_line(
                 "[frame-time] vblank=120 frame_us=33400 ge_us=9100 ge_async_wait_us=0 present_us=800 "
                 "io_us=0 cpu_us=24300 guest_cpu_us=23500 guest_us=16683 ge_calls=12 fps=29");
             require(sample.has_value(), "line not recognised");
             require(sample->frame_us == 33400u && sample->ge_us == 9100u, "frame/ge");
             require(sample->present_us == 800u && sample->guest_cpu_us == 23500u, "present/guest_cpu (not guest_us or cpu_us)");
         }},
        {"ignores other lines and malformed frame_us", [] {
             require(!parse_frame_time_line("[ge-phase] vblank=1 ge_us=5").has_value(), "other prefix");
             require(!parse_frame_time_line("[frame-time] vblank=1 ge_us=5").has_value(), "no frame_us");
             require(!parse_frame_time_line("[frame-time] frame_us=abc").has_value(), "bad number");
         }},
        {"emits one summary when 10 s of frames accumulate", [] {
             PerfWindow window;
             for (int i = 0; i < 249; ++i) require(!window.add(frame(40000u), 500u).has_value(), "early summary");
             const auto line = window.add(frame(40000u), 500u);
             require(line.has_value(), "no summary at 10 s");
             require(*line == "t=10s frames=250 fps_avg=25.0 fps_min=25.0 guest_ms_avg=20.0 guest_ms_max=20.0 "
                              "ge_ms_avg=10.0 ge_ms_max=10.0 present_ms_avg=1.0 present_ms_max=1.0 mem_peak_mb=500",
                     "summary text: " + *line);
         }},
        {"slow frame sets fps_min; windows restart and keep elapsed time", [] {
             PerfWindow window;
             for (int i = 0; i < 249; ++i) (void)window.add(frame(40000u), 500u);
             const auto first = window.add(frame(100000u), 700u);
             require(first && first->find("fps_min=10.0") != std::string::npos, "fps_min");
             require(first->find("mem_peak_mb=700") != std::string::npos, "peak memory");
             std::optional<std::string> second;
             for (int i = 0; i < 250 && !second; ++i) second = window.add(frame(40000u), 300u);
             require(second && second->rfind("t=20s frames=250 ", 0) == 0, "second window: " + second.value_or("none"));
             require(second->find("mem_peak_mb=300") != std::string::npos, "peak resets per window");
         }},
        {"the last window's averages are kept for the loading watch", [] {
             PerfWindow window;
             for (int i = 0; i < 250; ++i) (void)window.add(frame(40000u), 500u);
             const WindowStats last = window.last_window();
             require(last.fps_avg > 24.9 && last.fps_avg < 25.1, "fps_avg");
             require(last.ge_ms_avg > 9.9 && last.ge_ms_avg < 10.1, "ge_ms_avg");
         }},
        {"a loading screen that lasts three windows asks for one scheduler dump", [] {
             LoadingWatch watch;
             const WindowStats loading{59.2, 0.6};
             require(!watch.observe(loading) && !watch.observe(loading), "not before 30 s");
             require(watch.observe(loading), "at 30 s");
             require(!watch.observe(loading) && !watch.observe(loading), "once per loading screen");
         }},
        {"gameplay and a short loading screen never ask", [] {
             LoadingWatch watch;
             const WindowStats gameplay{30.5, 12.4}, loading{59.1, 0.5};
             require(!watch.observe(gameplay) && !watch.observe(loading) && !watch.observe(loading), "two windows");
             require(!watch.observe(gameplay) && !watch.observe(loading), "the count restarts after gameplay");
         }},
        {"it asks again after the game moves on, three times per session at most", [] {
             LoadingWatch watch;
             const WindowStats gameplay{30.0, 10.0}, loading{59.0, 0.5};
             int dumps = 0;
             for (int episode = 0; episode < 5; ++episode) {
                 for (int i = 0; i < 4; ++i) dumps += watch.observe(loading) ? 1 : 0;
                 (void)watch.observe(gameplay);
             }
             require(dumps == 3, "dumps=" + std::to_string(dumps));
         }},
    });
}

