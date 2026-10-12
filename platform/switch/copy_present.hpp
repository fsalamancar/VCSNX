#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace vcsnx {

// VRAM addresses that were GPU render targets (frame buckets) at some point. The Vulkan backend uses
// it only to report a copy whose source the GPU never drew (the screen keeps its previous picture).
class RenderTargetHistory {
public:
    static constexpr std::size_t kCapacity = 32;
    void note(std::uint32_t address) noexcept;
    [[nodiscard]] bool contains(std::uint32_t address) const noexcept;

private:
    std::array<std::uint32_t, kCapacity> addresses_{};
    std::size_t count_ = 0;
    std::size_t next_ = 0;
};

enum class CopyOnlyPresent { PresentSource, Hold };

// What to present when the winning bucket only copies one VRAM texture: its source when the GPU drew
// it this vblank, otherwise the previous picture. Releasing an undrawn copy after a few vblanks (a
// Plan 3 review suggestion) showed Switch loading screens black with a red stroke (run 12).
[[nodiscard]] CopyOnlyPresent decide_copy_only_present(bool source_drawn_this_vblank) noexcept;

// How long a copy of a never-drawn source is held before the backend logs it (2 s at 60 Hz).
inline constexpr std::uint32_t kUnknownSourceReportVblanks = 120u;

} // namespace vcsnx
