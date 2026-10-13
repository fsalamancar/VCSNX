#include "async_log.hpp"

#include <algorithm>
#include <vector>

namespace vcsnx {

AsyncLineWriter::AsyncLineWriter() : thread_([this] { run(); }) {}

AsyncLineWriter::~AsyncLineWriter() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    wake_.notify_one();
    if (thread_.joinable()) thread_.join();
}

void AsyncLineWriter::write(std::FILE *file, std::string line) {
    if (file == nullptr) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(Entry{file, std::move(line)});
    }
    wake_.notify_one();
}

void AsyncLineWriter::flush() {
    std::unique_lock<std::mutex> lock(mutex_);
    idle_.wait(lock, [this] { return queue_.empty() && !writing_; });
}

void AsyncLineWriter::run() {
    std::unique_lock<std::mutex> lock(mutex_);
    for (;;) {
        wake_.wait(lock, [this] { return stop_ || !queue_.empty(); });
        if (queue_.empty()) break; // stop_ with nothing left to write
        std::deque<Entry> batch;
        batch.swap(queue_);
        writing_ = true;
        lock.unlock();
        std::vector<std::FILE *> touched;
        for (const Entry &entry : batch) {
            std::fputs(entry.line.c_str(), entry.file);
            std::fputc('\n', entry.file);
            if (std::find(touched.begin(), touched.end(), entry.file) == touched.end()) touched.push_back(entry.file);
        }
        for (std::FILE *file : touched) std::fflush(file);
        lock.lock();
        writing_ = false;
        idle_.notify_all();
    }
    writing_ = false;
    idle_.notify_all();
}

} // namespace vcsnx
