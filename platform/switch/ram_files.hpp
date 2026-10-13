#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace vcsnx {

// The world streaming archives in RAM (author's idea, 2026-10-13: "use the RAM", against the stutter and
// pop-in while driving). VCS streams the city from MAINLA.IMG (236 MB), BEACH.IMG (199 MB) and GTA3PSP.IMG
// (39 MB) through sceIoRead on the game thread; on the Switch every read waits for the SD card. A background
// thread reads each file once, whole, into one block (asked for at startup, while the heap is still in one
// piece); until a file is complete its reads go to the SD as before, then they are copies from RAM
// (vcs_profile.cpp, read_virtual_disc). ~474 MB of the ~3.1 GB heap, of which ~640 MB are in use.
class RamFiles {
public:
    RamFiles() = default;
    ~RamFiles(); // waits for the loader
    RamFiles(const RamFiles &) = delete;
    RamFiles &operator=(const RamFiles &) = delete;

    // Starts loading `paths` (missing files are skipped). Once.
    void start(std::vector<std::filesystem::path> paths);
    // The whole file, if it is one of them and already loaded; empty otherwise. Paths compare without case
    // (the SD's names need not match the disc's). Any thread.
    [[nodiscard]] std::span<const std::uint8_t> find(const std::filesystem::path &path) const noexcept;
    // Waits until the loader is done.
    void wait() noexcept;

private:
    struct Entry {
        std::string path;                  // lower-case key
        std::filesystem::path open_path;   // as given
        std::unique_ptr<std::uint8_t[]> data;
        std::size_t size{};
        std::atomic<bool> ready{false};
        mutable std::atomic<bool> used{false}; // the first read from RAM is logged
    };
    void load_all() noexcept;

    std::vector<std::unique_ptr<Entry>> entries_;
    std::thread loader_;
};

// VCSNX_RAM_STREAMING: anything but "0" (or unset) keeps it on.
[[nodiscard]] bool ram_streaming_enabled(const char *setting) noexcept;

// The process's files (started by nx_startup on the Switch).
RamFiles &ram_files() noexcept;

} // namespace vcsnx
