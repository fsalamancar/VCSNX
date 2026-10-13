#pragma once
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

namespace vcsnx {

// Writes log lines to their files on its own thread. The stderr tee used to write and fflush() every
// forwarded line on the thread that logged it, mostly the game thread: about 26 ms per line on the
// Switch's SD card, a hitch for each [audio-summary] (every 2 s) and a 3-4 s pause for one [ge-tl]
// window of 130 lines (2026-10-13). Lines reach each file in order; each batch is flushed once.
class AsyncLineWriter {
public:
    AsyncLineWriter();
    ~AsyncLineWriter(); // writes what is queued, then stops
    AsyncLineWriter(const AsyncLineWriter &) = delete;
    AsyncLineWriter &operator=(const AsyncLineWriter &) = delete;

    // Queues `line` and a newline for `file` (nullptr: dropped). Any thread; never touches the file.
    void write(std::FILE *file, std::string line);
    // Blocks until everything queued so far is written and flushed.
    void flush();

private:
    struct Entry {
        std::FILE *file;
        std::string line;
    };
    void run();

    std::mutex mutex_;
    std::condition_variable wake_;
    std::condition_variable idle_;
    std::deque<Entry> queue_;
    bool writing_{};
    bool stop_{};
    std::thread thread_; // last: starts after the members above exist
};

} // namespace vcsnx
