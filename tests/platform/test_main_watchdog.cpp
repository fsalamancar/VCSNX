// The watchdog reports a main thread that stopped presenting, once per stall.
#include "main_watchdog.hpp"
#include "test_support.hpp"

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"nothing to report before the main thread ever presented", [] {
             vcsnx::StallWatch watch(3000);
             require(!watch.should_report(100000), "no present yet");
         }},
        {"a main thread that keeps presenting is fine", [] {
             vcsnx::StallWatch watch(3000);
             for (long long t = 0; t < 20000; t += 33) {
                 watch.presented(t);
                 require(!watch.should_report(t + 10), "presenting");
             }
         }},
        {"a stall is reported once, after the limit", [] {
             vcsnx::StallWatch watch(3000);
             watch.presented(1000);
             require(!watch.should_report(3999), "not yet");
             require(watch.should_report(4000), "3 s without a present");
             require(!watch.should_report(9000), "once per stall");
         }},
        {"a present re-arms it", [] {
             vcsnx::StallWatch watch(3000);
             watch.presented(0);
             require(watch.should_report(5000), "first stall");
             watch.presented(6000);
             require(!watch.should_report(7000), "presenting again");
             require(watch.should_report(9500), "a second stall is reported too");
         }},
    });
}
