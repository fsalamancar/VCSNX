#include "loading_screen.hpp"

#include <algorithm>

namespace vcsnx {
namespace {

constexpr float kHdWeight = 4.0f;
constexpr float kRamWeight = 1.0f;

} // namespace

bool reads_hd_artwork_now(std::uint32_t width, std::uint32_t height, bool preload_serves) noexcept {
    return !preload_serves && width == kLoadingArtWidth && height == kLoadingArtHeight;
}

bool looks_like_artwork(const HdImage &image) noexcept {
    constexpr int kArtworkChroma = 24;  // mean of max(r,g,b) - min(r,g,b): the 12 artworks 39-113, the grey two 0.7 and 4.1
    const std::size_t pixel_count = image.rgba.size() / 4u;
    if (pixel_count == 0u) return false;
    const std::size_t step = std::max<std::size_t>(1u, pixel_count / 4096u);
    std::uint64_t chroma = 0u;
    std::size_t sampled = 0u;
    for (std::size_t i = 0; i < pixel_count; i += step, ++sampled) {
        const std::uint8_t *p = image.rgba.data() + i * 4u;
        chroma += static_cast<std::uint64_t>(std::max({p[0], p[1], p[2]}) - std::min({p[0], p[1], p[2]}));
    }
    return chroma >= static_cast<std::uint64_t>(kArtworkChroma) * sampled;
}

float loading_fraction(std::size_t hd_done, std::size_t hd_total, std::uint64_t ram_done,
                       std::uint64_t ram_total) noexcept {
    float weight = 0.0f;
    float done = 0.0f;
    if (hd_total > 0u) {
        weight += kHdWeight;
        done += kHdWeight * static_cast<float>(std::min(hd_done, hd_total)) / static_cast<float>(hd_total);
    }
    if (ram_total > 0u) {
        weight += kRamWeight;
        done += kRamWeight * static_cast<float>(std::min(ram_done, ram_total)) / static_cast<float>(ram_total);
    }
    return weight == 0.0f ? 1.0f : done / weight;
}

void LoadingScreen::configure(LoadingGallery gallery) {
    std::lock_guard lock(mutex_);
    gallery_ = std::move(gallery);
}

void LoadingScreen::note_artwork(std::uint64_t key, std::uint32_t width, std::uint32_t height,
                                 const std::uint8_t *rgba) {
    if (width != kLoadingArtWidth || height != kLoadingArtHeight || rgba == nullptr) return;
    auto pixels = std::make_shared<HdImage>();
    pixels->width = width;
    pixels->height = height;
    pixels->rgba.assign(rgba, rgba + std::size_t{width} * height * 4u);
    const bool art = looks_like_artwork(*pixels);
    std::lock_guard lock(mutex_);
    if (key != artwork_key_) artwork_hd_ = nullptr;
    artwork_key_ = key;
    artwork_is_art_ = art;
    artwork_pixels_ = std::move(pixels);
}

void LoadingScreen::note_artwork_hd(std::uint64_t key, std::shared_ptr<const HdImage> image) {
    std::lock_guard lock(mutex_);
    if (key == artwork_key_) artwork_hd_ = std::move(image);
}

void LoadingScreen::note_artwork_drawn(long long now_ms) {
    std::lock_guard lock(mutex_);
    drawn_ms_ = now_ms;
    artwork_in_frame_ = true;
    clean_frames_ = 0;
}

void LoadingScreen::note_frame_finished() {
    std::lock_guard lock(mutex_);
    if (artwork_in_frame_) {
        artwork_in_frame_ = false;
        clean_frames_ = 0;
    } else if (clean_frames_ < kCleanFramesToLeave) {
        ++clean_frames_;
    }
}

long long LoadingScreen::since_artwork_ms(long long now_ms) const {
    std::lock_guard lock(mutex_);
    return now_ms - drawn_ms_;
}

void LoadingScreen::begin_wait(long long now_ms) {
    std::lock_guard lock(mutex_);
    waiting_ = true;
    waited_ = true;
    wait_fraction_ = 0.0f;
    drawn_ms_ = std::max(drawn_ms_, now_ms);
}

void LoadingScreen::update_wait(long long, float fraction) {
    std::lock_guard lock(mutex_);
    wait_fraction_ = std::clamp(fraction, 0.0f, 1.0f);
}

void LoadingScreen::finish_wait() {
    std::lock_guard lock(mutex_);
    waiting_ = false;
}

bool LoadingScreen::active_locked(long long now_ms) const {
    if (!artwork_pixels_) return false;
    return waiting_ || (artwork_is_art_ && (now_ms - drawn_ms_ <= kLoadingArtworkHoldMs ||
                                            clean_frames_ < kCleanFramesToLeave));
}

bool LoadingScreen::active(long long now_ms) const {
    std::lock_guard lock(mutex_);
    return active_locked(now_ms);
}

float LoadingScreen::fraction(long long now_ms) const {
    std::lock_guard lock(mutex_);
    (void)now_ms;
    if (waiting_) return wait_fraction_;
    return waited_ ? 1.0f : -1.0f;
}

void LoadingScreen::show(std::shared_ptr<const HdImage> image) {
    image_ = std::move(image);
    ++version_;
}

std::pair<std::shared_ptr<const HdImage>, std::uint64_t> LoadingScreen::image(long long now_ms) {
    std::unique_lock lock(mutex_);
    const bool up = active_locked(now_ms);
    if (!up) {
        if (up_ && !waiting_) waited_ = false;  // the next loading screen starts afresh
        up_ = false;
        return {image_, version_};
    }
    const std::shared_ptr<const HdImage> first = artwork_hd_ ? artwork_hd_ : artwork_pixels_;
    if (!up_) {
        // A loading screen came up: the game's artwork, in HD when there is a copy.
        up_ = true;
        shown_since_ms_ = now_ms;
        shown_slot_ = 0;
        shown_key_ = artwork_key_;
        grey_.clear();
        show(first);
        return {image_, version_};
    }
    if (shown_key_ == artwork_key_ && image_ != first) show(first);  // its HD copy arrived meanwhile
    const long long slot = (now_ms - shown_since_ms_) / kLoadingArtworkMs;
    if (slot <= shown_slot_ || !gallery_ || !artwork_hd_) return {image_, version_};
    shown_slot_ = slot;
    // Another artwork: the HD images of its size in RAM (no file reads), the game's own once a turn, grey ones never.
    const LoadingGallery gallery = gallery_;
    const std::uint32_t width = artwork_hd_->width;
    const std::uint32_t height = artwork_hd_->height;
    lock.unlock();
    const HdGallery others = gallery(width, height);
    lock.lock();
    HdGallery ring{{artwork_key_, first}};
    for (const auto &entry : others)
        if (entry.first != artwork_key_ && entry.second && grey_.count(entry.first) == 0u) ring.push_back(entry);
    std::size_t at = 0u;
    for (std::size_t i = 0; i < ring.size(); ++i)
        if (ring[i].first == shown_key_) at = i;
    for (std::size_t step = 1; step < ring.size(); ++step) {
        const auto &[key, candidate] = ring[(at + step) % ring.size()];
        if (key != artwork_key_ && !looks_like_artwork(*candidate)) {
            grey_.insert(key);
            continue;
        }
        shown_key_ = key;
        show(candidate);
        break;
    }
    return {image_, version_};
}

LoadingScreen &loading_screen() {
    static LoadingScreen screen;
    return screen;
}

} // namespace vcsnx
