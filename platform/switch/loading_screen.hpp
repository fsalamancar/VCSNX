#pragma once
#include "hd_preload.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <utility>
#include <vector>

namespace vcsnx {

// VCSNX's own loading screen. The author, 2026-10-14: the wait looked frozen, its artwork was the low-resolution one
// ("quiero que siempre esté en buena textura. y vaya pasando artworks. cada 7seg ... o que la barrita se mueva"), and
// the game's own screen showed just before and after it. So the window draws this instead of the game's frames
// whenever the game shows its loading artwork, and while the game waits for the RAM loads (vcs_profile.cpp) and draws
// nothing: the game's artwork in HD, another one every kLoadingArtworkMs from the HD images in RAM, and a bar.

inline constexpr long long kLoadingArtworkMs = 7000;
// The screen stays up this long after the artwork was last drawn (the game draws it every frame), and until the
// game has finished this many frames without it: the window then shows the game's latest frame, and until then that
// is its own loading screen (the author, 2026-10-14: it still showed after VCSNX's). Three, as the GPU can hand
// over the previous frame.
inline constexpr long long kLoadingArtworkHoldMs = 200;
inline constexpr int kCleanFramesToLeave = 3;
// The game's loading artworks are 512 x 256 textures (LOADSC*.XTX).
inline constexpr std::uint32_t kLoadingArtWidth = 512;
inline constexpr std::uint32_t kLoadingArtHeight = 256;

// Whether a texture the game uploads gets its HD copy read from its file right then: the loading artworks, which
// come up before the preload has them (it starts on their screen), so the PSP's never shows (2026-10-14).
[[nodiscard]] bool reads_hd_artwork_now(std::uint32_t width, std::uint32_t height, bool preload_serves) noexcept;
// Whether an image is one of the artworks, which are in strong colour: the other two pictures of their size, the
// memory card collage and the controller (2026-10-14), are grey.
[[nodiscard]] bool looks_like_artwork(const HdImage &image) noexcept;
// How far the loads are, 0 to 1. The HD textures weigh four times the city's archives (about 37 s against 8 s on
// the Switch, 2026-10-14); a load with nothing to do does not count.
[[nodiscard]] float loading_fraction(std::size_t hd_done, std::size_t hd_total, std::uint64_t ram_done,
                                     std::uint64_t ram_total) noexcept;

// The HD images in RAM of a size, by key (vcsnx::HdPreload::images_of_size).
using LoadingGallery = std::function<HdGallery(std::uint32_t width, std::uint32_t height)>;

class LoadingScreen {
public:
    // Where the other artworks come from.
    void configure(LoadingGallery gallery);
    // The texture pipeline (any thread): a texture of the artworks' size was decoded, with its replacement key and
    // its own pixels; and its HD copy, when the upload made one.
    void note_artwork(std::uint64_t key, std::uint32_t width, std::uint32_t height, const std::uint8_t *rgba);
    void note_artwork_hd(std::uint64_t key, std::shared_ptr<const HdImage> image);
    // The GE thread: the game drew its loading artwork.
    void note_artwork_drawn(long long now_ms);
    // The vblank thread: the game finished a frame (ge_gpu_backend_finish_color_frame).
    void note_frame_finished();
    // Milliseconds since the artwork was last drawn (for the window's log).
    [[nodiscard]] long long since_artwork_ms(long long now_ms) const;
    // The game thread, while it waits for the RAM loads (the game draws nothing meanwhile).
    void begin_wait(long long now_ms);
    void update_wait(long long now_ms, float fraction);
    void finish_wait();

    // The main thread: whether the screen is up, the bar (0..1, or below 0 for a bar that moves on its own: a
    // loading screen with no wait to follow), and the image with a number that changes with it.
    [[nodiscard]] bool active(long long now_ms) const;
    [[nodiscard]] float fraction(long long now_ms) const;
    [[nodiscard]] std::pair<std::shared_ptr<const HdImage>, std::uint64_t> image(long long now_ms);

private:
    bool active_locked(long long now_ms) const;
    void show(std::shared_ptr<const HdImage> image);

    mutable std::mutex mutex_;
    LoadingGallery gallery_;
    std::uint64_t artwork_key_{};
    bool artwork_is_art_{};
    std::shared_ptr<const HdImage> artwork_pixels_;  // the game's own artwork
    std::shared_ptr<const HdImage> artwork_hd_;      // its HD copy
    long long drawn_ms_{-1'000'000};
    bool artwork_in_frame_{};                  // drawn since the last finished frame
    int clean_frames_{kCleanFramesToLeave};    // finished frames without it since
    bool waiting_{};
    float wait_fraction_{};
    bool waited_{};             // a wait happened while the screen was up
    bool up_{};                 // the screen was up at the last image() call
    long long shown_since_ms_{};
    long long shown_slot_{};
    std::uint64_t shown_key_{};
    std::set<std::uint64_t> grey_;  // pictures of the artworks' size that are not artworks
    std::shared_ptr<const HdImage> image_;
    std::uint64_t version_{};
};

LoadingScreen &loading_screen();

} // namespace vcsnx
