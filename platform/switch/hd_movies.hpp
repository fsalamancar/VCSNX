#pragma once
// HD copies of the game's movies (docs/backlog.md item 10, prototype). When HD/<NAME>.mp4 exists next to
// vcsnx.nro (1920x1080, the same frames as the PSP's 480x272 <NAME>.PMF), the runtime decodes it one frame
// each time the game decodes one, and shows it instead of the 480x272 picture. VCSNX_HD_MOVIES=0 turns it off.

#include <filesystem>
#include <string>

namespace vcsnx {

[[nodiscard]] inline std::filesystem::path hd_movie_path(const std::string &data_directory,
                                                         const std::filesystem::path &source) {
    return std::filesystem::path(data_directory) / "HD" / (source.stem().string() + ".mp4");
}

// Called by the vendor's movie decoder (vcs_profile.cpp): a movie opened, the game decoded one frame of it,
// the movie closed.
void hd_movie_open(const std::filesystem::path &source);
void hd_movie_frame();
void hd_movie_close();

// At present time with a movie frame on screen: shows the newest HD frame, or keeps the last one. False when
// the movie has no HD copy, so the caller shows the game's own picture.
[[nodiscard]] bool hd_movie_present();

} // namespace vcsnx
