#include "stderr_tee.hpp"

#include "async_log.hpp"
#include "log_files.hpp"
#include "memory_usage.hpp"
#include "perf_log.hpp"
#include "vcsnx_hooks.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <utility>

namespace vcsnx {

namespace {
std::atomic<bool> scheduler_dump_requested{false};
std::string stall_report_path; // set by install_stderr_tee
} // namespace

void write_stall_report(const std::string &text) noexcept {
    if (stall_report_path.empty()) return;
    std::FILE *file = std::fopen(stall_report_path.c_str(), "a");
    if (file == nullptr) return;
    std::fwrite(text.data(), 1, text.size(), file);
    std::fclose(file);
}

void request_scheduler_dump() noexcept { scheduler_dump_requested.store(true, std::memory_order_release); }

bool take_scheduler_dump_request() noexcept {
    return scheduler_dump_requested.load(std::memory_order_acquire) &&
           scheduler_dump_requested.exchange(false, std::memory_order_acq_rel);
}

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
        static LoadingWatch loading;
        static MemorySampler memory(300u); // about once a perf.log window: each read is 5-7 ms on the Switch
        static std::FILE *perf = std::fopen((data_dir + "/perf.log").c_str(), "a");
        keep_previous_log(data_dir + "/vcsnx_stderr.log"); // the last session's, for a crash or hang
        keep_previous_log(data_dir + "/vcsnx_stall.log");
        stall_report_path = data_dir + "/vcsnx_stall.log";
        static std::FILE *log = std::fopen((data_dir + "/vcsnx_stderr.log").c_str(), "w");
        if (perf != nullptr) {
            std::fputs("--- vcsnx session ---\n", perf);
            std::fflush(perf);
        }
        // The SD writes happen on the writer's thread (async_log.hpp), never on the thread that logs: the
        // stderr copy too, which is vcsnx_boot.log unbuffered (nx_startup.cpp) or nxlink. Kept for the whole
        // process (no destructor racing late log lines); what is queued is written at exit.
        static AsyncLineWriter *writer = new AsyncLineWriter;
        std::atexit([] { writer->flush(); });
        static LineTeeBuf tee(nullptr, [](const std::string &line) {
            if (const auto sample = parse_frame_time_line(line)) {
                if (const auto summary = window.add(*sample, memory.sample(memory_in_use_mb)); summary) {
                    writer->write(perf, *summary);
                    if (loading.observe(window.last_window())) {
                        writer->write(perf, "loading screen for 30 s: game threads dumped to vcsnx_stderr.log ([loading-stall])");
                        request_scheduler_dump();
                    }
                }
                return false; // ~30 lines/s: summarised in perf.log, not forwarded
            }
            if (const auto phase = parse_ge_phase_line(line)) {
                window.add_phase(*phase); // PSPRECOMP_GE_PHASE_DIAG: one per vblank, summarised the same way
                return false;
            }
            writer->write(log, line);
            writer->write(stderr, line);
            return false;
        });
        std::cerr.rdbuf(&tee);
    });
}

} // namespace vcsnx
