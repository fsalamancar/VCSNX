#include "stderr_tee.hpp"
#include "test_support.hpp"

#include <ostream>
#include <sstream>
#include <string>
#include <vector>

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"forwards whole lines and hands each to the callback", [] {
             std::stringbuf sink;
             std::vector<std::string> seen;
             vcsnx::LineTeeBuf tee(&sink, [&seen](const std::string &line) {
                 seen.push_back(line);
                 return line.rfind("[frame-time]", 0) != 0;
             });
             std::ostream out(&tee);
             out << "hello\n[frame-time] frame_us=1\npartial";
             require(sink.str() == "hello\n", "frame-time lines must not be forwarded: " + sink.str());
             require(seen.size() == 2u && seen[0] == "hello" && seen[1] == "[frame-time] frame_us=1", "callback lines");
             out << " end\n";
             require(sink.str() == "hello\npartial end\n", "partial line completes on newline");
             require(seen.size() == 3u && seen[2] == "partial end", "third line");
         }},
        {"flush reaches the forward buffer", [] {
             std::stringbuf sink;
             vcsnx::LineTeeBuf tee(&sink, [](const std::string &) { return true; });
             std::ostream out(&tee);
             out << "x\n" << std::flush;
             require(out.good() && sink.str() == "x\n", "flush");
         }},
    });
}
