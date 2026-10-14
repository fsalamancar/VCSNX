#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vcsnx {

// HD replacement textures in RAM (author, 2026-10-13: "aprovecha la ram ... que carguen al inicio todo"). The
// console used to read and decode each Textures/<key>.png the first time the game drew that texture, so it
// showed the small one for a moment. Background threads decode every replacement into RAM (~633 MB for the
// part-1 pack and the city); the replacement worker takes them from here. They start, and the game's own
// loading screen (artwork and bar) waits for them, at the first level archive read: from startup they took the
// game's core while the movies played, and the screen stayed black after the bAlo splash (author, 2026-10-14).

struct HdImage {
    std::uint32_t width{}, height{};
    std::vector<std::uint8_t> rgba;
};
using HdDecoder = std::function<std::optional<HdImage>(const std::string &file)>;
// Images in RAM with their keys, by key.
using HdGallery = std::vector<std::pair<std::uint64_t, std::shared_ptr<const HdImage>>>;

struct HdPreloadProgress {
    std::size_t done{}, total{};  // files looked at / found
    std::uint64_t bytes{};        // decoded bytes kept
    bool finished{};
};

// The key a replacement's name carries: "<16 hex digits>.png".
[[nodiscard]] std::optional<std::uint64_t> hd_key_of(std::string_view file_name) noexcept;
// Whether the game's loading screen waits here: the first read of a level archive (*.LVZ).
[[nodiscard]] bool waits_for_hd_preload(std::string_view path) noexcept;
// How many doublings an HD copy of `hd_width` x `hd_height` is over the game's `width` x `height` texture (0-3),
// or nothing when it is not such a copy (another shape, not a power-of-two multiple, over 8192).
[[nodiscard]] std::optional<std::uint8_t> hd_scale_shift(std::uint32_t hd_width, std::uint32_t hd_height,
                                                         std::uint32_t width, std::uint32_t height) noexcept;
// Whether a texture goes up as its HD copy right away instead of a few frames later from the background pipeline:
// interface art in menus and loading screens, where the game's own picture showed first (the PSP's controls drawing,
// the loading artworks). Not while playing: the radar's HD map tiles then held the GE thread (2026-10-14).
[[nodiscard]] inline bool hd_copy_at_upload(bool through, bool playing) noexcept { return through && !playing; }
// VCSNX_HD_PRELOAD: on unless "0".
[[nodiscard]] bool hd_preload_enabled(const char *env_value) noexcept;

class HdPreload {
public:
    HdPreload() = default;
    ~HdPreload();  // stops and joins the workers
    HdPreload(const HdPreload &) = delete;
    HdPreload &operator=(const HdPreload &) = delete;

    // What start() loads: every <key>.png of `directory`, decoded by `threads` workers, keeping images until
    // `budget_bytes`. Reads nothing and starts no thread.
    void configure(std::string directory, std::uint64_t budget_bytes, unsigned threads, HdDecoder decoder);
    // Starts the configured preload. Once; without a configure, nothing.
    void start();
    // configure + start.
    void start(std::string directory, std::uint64_t budget_bytes, unsigned threads, HdDecoder decoder);
    [[nodiscard]] std::shared_ptr<const HdImage> find(std::uint64_t key) const;
    // Every image in RAM so far of `width` x `height` (the loading screen's other artworks), by key.
    [[nodiscard]] HdGallery images_of_size(std::uint32_t width, std::uint32_t height) const;
    [[nodiscard]] HdPreloadProgress progress() const;
    // Whether the images come from `directory` (the replacement folder the game uses).
    [[nodiscard]] bool serves(std::string_view directory) const;
    // Blocks until every file was looked at (returns at once if never started).
    void wait();
    // Whether wait() would return at once.
    [[nodiscard]] bool done() const;

private:
    void work(unsigned index);

    mutable std::mutex mutex_;
    std::condition_variable finished_cv_;
    std::unordered_map<std::uint64_t, std::shared_ptr<const HdImage>> images_;
    std::vector<std::pair<std::uint64_t, std::string>> files_;
    std::vector<std::thread> workers_;
    HdDecoder decoder_;
    std::string directory_;
    // What configure() set aside for start().
    std::string configured_directory_;
    std::uint64_t configured_budget_{};
    unsigned configured_threads_{};
    HdDecoder configured_decoder_;
    std::uint64_t budget_{};
    std::atomic<bool> started_{false};
    std::atomic<bool> stop_{false};
    std::atomic<std::size_t> next_{0};
    std::chrono::steady_clock::time_point begin_{};
    std::size_t done_{};
    std::uint64_t bytes_{};
    unsigned running_{};
    bool finished_{};
};

HdPreload &hd_preload();

} // namespace vcsnx
