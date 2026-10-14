// HD replacement textures are decoded into RAM at startup, so the game never waits on the SD for them.
#include "hd_preload.hpp"
#include "test_support.hpp"

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

namespace fs = std::filesystem;

struct TempDir {
    fs::path path;
    TempDir() {
        path = fs::temp_directory_path() / ("vcsnx-hdpre-" + std::to_string(std::rand()));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    void file(const std::string &name, std::uint32_t side) const {
        std::ofstream(path / name) << side; // the fake decoder reads the side back
    }
};

// Decodes the side written by TempDir::file into a side x side image; a file that says 0 fails to decode.
vcsnx::HdDecoder fake_decoder(std::atomic<int> *calls = nullptr) {
    return [calls](const std::string &file) -> std::optional<vcsnx::HdImage> {
        if (calls != nullptr) ++*calls;
        std::uint32_t side = 0;
        std::ifstream(file) >> side;
        if (side == 0u) return std::nullopt;
        vcsnx::HdImage image;
        image.width = image.height = side;
        image.rgba.assign(static_cast<std::size_t>(side) * side * 4u, static_cast<std::uint8_t>(side));
        return image;
    };
}

} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"a replacement's key is its sixteen hex digits", [] {
             require(vcsnx::hd_key_of("0123456789abcdef.png") == 0x0123456789abcdefull, "lowercase");
             require(vcsnx::hd_key_of("0123456789ABCDEF.png") == 0x0123456789abcdefull, "uppercase");
             require(!vcsnx::hd_key_of("0123456789abcde.png"), "fifteen digits");
             require(!vcsnx::hd_key_of("0123456789abcdeg.png"), "not hex");
             require(!vcsnx::hd_key_of("0123456789abcdef.png.usage"), "not a png");
             require(!vcsnx::hd_key_of("notes.txt"), "something else");
         }},
        {"every replacement ends up in RAM", [] {
             TempDir dir;
             dir.file("00000000000000aa.png", 8u);
             dir.file("00000000000000bb.png", 16u);
             dir.file("readme.txt", 4u);
             vcsnx::HdPreload preload;
             preload.start(dir.path.string(), 1u << 20, 2u, fake_decoder());
             preload.wait();
             const vcsnx::HdPreloadProgress p = preload.progress();
             require(p.finished && p.total == 2u && p.done == 2u, "two of two");
             require(p.bytes == 8u * 8u * 4u + 16u * 16u * 4u, "their size");
             const auto image = preload.find(0xbbu);
             require(image != nullptr && image->width == 16u && image->rgba.size() == 16u * 16u * 4u, "the 16 x 16 one");
             require(preload.find(0xccu) == nullptr, "not there");
         }},
        {"past the budget the rest stays on the SD", [] {
             TempDir dir;
             dir.file("00000000000000a1.png", 16u);
             dir.file("00000000000000a2.png", 16u);
             dir.file("00000000000000a3.png", 16u);
             vcsnx::HdPreload preload;
             preload.start(dir.path.string(), 16u * 16u * 4u * 2u, 1u, fake_decoder());
             preload.wait();
             const vcsnx::HdPreloadProgress p = preload.progress();
             require(p.finished && p.done == 3u, "all looked at");
             require(p.bytes <= 16u * 16u * 4u * 2u, "never over the budget");
             int kept = 0;
             for (const std::uint64_t key : {0xa1ull, 0xa2ull, 0xa3ull}) kept += preload.find(key) != nullptr;
             require(kept == 2, "two kept");
         }},
        {"the images in RAM of one size, for the loading screen's other artworks", [] {
             TempDir dir;
             dir.file("00000000000000a2.png", 16u);
             dir.file("00000000000000a1.png", 16u);
             dir.file("00000000000000b1.png", 8u);
             vcsnx::HdPreload preload;
             preload.start(dir.path.string(), 1u << 20, 1u, fake_decoder());
             preload.wait();
             const auto sixteen = preload.images_of_size(16u, 16u);
             require(sixteen.size() == 2u && sixteen[0].first == 0xa1u && sixteen[0].second == preload.find(0xa1u) &&
                         sixteen[1].first == 0xa2u,
                     "both 16 x 16, by key");
             require(preload.images_of_size(32u, 32u).empty(), "none of another size");
         }},
        {"a file that does not decode is skipped", [] {
             TempDir dir;
             dir.file("00000000000000d0.png", 0u);
             dir.file("00000000000000d1.png", 4u);
             vcsnx::HdPreload preload;
             preload.start(dir.path.string(), 1u << 20, 1u, fake_decoder());
             preload.wait();
             require(preload.progress().done == 2u, "both looked at");
             require(preload.find(0xd0u) == nullptr && preload.find(0xd1u) != nullptr, "only the good one");
         }},
        {"it serves only the folder it loaded", [] {
             TempDir dir;
             vcsnx::HdPreload preload;
             require(!preload.serves(dir.path.string()), "not started");
             preload.start(dir.path.string(), 1u << 20, 1u, fake_decoder());
             preload.wait();
             require(preload.serves(dir.path.string()), "its folder");
             require(preload.serves(dir.path.string() + "/"), "with a slash");
             require(!preload.serves((dir.path / "other").string()), "another folder");
         }},
        {"starting twice loads once, and a missing folder finishes empty", [] {
             TempDir dir;
             dir.file("00000000000000e1.png", 4u);
             std::atomic<int> calls{0};
             vcsnx::HdPreload preload;
             preload.start(dir.path.string(), 1u << 20, 1u, fake_decoder(&calls));
             preload.start(dir.path.string(), 1u << 20, 1u, fake_decoder(&calls));
             preload.wait();
             require(calls == 1, "one decode");
             vcsnx::HdPreload none;
             none.start((dir.path / "missing").string(), 1u << 20, 1u, fake_decoder());
             none.wait();
             require(none.progress().finished && none.progress().total == 0u, "nothing to do");
         }},
        {"configured at startup, nothing loads until the loading screen starts it", [] {
             // The author (2026-10-14): "en la parte de los artworks cargas todo. no en los videos". On the
             // Switch the workers took the game's core while the movies played and the screen stayed black.
             TempDir dir;
             dir.file("00000000000000f1.png", 4u);
             std::atomic<int> calls{0};
             vcsnx::HdPreload preload;
             preload.configure(dir.path.string(), 1u << 20, 1u, fake_decoder(&calls));
             require(calls == 0 && !preload.serves(dir.path.string()) && preload.progress().total == 0u,
                     "nothing read or decoded yet");
             preload.start();
             preload.wait();
             require(preload.done(), "done once every file was looked at");
             require(calls == 1 && preload.serves(dir.path.string()) && preload.find(0xf1u) != nullptr, "loaded");
         }},
        {"starting what was never configured does nothing", [] {
             vcsnx::HdPreload preload;
             preload.start();
             preload.wait();
             require(preload.progress().total == 0u && !preload.serves("/any") && preload.done(), "no preload");
         }},
        {"the loading screen waits at the first read of a level archive", [] {
             require(vcsnx::waits_for_hd_preload("sdmc:/switch/vcsnx/PSP_DATA/PSP_GAME/USRDIR/RUNDATA/MAINLA.LVZ"), "MAINLA.LVZ");
             require(vcsnx::waits_for_hd_preload("RUNDATA/beach.lvz"), "lowercase");
             require(!vcsnx::waits_for_hd_preload("RUNDATA/MAINLA.IMG"), "the sectors stream later");
             require(!vcsnx::waits_for_hd_preload("RUNDATA/LOADSC0.XTX"), "the loading screen's own art");
         }},
        {"an HD copy fits the game's texture when it is a power-of-two larger, up to 8192", [] {
             require(vcsnx::hd_scale_shift(1024u, 512u, 512u, 256u) == 1u, "2x");
             require(vcsnx::hd_scale_shift(512u, 256u, 512u, 256u) == 0u, "the same size");
             require(vcsnx::hd_scale_shift(2048u, 1024u, 256u, 128u) == 3u, "8x");
             require(!vcsnx::hd_scale_shift(1000u, 512u, 512u, 256u), "not a power-of-two multiple");
             require(!vcsnx::hd_scale_shift(1024u, 1024u, 512u, 256u), "another shape");
             require(!vcsnx::hd_scale_shift(16384u, 8192u, 2048u, 1024u), "too big");
         }},
        {"interface art gets its HD copy at upload only in menus and loading screens", [] {
             // Switch, 2026-10-14: at once for all interface art, the radar's HD map tiles went up on the GE thread
             // while driving, and slow GE lists went from 1.4 to 20 every 10 s ("el pop in volvió").
             require(vcsnx::hd_copy_at_upload(true, false), "a menu or a loading screen");
             require(!vcsnx::hd_copy_at_upload(true, true), "playing: in the background as before");
             require(!vcsnx::hd_copy_at_upload(false, false), "world textures: in the background");
         }},
        {"VCSNX_HD_PRELOAD=0 turns it off", [] {
             require(vcsnx::hd_preload_enabled(nullptr), "on by default");
             require(vcsnx::hd_preload_enabled("1"), "1");
             require(!vcsnx::hd_preload_enabled("0"), "0");
         }},
    });
}
