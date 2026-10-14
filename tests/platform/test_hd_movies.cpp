// Where the HD copy of each of the game's movies lives (docs/backlog.md item 10): next to vcsnx.nro, in HD/,
// named after the PSP movie.
#include "hd_movies.hpp"
#include "test_support.hpp"

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"a movie's HD copy is HD/<name>.mp4 in the data folder", [] {
             const auto path = vcsnx::hd_movie_path("sdmc:/switch/vcsnx",
                                                    "sdmc:/switch/vcsnx/PSP_DATA/PSP_GAME/USRDIR/RUNDATA/PSP/MOVIES/TITLES.PMF");
             require(path.generic_string() == "sdmc:/switch/vcsnx/HD/TITLES.mp4", path.generic_string());
         }},
        {"the PSP name is kept as it is", [] {
             require(vcsnx::hd_movie_path("/data", "/data/PSP_DATA/logo.pmf").generic_string() == "/data/HD/logo.mp4",
                     "lower-case name");
         }},
    });
}
