#include "hd_movies.hpp"

#include "display_window.hpp"
#include "vcs_media_decoder.hpp"
#include "vcsnx_platform.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <system_error>
#include <vector>

namespace vcsnx {
namespace {

// The HD copies are made at exactly this size (the decoder hands out frames of the file's own size).
constexpr std::uint32_t kWidth = 1920;
constexpr std::uint32_t kHeight = 1080;

struct HdMovie {
    vcs::VideoStreamDecoder decoder;
    std::vector<std::uint8_t> frame;
    bool active{};
    bool fresh{};
};

HdMovie &movie() {
    static HdMovie state;
    return state;
}

bool enabled() {
    const char *value = std::getenv("VCSNX_HD_MOVIES");
    return value == nullptr || std::strcmp(value, "0") != 0;
}

} // namespace

void hd_movie_open(const std::filesystem::path &source) {
    hd_movie_close();
    if (!enabled()) return;
    const std::filesystem::path path = hd_movie_path(data_directory(), source);
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) return;
    HdMovie &state = movie();
    if (!state.decoder.open(path)) {
        std::fprintf(stderr, "[hd-movie] cannot open %s\n", path.string().c_str());
        return;
    }
    state.frame.assign(std::size_t{kWidth} * kHeight * 4u, 0u);
    state.active = true;
    std::fprintf(stderr, "[hd-movie] playing %s\n", path.string().c_str());
}

void hd_movie_frame() {
    HdMovie &state = movie();
    if (!state.active) return;
    if (state.decoder.read(state.frame) < state.frame.size()) {
        std::fprintf(stderr, "[hd-movie] the HD copy ended before the movie\n");
        hd_movie_close();
        return;
    }
    state.fresh = true;
}

void hd_movie_close() {
    HdMovie &state = movie();
    state.decoder.close();
    state.active = false;
    state.fresh = false;
}

bool hd_movie_present() {
    HdMovie &state = movie();
    if (!state.active) return false;
    if (state.fresh) {
        vcs::display_window_present_rgba(std::as_bytes(std::span(state.frame)), kWidth, kHeight);
        state.fresh = false;
    }
    return true;
}

} // namespace vcsnx
