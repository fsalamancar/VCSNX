#pragma once
// Per-draw check of the GE hardware-transform path (Plan 3). For each vertex the GPU would draw,
// recomputes on the CPU what psp_ge_hw.vert / psp_ge_hw_packed0115.vert produce from the push
// constants ge_gpu_backend_vulkan.cpp builds, and compares it with the software path, which the
// user validated on the console. Enabled with VCSNX_HW_VERIFY=1 (summaries) or 2 (plus details).
#include "ge_gpu_backend.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace vcsnx {

// One vertex as the GPU would place it: guest screen pixels (the backend's frame_extent() space,
// like the software viewport transform), PSP depth units (0..65535), texture coordinates after
// scale/offset, fog factor and RGBA8 colour.
struct HwVertexOut {
    bool valid{};
    float x{}, y{}, z{}, w{1.0f};
    float u{}, v{};
    float q{1.0f}; // projective texture denominator; the fragment stage samples at (u/q, v/q)
    float fog{1.0f};
    std::array<std::uint8_t, 4> rgba{};
};

// The software path's result for the same vertex (decode + viewport transform). `clipped`: the
// vertex is behind the camera (w <= 0), where the two paths differ by design.
struct HwReference {
    float x{}, y{}, z{};
    float u{}, v{};
    float q{1.0f};
    float fog{1.0f};
    std::array<std::uint8_t, 4> rgba{};
    bool clipped{};
};

[[nodiscard]] HwVertexOut emulate_hw_vertex(const vcs::GeGpuVertex &vertex,
                                            const vcs::GeGpuHardwareTransform &transform,
                                            float frame_width, float frame_height) noexcept;
// `raw`: one 10-byte PSP 0x0115 record (u8 u, u8 v, u16 colour 5551, s16 x, y, z).
[[nodiscard]] HwVertexOut emulate_hw_packed_0115(const std::uint8_t *raw,
                                                 const vcs::GeGpuHardwareTransform &transform,
                                                 float frame_width, float frame_height) noexcept;

inline constexpr unsigned kHwBadPosition = 1u;
inline constexpr unsigned kHwBadDepth = 2u;
inline constexpr unsigned kHwBadUv = 4u;
inline constexpr unsigned kHwBadColor = 8u;
inline constexpr unsigned kHwBadFog = 16u;

struct HwVerifyCounts {
    std::uint64_t draws{}, compared{}, clipped{};
    std::uint64_t pos_bad{}, depth_bad{}, uv_bad{}, color_bad{}, fog_bad{};
    float max_pos_err_px{};
};

// The GPU backend's running totals of draws it did not draw (GeGpuBackendReport: rejected_gpu_draws,
// game_vertex_overflows, game_textured_draws_without_texture). The summary reports their growth.
struct HwBackendCounters {
    std::uint64_t rejected_draws{}, vertex_overflows{}, textured_without_texture{};
};

// Tolerances: position 0.5 px plus 1e-5 of the coordinate, depth 2 units plus 1e-5, projected UV
// (u/q, v/q) 1e-4, fog 1/255, colour 1 LSB per channel (the
// shaders floor lit colour to RGBA8 while the CPU path rounds).
class HwVerifier {
public:
    void begin_draw() noexcept;
    // Returns the kHwBad* bits this vertex failed (0 when it matches or is clipped).
    unsigned add_vertex(const HwVertexOut &gpu, const HwReference &cpu) noexcept;
    [[nodiscard]] const HwVerifyCounts &counts() const noexcept { return counts_; }
    // Whether to print a [hw-verify-detail] line for this mismatch: at most 4 per (vertex type,
    // packed, mismatch bits) and 64 per session, so rare formats are not drowned by the common one.
    [[nodiscard]] bool want_detail(std::uint32_t vertex_type, bool packed, unsigned bad);
    // The first call starts a 10 s window; once it has elapsed, returns the summary line
    // ("[hw-verify] draws=N compared=... max_pos_err_px=... hw_rejected=... vertex_overflows=...
    // texture_missing=...", the last three being how much `backend` grew in the window) and restarts.
    [[nodiscard]] std::optional<std::string> take_report_if_due(long long now_ms, const HwBackendCounters &backend = {});

private:
    HwVerifyCounts counts_{};
    HwBackendCounters backend_at_window_start_{};
    long long window_start_ms_{-1};
    std::map<std::uint64_t, int> details_per_kind_;
    int details_total_{};
};

// The guest frame the Vulkan backend maps to its target (ge_gpu_backend_vulkan.cpp frame_extent():
// 512x320, or PSPRECOMP_VULKAN_FRAME=WxH), as {width, height}.
[[nodiscard]] std::pair<float, float> hw_verify_frame_extent() noexcept;

// Periodic GPU frame dumps for side-by-side checks (VCSNX_GPU_DUMP_EVERY=N vblanks; the vendor's
// dump names each file by vblank). due() is true once per period; late vblanks do not pile up.
class PeriodicDump {
public:
    explicit PeriodicDump(std::uint64_t every_vblanks) noexcept : every_(every_vblanks), next_(every_vblanks) {}
    [[nodiscard]] bool due(std::uint64_t vblank) noexcept;

private:
    std::uint64_t every_;
    std::uint64_t next_;
};
[[nodiscard]] std::uint64_t gpu_dump_every_vblanks() noexcept;

// Bisection aid: vertex types (GE VTYPE, e.g. 0x00119f) listed in VCSNX_HW_EXCLUDE_VTYPES stay on
// the screen-space path when the hardware transform is on. Unparsable entries are ignored.
class VertexTypeFilter {
public:
    explicit VertexTypeFilter(const char *list);
    [[nodiscard]] bool allows(std::uint32_t vertex_type) const noexcept;

private:
    std::vector<std::uint32_t> excluded_;
};
[[nodiscard]] bool hw_transform_allows_vertex_type(std::uint32_t vertex_type) noexcept;

// VCSNX_HW_VERIFY: 0 off (default), 1 summaries, 2 summaries plus the first mismatching vertices.
[[nodiscard]] int hw_verify_level() noexcept;

} // namespace vcsnx
