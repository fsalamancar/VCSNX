#pragma once
// Routes std::cerr on the device: "[frame-time]" and "[ge-phase]" lines feed perf.log, every other line goes
// to vcsnx_stderr.log and to the original stderr (nxlink when connected). The files are written by a
// background thread (async_log.hpp).

#include <functional>
#include <mutex>
#include <streambuf>
#include <string>

namespace vcsnx {

// Hands each complete line (without '\n') to on_line; when it returns true the line is also
// written to `forward`. A partial line waits for its newline.
class LineTeeBuf : public std::streambuf {
public:
    LineTeeBuf(std::streambuf *forward, std::function<bool(const std::string &)> on_line);

protected:
    int_type overflow(int_type ch) override;
    std::streamsize xsputn(const char *text, std::streamsize count) override;
    int sync() override;

private:
    std::streambuf *forward_;
    std::function<bool(const std::string &)> on_line_;
    std::string pending_;
    std::mutex mutex_;
};

// Installs the tee on std::cerr once: <data_dir>/perf.log (appended, one line per 10 s window)
// and <data_dir>/vcsnx_stderr.log (rewritten each session). Later calls do nothing.
void install_stderr_tee(const std::string &data_dir);

} // namespace vcsnx
