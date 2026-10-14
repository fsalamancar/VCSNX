// VCSNX's own loading screen, up whenever the game shows its loading artwork and while the game waits for the RAM
// loads: the game's artwork in HD, another one every 7 s from the HD images in RAM, and a bar that moves.
#include "loading_screen.hpp"
#include "test_support.hpp"

#include <string>

namespace {

std::shared_ptr<const vcsnx::HdImage> picture(std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
    auto image = std::make_shared<vcsnx::HdImage>();
    image->width = 1024u;
    image->height = 512u;
    image->rgba = {red, green, blue, 255};
    return image;
}

vcsnx::HdImage pixels(std::vector<std::uint8_t> rgba) {
    vcsnx::HdImage image;
    image.width = static_cast<std::uint32_t>(rgba.size() / 4u);
    image.height = 1u;
    image.rgba = std::move(rgba);
    return image;
}

// The game's own artwork, 512 x 256, in colour or grey.
std::vector<std::uint8_t> original(bool colour) {
    std::vector<std::uint8_t> rgba(vcsnx::kLoadingArtWidth * vcsnx::kLoadingArtHeight * 4u);
    for (std::size_t i = 0; i < rgba.size(); i += 4u) {
        rgba[i] = colour ? 200 : 90;
        rgba[i + 1] = colour ? 60 : 90;
        rgba[i + 2] = colour ? 40 : 90;
        rgba[i + 3] = 255;
    }
    return rgba;
}

// The game finishing `count` frames (vblank thread), the first one holding the artwork drawn just before.
void game_frames(vcsnx::LoadingScreen &screen, int count) {
    for (int i = 0; i < count; ++i) screen.note_frame_finished();
}

} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"the artworks are in colour, the memory card and controls screens grey", [] {
             // Mac and Switch, 2026-10-14: 12 of the 14 HD images of the artworks' size are artworks; the memory
             // card collage and the controller picture came up in the rotation too.
             require(vcsnx::looks_like_artwork(pixels({200, 60, 40, 255, 30, 120, 200, 255})), "colour");
             require(!vcsnx::looks_like_artwork(pixels({90, 90, 90, 255, 20, 22, 21, 255})), "grey");
             require(!vcsnx::looks_like_artwork(pixels({})), "empty");
         }},
        {"the bar follows both loads, the HD textures weighing most", [] {
             require(vcsnx::loading_fraction(0u, 100u, 0u, 1000u) == 0.0f, "nothing yet");
             require(vcsnx::loading_fraction(100u, 100u, 1000u, 1000u) == 1.0f, "all done");
             require(vcsnx::loading_fraction(0u, 100u, 1000u, 1000u) == 0.2f, "the archives first");
             require(vcsnx::loading_fraction(50u, 100u, 0u, 0u) == 0.5f, "only HD textures");
             require(vcsnx::loading_fraction(0u, 0u, 500u, 1000u) == 0.5f, "no HD folder");
             require(vcsnx::loading_fraction(0u, 0u, 0u, 0u) == 1.0f, "nothing to load");
         }},
        {"a loading artwork's HD copy is read from its file before the preload has it", [] {
             // The author, 2026-10-14: the PSP's artwork showed before and after VCSNX's screen. The preload starts
             // on that very screen, so the artwork's HD copy is read on its own when the game uploads it.
             require(vcsnx::reads_hd_artwork_now(vcsnx::kLoadingArtWidth, vcsnx::kLoadingArtHeight, false), "artwork");
             require(!vcsnx::reads_hd_artwork_now(vcsnx::kLoadingArtWidth, vcsnx::kLoadingArtHeight, true),
                     "the preload has it in RAM by then");
             require(!vcsnx::reads_hd_artwork_now(256u, 128u, false), "other interface art waits for the preload");
         }},
        {"the screen is up while the game draws its loading artwork, never for a grey picture", [] {
             // The author, 2026-10-14: the game's own loading screen showed just before and after VCSNX's wait.
             vcsnx::LoadingScreen screen;
             const auto art = original(true);
             screen.note_artwork(0xb2u, vcsnx::kLoadingArtWidth, vcsnx::kLoadingArtHeight, art.data());
             require(!screen.active(1000), "not before it is drawn");
             screen.note_artwork_drawn(1000);
             require(screen.active(1100), "while it is drawn");
             game_frames(screen, 1 + vcsnx::kCleanFramesToLeave);
             require(!screen.active(1000 + vcsnx::kLoadingArtworkHoldMs + 1), "gone with it");
             const auto memcard = original(false);
             screen.note_artwork(0x30u, vcsnx::kLoadingArtWidth, vcsnx::kLoadingArtHeight, memcard.data());
             screen.note_artwork_drawn(2000);
             require(!screen.active(2050), "the memory card screen is the game's");
         }},
        {"through the wait the bar follows the loads, then stays full until the artwork goes", [] {
             vcsnx::LoadingScreen screen;
             const auto art = original(true);
             screen.note_artwork(0xb2u, vcsnx::kLoadingArtWidth, vcsnx::kLoadingArtHeight, art.data());
             screen.note_artwork_drawn(900);
             screen.begin_wait(1000);
             require(screen.active(30000), "up through the wait, the game drawing nothing");
             screen.update_wait(2000, 0.4f);
             require(screen.fraction(2000) == 0.4f, "the loads' share");
             screen.finish_wait();
             screen.note_artwork_drawn(40000);
             require(screen.active(40100) && screen.fraction(40100) == 1.0f, "full while the game finishes loading");
             game_frames(screen, 1 + vcsnx::kCleanFramesToLeave);
             require(!screen.active(40000 + vcsnx::kLoadingArtworkHoldMs + 1), "gone with the artwork");
         }},
        {"after the artwork the screen stays up until the game has frames of its own", [] {
             // The author, 2026-10-14 (nro 8a9b0ea1): the PSP's loading screen still showed. 200 ms after the last
             // artwork draw the window went back to the game's latest frame, which was still its loading screen
             // while the game finished loading and drew nothing new.
             vcsnx::LoadingScreen screen;
             const auto art = original(true);
             screen.note_artwork(0xb2u, vcsnx::kLoadingArtWidth, vcsnx::kLoadingArtHeight, art.data());
             screen.note_artwork_drawn(1000);
             game_frames(screen, 1);  // the frame with the artwork
             require(screen.active(5000), "no frame of the game's own yet, seconds later");
             game_frames(screen, vcsnx::kCleanFramesToLeave - 1);
             require(screen.active(5000), "one short (the GPU can hand over the previous frame)");
             game_frames(screen, 1);
             require(!screen.active(5000), "the game's own frames");
         }},
        {"a frame with the artwork again starts the count over", [] {
             vcsnx::LoadingScreen screen;
             const auto art = original(true);
             screen.note_artwork(0xb2u, vcsnx::kLoadingArtWidth, vcsnx::kLoadingArtHeight, art.data());
             screen.note_artwork_drawn(1000);
             game_frames(screen, vcsnx::kCleanFramesToLeave);
             screen.note_artwork_drawn(2000);
             game_frames(screen, vcsnx::kCleanFramesToLeave);
             require(screen.active(5000), "the artwork came back");
             game_frames(screen, 1);
             require(!screen.active(5000), "then the game's frames");
         }},
        {"frames before any artwork do not bring the screen up", [] {
             vcsnx::LoadingScreen screen;
             const auto art = original(true);
             screen.note_artwork(0xb2u, vcsnx::kLoadingArtWidth, vcsnx::kLoadingArtHeight, art.data());
             game_frames(screen, 10);
             require(!screen.active(100), "uploaded, never drawn");
         }},
        {"a later loading screen, with no wait, has a bar that moves on its own", [] {
             vcsnx::LoadingScreen screen;
             const auto art = original(true);
             screen.note_artwork(0xb2u, vcsnx::kLoadingArtWidth, vcsnx::kLoadingArtHeight, art.data());
             screen.note_artwork_drawn(1000);
             require(screen.fraction(1000) < 0.0f, "a moving bar");
         }},
        {"the game's artwork in HD, then every 7 s another from RAM, never a grey one", [] {
             const auto blue = picture(30, 90, 220);
             const auto grey = picture(90, 90, 90);
             const auto green = picture(40, 200, 60);
             vcsnx::LoadingScreen screen;
             screen.configure([&](std::uint32_t width, std::uint32_t height) {
                 require(width == 1024u && height == 512u, "asked for the HD artwork's size");
                 return vcsnx::HdGallery{{0xa1u, blue}, {0xa2u, grey}, {0xa3u, green}, {0xb2u, picture(200, 60, 40)}};
             });
             const auto art = original(true);
             screen.note_artwork(0xb2u, vcsnx::kLoadingArtWidth, vcsnx::kLoadingArtHeight, art.data());
             screen.note_artwork_hd(0xb2u, picture(200, 60, 40));
             for (long long t = 1000; t <= 40000; t += 100) screen.note_artwork_drawn(t);
             const auto [first, v0] = screen.image(1000);
             require(first && first->width == 1024u && first->rgba[0] == 200u, "the HD copy of the game's artwork");
             require(screen.image(5000).second == v0, "the same artwork before 7 s");
             require(screen.image(8000).first == blue, "another at 7 s");
             require(screen.image(15000).first == green, "and another at 14 s, the grey one skipped");
             require(screen.image(22000).first->rgba[0] == 200u, "round again to the game's own");
             require(screen.image(29000).first == blue, "and on");
         }},
        {"without an HD copy the screen shows the game's own artwork", [] {
             vcsnx::LoadingScreen screen;
             const auto art = original(true);
             screen.note_artwork(0xb2u, vcsnx::kLoadingArtWidth, vcsnx::kLoadingArtHeight, art.data());
             screen.note_artwork_drawn(0);
             const auto [image, version] = screen.image(0);
             require(image && image->width == vcsnx::kLoadingArtWidth && image->rgba[0] == 200u, "the original pixels");
             require(screen.image(7000).second == version, "nothing to rotate");
         }},
        {"only textures of the artworks' size count as the artwork", [] {
             vcsnx::LoadingScreen screen;
             const std::vector<std::uint8_t> small(64u * 64u * 4u, 200u);
             screen.note_artwork(0x11u, 64u, 64u, small.data());
             screen.note_artwork_drawn(0);
             require(!screen.active(10) && screen.image(10).first == nullptr, "no artwork");
         }},
    });
}
