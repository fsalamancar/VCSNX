#include "hd_preload.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <system_error>

#if defined(__SWITCH__)
#include <switch.h>
#endif

namespace vcsnx {
namespace {

bool ends_with_ignoring_case(std::string_view text, std::string_view suffix) noexcept {
    if (text.size() < suffix.size()) return false;
    text.remove_prefix(text.size() - suffix.size());
    for (std::size_t i = 0; i < suffix.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(text[i])) != suffix[i]) return false;
    return true;
}

std::string normalized(std::string_view directory) {
    std::string text(directory);
    while (text.size() > 1u && (text.back() == '/' || text.back() == '\\')) text.pop_back();
    return text;
}

} // namespace

std::optional<std::uint64_t> hd_key_of(std::string_view name) noexcept {
    if (name.size() != 20u || !ends_with_ignoring_case(name, ".png")) return std::nullopt;
    std::uint64_t key = 0u;
    for (std::size_t i = 0; i < 16u; ++i) {
        const char c = name[i];
        int digit = -1;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        if (digit < 0) return std::nullopt;
        key = (key << 4u) | static_cast<std::uint64_t>(digit);
    }
    return key;
}

std::optional<std::uint8_t> hd_scale_shift(std::uint32_t hd_width, std::uint32_t hd_height, std::uint32_t width,
                                           std::uint32_t height) noexcept {
    if (hd_width > 8192u || hd_height > 8192u) return std::nullopt;
    for (std::uint8_t shift = 0u; shift <= 3u; ++shift)
        if (hd_width == (width << shift) && hd_height == (height << shift)) return shift;
    return std::nullopt;
}

bool waits_for_hd_preload(std::string_view path) noexcept { return ends_with_ignoring_case(path, ".lvz"); }

bool hd_preload_enabled(const char *env_value) noexcept {
    return env_value == nullptr || std::string_view(env_value) != "0";
}

HdPreload::~HdPreload() {
    stop_.store(true, std::memory_order_relaxed);
    for (std::thread &worker : workers_)
        if (worker.joinable()) worker.join();
}

void HdPreload::configure(std::string directory, std::uint64_t budget_bytes, unsigned threads, HdDecoder decoder) {
    std::lock_guard lock(mutex_);
    configured_directory_ = std::move(directory);
    configured_budget_ = budget_bytes;
    configured_threads_ = threads;
    configured_decoder_ = std::move(decoder);
}

void HdPreload::start() {
    std::string directory;
    std::uint64_t budget = 0u;
    unsigned threads = 0u;
    HdDecoder decoder;
    {
        std::lock_guard lock(mutex_);
        if (!configured_decoder_) return;
        directory = configured_directory_;
        budget = configured_budget_;
        threads = configured_threads_;
        decoder = configured_decoder_;
    }
    start(std::move(directory), budget, threads, std::move(decoder));
}

void HdPreload::start(std::string directory, std::uint64_t budget_bytes, unsigned threads, HdDecoder decoder) {
    if (started_.exchange(true)) return;
    std::lock_guard lock(mutex_);
    begin_ = std::chrono::steady_clock::now();
    directory_ = normalized(directory);
    budget_ = budget_bytes;
    decoder_ = std::move(decoder);
    std::error_code ec;
    for (std::filesystem::directory_iterator it(directory_, ec), end; !ec && it != end; it.increment(ec))
        if (const auto key = hd_key_of(it->path().filename().string()); key && it->is_regular_file(ec))
            files_.emplace_back(*key, it->path().string());
    const unsigned count = static_cast<unsigned>(std::min<std::size_t>(std::max(1u, threads), files_.size()));
    if (count == 0u) {
        finished_ = true;
        return;
    }
    running_ = count;
    for (unsigned i = 0; i < count; ++i) workers_.emplace_back([this, i] { work(i); });
}

void HdPreload::work(unsigned index) {
#if defined(__SWITCH__)
    // One worker per core at the lowest priority a homebrew thread may have, 0x3B (0x3F is refused, so they
    // stayed at the game thread's own 0x3B and, Horizon not time-slicing equal priorities, held its core while
    // the movies played). They run while the loading screen waits for them, the game thread blocked.
    const int core = static_cast<int>(index % 3u);
    svcSetThreadCoreMask(CUR_THREAD_HANDLE, core, 1u << core);
    if (const Result result = svcSetThreadPriority(CUR_THREAD_HANDLE, 0x3B); R_FAILED(result) && index == 0u)
        std::fprintf(stderr, "[hd-preload] worker priority 0x3B refused: 0x%x\n", static_cast<unsigned>(result));
#else
    (void)index;
#endif
    while (!stop_.load(std::memory_order_relaxed)) {
        const std::size_t i = next_.fetch_add(1u);
        if (i >= files_.size()) break;
        std::optional<HdImage> image = decoder_(files_[i].second);
        std::lock_guard lock(mutex_);
        ++done_;
        if (image && bytes_ + image->rgba.size() <= budget_) {
            bytes_ += image->rgba.size();
            images_[files_[i].first] = std::make_shared<const HdImage>(std::move(*image));
        }
    }
    std::lock_guard lock(mutex_);
    if (--running_ == 0u) {
        finished_ = true;
        finished_cv_.notify_all();
        std::cerr << "[hd-preload] " << images_.size() << " of " << files_.size() << " replacements in RAM ("
                  << (bytes_ >> 20) << " MB) after "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin_).count()
                  << " ms\n";
    }
}

std::shared_ptr<const HdImage> HdPreload::find(std::uint64_t key) const {
    std::lock_guard lock(mutex_);
    const auto found = images_.find(key);
    return found == images_.end() ? nullptr : found->second;
}

HdGallery HdPreload::images_of_size(std::uint32_t width, std::uint32_t height) const {
    HdGallery found;
    {
        std::lock_guard lock(mutex_);
        for (const auto &[key, image] : images_)
            if (image->width == width && image->height == height) found.emplace_back(key, image);
    }
    std::sort(found.begin(), found.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
    return found;
}

HdPreloadProgress HdPreload::progress() const {
    std::lock_guard lock(mutex_);
    return {done_, files_.size(), bytes_, finished_};
}

bool HdPreload::serves(std::string_view directory) const {
    if (!started_.load()) return false;
    std::lock_guard lock(mutex_);
    return normalized(directory) == directory_;
}

void HdPreload::wait() {
    if (!started_.load()) return;
    std::unique_lock lock(mutex_);
    finished_cv_.wait(lock, [this] { return finished_; });
}

bool HdPreload::done() const {
    if (!started_.load()) return true;
    std::lock_guard lock(mutex_);
    return finished_;
}

HdPreload &hd_preload() {
    static HdPreload instance;
    return instance;
}

} // namespace vcsnx
