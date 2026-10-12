#include "gpu_frame_hold.hpp"

namespace vcsnx {

void step_gpu_frame_hold(bool &holding, std::uint64_t &idle_vblanks, bool gpu_frame_ready,
                         bool ge_lists_in_flight) noexcept {
    if (gpu_frame_ready) {
        holding = true;
        idle_vblanks = 0u;
    } else if (holding && ge_lists_in_flight) {
        idle_vblanks = 0u; // the GPU is busy, not silent
    } else if (holding && ++idle_vblanks > 4u) {
        holding = false;
    }
}

} // namespace vcsnx
