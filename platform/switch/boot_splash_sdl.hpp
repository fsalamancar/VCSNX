#pragma once
// Plays the boot splash (boot_splash.hpp) with SDL. Built only when the user's logo is there
// (cmake/vcsnx_splash.cmake defines VCSNX_HAS_SPLASH).

#include "boot_splash.hpp"

#include <cstddef>

struct SDL_Renderer;

namespace vcsnx {

// Defined by the splash_data.cpp CMake generates from assets/splash/ (tools/make_splash.py).
extern const unsigned char kSplashPng[];
extern const std::size_t kSplashPngSize;
extern const unsigned char kSplashWav[]; // a .wav file; kSplashWavSize is 0 without one
extern const std::size_t kSplashWavSize;
extern const SplashPiece kSplashPieces[];
extern const std::size_t kSplashPieceCount;

// Plays the splash and its sound on the main thread before the game starts, so the game's intro is
// not heard over it. A or + skips to the fade-out. Returns false when the window was closed meanwhile.
bool play_boot_splash(SDL_Renderer *renderer);

} // namespace vcsnx
