#pragma once
#include <cstdint>

namespace vcsnx {

// One vblank of vcs_profile.cpp's hold on the last GPU frame. The Vulkan path never draws the
// guest framebuffer in RAM, so showing it instead of the last GPU frame flickers. Upstream lets go
// after 4 vblanks without a new GPU frame (menus, videos); with the GE on its own thread a Switch
// frame takes 3-8 vblanks, so vblanks with display lists in flight do not count as idle.
void step_gpu_frame_hold(bool &holding, std::uint64_t &idle_vblanks, bool gpu_frame_ready,
                         bool ge_lists_in_flight) noexcept;

// Whether the vblank loop hands the GPU frame to the window. A held frame is already on screen
// (the window keeps its last picture), so only a new one is published.
[[nodiscard]] constexpr bool gpu_frame_needs_publish(bool gpu_frame_ready) noexcept { return gpu_frame_ready; }

} // namespace vcsnx
