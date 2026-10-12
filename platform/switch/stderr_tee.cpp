#include "stderr_tee.hpp"

#include "memory_usage.hpp"
#include "perf_log.hpp"

#include <cstdio>
#include <iostream>
#include <utility>

namespace vcsnx {

LineTeeBuf::LineTeeBuf(std::streambuf *forward, std::function<bool(const std::string &)> on_line)
    : forward_(forward), on_line_(std::move(on_line)) {}

LineTeeBuf::int_type LineTeeBuf::overflow(int_type ch) {
    if (traits_type::eq_int_type(ch, traits_type::eof())) return traits_type::not_eof(ch);
    const char c = traits_type::to_char_type(ch);
    xsputn(&c, 1);
    return ch;
}

std::streamsize LineTeeBuf::xsputn(const char *text, std::streamsize count) {
    std::lock_guard<std::mutex> guard(mutex_);
    for (std::streamsize index = 0; index < count; ++index) {
        if (text[index] != '\n') {
            pending_.push_back(text[index]);
            continue;
        }
        if (on_line_(pending_) && forward_ != nullptr) {
            forward_->sputn(pending_.data(), static_cast<std::streamsize>(pending_.size()));
            forward_->sputc('\n');
        }
        pending_.clear();
    }
    return count;
}

int LineTeeBuf::sync() {
    std::lock_guard<std::mutex> guard(mutex_);
    return forward_ == nullptr ? 0 : forward_->pubsync();
}

void install_stderr_tee(const std::string &data_dir) {
    static std::once_flag once;
    std::call_once(once, [&data_dir] {
        static PerfWindow window;
        static std::FILE *perf = std::fopen((data_dir + "/perf.log").c_str(), "a");
        static std::FILE *log = std::fopen((data_dir + "/vcsnx_stderr.log").c_str(), "w");
        if (perf != nullptr) {
            std::fputs("--- vcsnx session ---\n", perf);
            std::fflush(perf);
        }
        static LineTeeBuf tee(std::cerr.rdbuf(), [](const std::string &line) {
            if (const auto sample = parse_frame_time_line(line)) {
                if (const auto summary = window.add(*sample, memory_in_use_mb()); summary && perf != nullptr) {
                    std::fputs(summary->c_str(), perf);
                    std::fputc('\n', perf);
                    std::fflush(perf);
                }
                return false; // ~30 lines/s: summarised in perf.log, not forwarded
            }
            if (log != nullptr) {
                std::fputs(line.c_str(), log);
                std::fputc('\n', log);
                std::fflush(log);
            }
            return true;
        });
        std::cerr.rdbuf(&tee);
    });
}

} // namespace vcsnx
