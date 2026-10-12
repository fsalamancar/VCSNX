#pragma once
// Platform controller source. start/pump/shutdown run on the main thread (the one that owns
// SDL); read may be called from any thread and returns the sample taken by the last pump.

#include "pad_mapping.hpp"

namespace vcsnx {

void pad_source_start();
void pad_source_pump();
[[nodiscard]] PadSample pad_source_read() noexcept;
void pad_source_shutdown();

} // namespace vcsnx
