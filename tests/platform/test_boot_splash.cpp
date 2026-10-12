// The boot splash timeline: the logo builds up (the green pieces light up like a neon tube), glows,
// glitches like a VHS tape, then blurs and fades to black.
#include "boot_splash.hpp"
#include "test_support.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using vcsnx::SplashDraw;
using vcsnx::SplashLayer;
using vcsnx::SplashPiece;
using vcsnx::SplashRole;

constexpr float kLogoW = 100.0f;
constexpr float kLogoH = 50.0f;
constexpr vcsnx::SplashScreen kScreen{1280.0f, 720.0f};

const std::vector<SplashPiece> kPieces{
    {SplashRole::SlideLeft, 0, {0.0f, 25.0f, 20.0f, 50.0f}},
    {SplashRole::Neon, 0, {20.0f, 25.0f, 50.0f, 50.0f}},
    {SplashRole::Neon, 0, {50.0f, 25.0f, 60.0f, 50.0f}},
    {SplashRole::SlideRight, 0, {60.0f, 25.0f, 100.0f, 50.0f}},
    {SplashRole::Drop, 0, {0.0f, 0.0f, 50.0f, 25.0f}},
    {SplashRole::Drop, 1, {50.0f, 0.0f, 100.0f, 25.0f}},
};

std::vector<SplashDraw> frame(float t) { return vcsnx::splash_frame(t, kPieces, kLogoW, kLogoH, kScreen); }

bool near(float a, float b) { return std::fabs(a - b) < 0.01f; }

bool untinted(const SplashDraw &draw) { return draw.tint.r == 1.0f && draw.tint.g == 1.0f && draw.tint.b == 1.0f; }

// The untinted sharp draw of a piece, if the frame has one.
const SplashDraw *piece_draw(const std::vector<SplashDraw> &draws, const SplashPiece &piece) {
    for (const SplashDraw &draw : draws)
        if (draw.layer == SplashLayer::Sharp && untinted(draw) && near(draw.source.x0, piece.box.x0) &&
            near(draw.source.x1, piece.box.x1) && near(draw.source.y1, piece.box.y1))
            return &draw;
    return nullptr;
}

float layer_intensity(const std::vector<SplashDraw> &draws, SplashLayer layer) {
    float total = 0.0f;
    for (const SplashDraw &draw : draws)
        if (draw.layer == layer) total += draw.intensity;
    return total;
}

std::vector<const SplashDraw *> tinted(const std::vector<SplashDraw> &draws, float r, float g, float b) {
    std::vector<const SplashDraw *> out;
    for (const SplashDraw &draw : draws)
        if (draw.layer == SplashLayer::Sharp && draw.tint.r == r && draw.tint.g == g && draw.tint.b == b)
            out.push_back(&draw);
    return out;
}

// Where a logo point rests: the logo is centred and spans 54% of the screen width (6.912 px per logo px).
float rest_x(float x) { return 640.0f + (x - 50.0f) * 6.912f; }
float rest_y(float y) { return 360.0f + (y - 25.0f) * 6.912f; }

} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"the splash starts and ends on black", [] {
             require(frame(0.0f).empty(), "nothing is drawn at the first frame");
             require(frame(vcsnx::kSplashSeconds).empty(), "nothing is drawn once it is over");
             for (const SplashDraw &draw : frame(vcsnx::kSplashSeconds - 0.02f))
                 require(draw.intensity < 0.02f, "the last frame is almost black");
         }},
        {"every piece rests centred at full brightness while the logo holds", [] {
             const auto draws = frame(2.2f);
             for (const SplashPiece &piece : kPieces) {
                 const SplashDraw *draw = piece_draw(draws, piece);
                 require(draw != nullptr, "piece drawn whole");
                 require(near(draw->intensity, 1.0f), "full brightness");
                 require(near(draw->target.x0, rest_x(piece.box.x0)) && near(draw->target.x1, rest_x(piece.box.x1)),
                         "x at rest: " + std::to_string(draw->target.x0));
                 require(near(draw->target.y0, rest_y(piece.box.y0)) && near(draw->target.y1, rest_y(piece.box.y1)),
                         "y at rest: " + std::to_string(draw->target.y0));
             }
         }},
        {"the outer pieces slide in from their sides", [] {
             const auto draws = frame(0.3f);
             const SplashDraw *left = piece_draw(draws, kPieces[0]);
             const SplashDraw *right = piece_draw(draws, kPieces[3]);
             require(left != nullptr && right != nullptr, "both drawn while sliding");
             require(left->target.x0 < rest_x(kPieces[0].box.x0), "left piece comes from the left");
             require(right->target.x0 > rest_x(kPieces[3].box.x0), "right piece comes from the right");
             require(left->intensity > 0.0f && left->intensity < 1.0f, "and fades in");
         }},
        {"the green pieces light up like a neon tube", [] {
             const auto lit = [](float t) {
                 const auto draws = frame(t);
                 const SplashDraw *neon = piece_draw(draws, kPieces[1]);
                 if (neon != nullptr)
                     require(near(neon->target.x0, rest_x(kPieces[1].box.x0)) && near(neon->target.y0, rest_y(25.0f)),
                             "a neon tube does not move");
                 return neon == nullptr ? 0.0f : neon->intensity;
             };
             require(lit(0.6f) == 0.0f, "off before it starts");
             require(near(lit(0.72f), 1.0f), "first flash");
             require(lit(0.77f) == 0.0f, "off again");
             require(lit(0.81f) > 0.0f && lit(0.81f) < 1.0f, "a weak flicker");
             require(near(lit(1.2f), 1.0f), "then it stays on");
         }},
        {"the top pieces drop in one after another", [] {
             const auto draws = frame(1.3f);
             const SplashDraw *first = piece_draw(draws, kPieces[4]);
             const SplashDraw *second = piece_draw(draws, kPieces[5]);
             require(first != nullptr, "the first is falling");
             require(first->target.y0 < rest_y(0.0f), "from above");
             require(second == nullptr || second->intensity < first->intensity, "the second follows it");
         }},
        {"the green glow follows the neon and pulses while the logo holds", [] {
             require(layer_intensity(frame(0.6f), SplashLayer::Glow) == 0.0f, "no glow while the tube is off");
             require(layer_intensity(frame(0.72f), SplashLayer::Glow) > 0.0f, "the tube glows when lit");
             require(layer_intensity(frame(2.3f), SplashLayer::Glow) > layer_intensity(frame(1.3f), SplashLayer::Glow),
                     "it swells at the hold");
             require(layer_intensity(frame(2.9f), SplashLayer::Glow) == 0.0f, "no glow in the glitch");
         }},
        {"the glitch splits the logo into red, green and blue copies", [] {
             const auto draws = frame(2.90f);
             const auto red = tinted(draws, 1.0f, 0.0f, 0.0f);
             const auto green = tinted(draws, 0.0f, 1.0f, 0.0f);
             const auto blue = tinted(draws, 0.0f, 0.0f, 1.0f);
             require(!green.empty() && red.size() == green.size() && blue.size() == green.size(),
                     "one copy of each colour per band");
             for (std::size_t i = 0; i < green.size(); ++i) {
                 require(near(red[i]->source.y0, green[i]->source.y0), "the copies of a band line up");
                 require(red[i]->target.x0 < green[i]->target.x0 && green[i]->target.x0 < blue[i]->target.x0,
                         "red left, blue right");
             }
         }},
        {"torn glitch frames shift bands sideways", [] {
             const auto steady_frame = frame(2.90f);
             const auto steady = tinted(steady_frame, 0.0f, 1.0f, 0.0f);
             require(!steady.empty(), "the glitch draws its bands");
             for (const SplashDraw *band : steady) require(near(band->target.x0, rest_x(0.0f)), "a steady frame is whole");
             const auto torn_frame = frame(2.85f);
             const auto torn = tinted(torn_frame, 0.0f, 1.0f, 0.0f);
             bool shifted = false;
             for (const SplashDraw *band : torn) shifted = shifted || !near(band->target.x0, rest_x(0.0f));
             require(shifted, "a torn frame moves some bands");
         }},
        {"the logo blurs as it fades out", [] {
             const auto draws = frame(3.4f);
             require(layer_intensity(draws, SplashLayer::Blurred) > 0.2f, "the blurred logo shows");
             require(layer_intensity(draws, SplashLayer::Sharp) < 0.5f, "the sharp logo fades");
         }},
        {"skipping jumps to the fade-out", [] {
             require(near(vcsnx::splash_skip(0.5f), vcsnx::kSplashFadeOutStart), "from the build-up");
             require(near(vcsnx::splash_skip(2.9f), vcsnx::kSplashFadeOutStart), "from the glitch");
             require(near(vcsnx::splash_skip(3.2f), 3.2f), "a fade-out in progress continues");
         }},
        {"a skipped sound fades from full to silence", [] {
             std::vector<std::int16_t> stereo(8, 1000); // 4 frames, 2 channels
             vcsnx::splash_fade_out(stereo, 2);
             require(stereo[0] == 1000 && stereo[1] == 1000, "starts at full volume");
             require(stereo[6] == 0 && stereo[7] == 0, "ends silent");
             for (std::size_t i = 0; i + 2 < stereo.size(); i += 2) {
                 require(stereo[i] == stereo[i + 1], "both channels of a frame fade together");
                 require(stereo[i + 2] <= stereo[i], "never gets louder");
             }
         }},
    });
}
