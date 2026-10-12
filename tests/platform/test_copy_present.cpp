// A frame made only of strips that sample one VRAM texture is a composition copy: present its source
// when the GPU drew it this vblank, otherwise keep the previous picture (Switch run 12: releasing an
// undrawn copy showed loading screens black with a red stroke).
#include "copy_present.hpp"
#include "test_support.hpp"

#include <cstdint>

int main() {
    using vcsnx::CopyOnlyPresent;
    using vcsnx::decide_copy_only_present;
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"a copy of a source not drawn this vblank keeps the previous picture", [] {
             require(decide_copy_only_present(false) == CopyOnlyPresent::Hold, "hold");
         }},
        {"a render target drawn this vblank is presented instead of its copy", [] {
             require(decide_copy_only_present(true) == CopyOnlyPresent::PresentSource, "present the source");
         }},
        {"history remembers noted render targets only", [] {
             vcsnx::RenderTargetHistory history;
             history.note(0x088000u);
             require(history.contains(0x088000u), "noted address");
             require(!history.contains(0x044000u), "never noted");
         }},
        {"history keeps the newest addresses and ignores repeats", [] {
             vcsnx::RenderTargetHistory history;
             history.note(0x1000u);
             for (int i = 0; i < 100; ++i) history.note(0x1000u);  // repeats do not evict anything
             for (std::uint32_t i = 1; i < vcsnx::RenderTargetHistory::kCapacity; ++i) history.note(0x1000u + i * 16u);
             require(history.contains(0x1000u), "full but nothing evicted yet");
             history.note(0x9000u);
             require(!history.contains(0x1000u), "the oldest address is evicted when full");
             require(history.contains(0x9000u), "the newest address is kept");
         }},
    });
}
