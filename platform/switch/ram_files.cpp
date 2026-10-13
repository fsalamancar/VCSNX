#include "ram_files.hpp"

#include <cctype>
#include <chrono>
#include <fstream>
#include <iostream>
#include <new>

#if defined(__SWITCH__)
#include <switch.h>
#endif

namespace vcsnx {
namespace {

constexpr std::size_t kChunkBytes = std::size_t{4} << 20;

std::string key_of(const std::filesystem::path &path) {
    std::string key = path.lexically_normal().generic_string();
    for (char &c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return key;
}

} // namespace

RamFiles::~RamFiles() { wait(); }

void RamFiles::start(std::vector<std::filesystem::path> paths) {
    if (!entries_.empty() || loader_.joinable()) return;
    for (const std::filesystem::path &path : paths) {
        auto entry = std::make_unique<Entry>();
        entry->path = key_of(path);
        entry->open_path = path;
        entries_.push_back(std::move(entry));
    }
    if (entries_.empty()) return;
    loader_ = std::thread([this] { load_all(); });
}

std::span<const std::uint8_t> RamFiles::find(const std::filesystem::path &path) const noexcept {
    if (entries_.empty()) return {};
    const std::string key = key_of(path);
    for (const auto &entry : entries_) {
        if (!entry->ready.load(std::memory_order_acquire) || entry->path != key) continue;
        if (!entry->used.exchange(true, std::memory_order_relaxed))
            std::cerr << "[vcsnx-ram] reads of " << std::filesystem::path(entry->path).filename().string()
                      << " now come from RAM\n";
        return {entry->data.get(), entry->size};
    }
    return {};
}

void RamFiles::wait() noexcept {
    if (loader_.joinable()) loader_.join();
}

void RamFiles::load_all() noexcept {
#if defined(__SWITCH__)
    // On the main thread's core and priority, while it decoded the boot movies flat out, MAINLA.IMG came in at
    // 5 MB/s (44 s); BEACH.IMG, after the movies, at 60 MB/s (2026-10-13). Core 2 (the GE worker's, idle until
    // gameplay) and a priority above the main thread's: this thread mostly waits for the SD.
    svcSetThreadCoreMask(CUR_THREAD_HANDLE, 2, 1u << 2);
    svcSetThreadPriority(CUR_THREAD_HANDLE, 0x2B);
#endif
    for (const auto &entry : entries_) {
        const auto begin = std::chrono::steady_clock::now();
        std::ifstream input(entry->open_path, std::ios::binary | std::ios::ate);
        if (!input) continue;
        const auto end = input.tellg();
        if (end <= 0) continue;
        const auto size = static_cast<std::size_t>(end);
        std::unique_ptr<std::uint8_t[]> data(new (std::nothrow) std::uint8_t[size]);
        if (!data) {
            std::cerr << "[vcsnx-ram] no room for " << entry->path << " (" << (size >> 20) << " MB)\n";
            continue;
        }
        input.seekg(0, std::ios::beg);
        std::size_t done = 0u;
        while (done < size && input) {
            const std::size_t now = std::min(kChunkBytes, size - done);
            input.read(reinterpret_cast<char *>(data.get() + done), static_cast<std::streamsize>(now));
            done += static_cast<std::size_t>(input.gcount());
        }
        if (done != size) {
            std::cerr << "[vcsnx-ram] short read of " << entry->path << "\n";
            continue;
        }
        entry->data = std::move(data);
        entry->size = size;
        entry->ready.store(true, std::memory_order_release);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin);
        std::cerr << "[vcsnx-ram] " << std::filesystem::path(entry->path).filename().string() << " " << (size >> 20)
                  << " MB in RAM after " << ms.count() << " ms\n";
    }
}

bool ram_streaming_enabled(const char *setting) noexcept {
    return !(setting != nullptr && setting[0] == '0' && setting[1] == '\0');
}

RamFiles &ram_files() noexcept {
    static RamFiles files;
    return files;
}

} // namespace vcsnx
