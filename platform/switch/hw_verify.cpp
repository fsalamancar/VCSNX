#include "hw_verify.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace vcsnx {
namespace {

struct Vec4 {
    float x, y, z, w;
};

float dot(const Vec4 &row, float x, float y, float z) noexcept {
    return row.x * x + row.y * y + row.z * z + row.w; // p.w is always 1 in both shaders
}

// ge_gpu_backend_vulkan.cpp build_hw_transform_push_constants(): model_to_clip rows (column-major
// storage) with the viewport folded into rows 0-2 for the frame_extent() target.
struct PushRows {
    Vec4 row0, row1, row2, row3, view_z;
};

PushRows push_rows(const vcs::GeGpuHardwareTransform &t, float frame_width, float frame_height) noexcept {
    const auto row = [&](std::size_t r) {
        return Vec4{t.model_to_clip[r], t.model_to_clip[4u + r], t.model_to_clip[8u + r], t.model_to_clip[12u + r]};
    };
    const auto add_scaled = [](const Vec4 &a, float sa, const Vec4 &b, float sb) {
        return Vec4{a.x * sa + b.x * sb, a.y * sa + b.y * sb, a.z * sa + b.z * sb, a.w * sa + b.w * sb};
    };
    const Vec4 clip_x = row(0u), clip_y = row(1u), clip_z = row(2u), clip_w = row(3u);
    const float x_a = t.viewport_scale_x * (2.0f / frame_width);
    const float x_b = (t.viewport_center_x - t.viewport_offset_x) * (2.0f / frame_width) - 1.0f;
    const float y_a = t.viewport_scale_y * (2.0f / frame_height);
    const float y_b = (t.viewport_center_y - t.viewport_offset_y) * (2.0f / frame_height) - 1.0f;
    constexpr float inv_depth = 1.0f / 65535.0f;
    const float z_a = t.viewport_scale_z * inv_depth;
    const float z_b = t.viewport_center_z * inv_depth;
    return {add_scaled(clip_x, x_a, clip_w, x_b), add_scaled(clip_y, y_a, clip_w, y_b),
            add_scaled(clip_z, z_a, clip_w, z_b), clip_w,
            Vec4{t.model_to_view_z[0], t.model_to_view_z[1], t.model_to_view_z[2], t.model_to_view_z[3]}};
}

// The part both shaders share: position, depth and fog from a model-space point, then the
// perspective divide and NDC -> guest pixels that the rasterizer does after the shader.
HwVertexOut place(float x, float y, float z, const vcs::GeGpuHardwareTransform &t, float frame_width,
                  float frame_height) noexcept {
    const PushRows rows = push_rows(t, frame_width, frame_height);
    HwVertexOut out{};
    float clip_w = dot(rows.row3, x, y, z);
    if (std::fabs(clip_w) < 1.0e-12f) clip_w = 1.0f;
    float clip_z = dot(rows.row2, x, y, z);
    if (!t.depth_clip_enabled) clip_z = std::clamp(clip_z, 0.0f, clip_w);
    out.w = clip_w;
    out.x = (dot(rows.row0, x, y, z) / clip_w + 1.0f) * 0.5f * frame_width;
    out.y = (dot(rows.row1, x, y, z) / clip_w + 1.0f) * 0.5f * frame_height;
    out.z = clip_z / clip_w * 65535.0f;
    out.fog = std::clamp((dot(rows.view_z, x, y, z) + t.fog_end) * t.fog_slope, 0.0f, 1.0f);
    out.valid = std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z);
    return out;
}

std::uint8_t to_byte(float unit) noexcept {
    return static_cast<std::uint8_t>(std::clamp(unit, 0.0f, 1.0f) * 255.0f + 0.5f);
}

// Both shaders' optional affine vertex lighting: clamp(c * mul + add) then floor to RGBA8.
void apply_affine_lighting(std::array<float, 4> &color, const vcs::GeGpuHardwareTransform &t) noexcept {
    if (!t.vertex_color_affine) return;
    for (std::size_t i = 0; i < 4u; ++i) {
        const float lit = std::clamp(color[i] * t.vertex_color_mul[i] + t.vertex_color_add[i], 0.0f, 1.0f);
        color[i] = std::floor(lit * 255.0f) / 255.0f;
    }
}

float expand5(std::uint32_t value) noexcept {
    value &= 31u;
    return static_cast<float>((value << 3u) | (value >> 2u)) / 255.0f;
}

} // namespace

HwVertexOut emulate_hw_vertex(const vcs::GeGpuVertex &vertex, const vcs::GeGpuHardwareTransform &transform,
                              float frame_width, float frame_height) noexcept {
    HwVertexOut out = place(vertex.x, vertex.y, vertex.z, transform, frame_width, frame_height);
    out.u = vertex.u * transform.uv_scale_u + transform.uv_offset_u;
    out.v = vertex.v * transform.uv_scale_v + transform.uv_offset_v;
    out.q = vertex.q; // psp_ge_hw.vert passes in_q through
    // GeGpuVertex::rgba is R8G8B8A8 (R in the low byte), read by the shader as uvec4 / 255.
    std::array<float, 4> color{};
    for (std::size_t i = 0; i < 4u; ++i) color[i] = static_cast<float>((vertex.rgba >> (8u * i)) & 0xFFu) / 255.0f;
    apply_affine_lighting(color, transform);
    for (std::size_t i = 0; i < 4u; ++i) out.rgba[i] = to_byte(color[i]);
    return out;
}

HwVertexOut emulate_hw_packed_0115(const std::uint8_t *raw, const vcs::GeGpuHardwareTransform &transform,
                                   float frame_width, float frame_height) noexcept {
    std::uint16_t color5551 = 0;
    std::int16_t position[3]{};
    std::memcpy(&color5551, raw + 2, sizeof(color5551));
    std::memcpy(position, raw + 4, sizeof(position));
    constexpr float kS16 = 1.0f / 32768.0f;
    HwVertexOut out = place(static_cast<float>(position[0]) * kS16, static_cast<float>(position[1]) * kS16,
                            static_cast<float>(position[2]) * kS16, transform, frame_width, frame_height);
    out.u = static_cast<float>(raw[0]) / 128.0f * transform.uv_scale_u + transform.uv_offset_u;
    out.v = static_cast<float>(raw[1]) / 128.0f * transform.uv_scale_v + transform.uv_offset_v;
    std::array<float, 4> color{expand5(color5551), expand5(color5551 >> 5u), expand5(color5551 >> 10u),
                               (color5551 & 0x8000u) != 0u ? 1.0f : 0.0f};
    apply_affine_lighting(color, transform);
    for (std::size_t i = 0; i < 4u; ++i) out.rgba[i] = to_byte(color[i]);
    return out;
}

void HwVerifier::begin_draw() noexcept { ++counts_.draws; }

unsigned HwVerifier::add_vertex(const HwVertexOut &gpu, const HwReference &cpu) noexcept {
    if (cpu.clipped) {
        ++counts_.clipped;
        return 0u;
    }
    ++counts_.compared;
    unsigned bad = 0u;
    const float position_error = std::max(std::fabs(gpu.x - cpu.x), std::fabs(gpu.y - cpu.y));
    const float position_scale = std::max(std::fabs(cpu.x), std::fabs(cpu.y));
    if (!gpu.valid || !(position_error <= 0.5f + 1.0e-5f * position_scale)) bad |= kHwBadPosition;
    const float gpu_depth = std::clamp(gpu.z, 0.0f, 65535.0f);
    const float cpu_depth = std::clamp(cpu.z, 0.0f, 65535.0f);
    if (!(std::fabs(gpu_depth - cpu_depth) <= 2.0f + 1.0e-5f * cpu_depth)) bad |= kHwBadDepth;
    const auto projected = [](float coordinate, float q) { return std::fabs(q) > 1.0e-12f ? coordinate / q : coordinate; };
    if (!(std::fabs(projected(gpu.u, gpu.q) - projected(cpu.u, cpu.q)) <= 1.0e-4f &&
          std::fabs(projected(gpu.v, gpu.q) - projected(cpu.v, cpu.q)) <= 1.0e-4f))
        bad |= kHwBadUv;
    for (std::size_t i = 0; i < 4u; ++i) {
        const int difference = static_cast<int>(gpu.rgba[i]) - static_cast<int>(cpu.rgba[i]);
        if (difference > 1 || difference < -1) bad |= kHwBadColor;
    }
    if (!(std::fabs(gpu.fog - cpu.fog) <= 1.0f / 255.0f)) bad |= kHwBadFog;
    if (std::isfinite(position_error)) counts_.max_pos_err_px = std::max(counts_.max_pos_err_px, position_error);
    if ((bad & kHwBadPosition) != 0u) ++counts_.pos_bad;
    if ((bad & kHwBadDepth) != 0u) ++counts_.depth_bad;
    if ((bad & kHwBadUv) != 0u) ++counts_.uv_bad;
    if ((bad & kHwBadColor) != 0u) ++counts_.color_bad;
    if ((bad & kHwBadFog) != 0u) ++counts_.fog_bad;
    return bad;
}

bool HwVerifier::want_detail(std::uint32_t vertex_type, bool packed, unsigned bad) {
    if (details_total_ >= 64) return false;
    const std::uint64_t kind = (static_cast<std::uint64_t>(vertex_type) << 8u) |
                               (static_cast<std::uint64_t>(packed ? 1u : 0u) << 7u) | (bad & 0x7Fu);
    int &count = details_per_kind_[kind];
    if (count >= 4) return false;
    ++count;
    ++details_total_;
    return true;
}

std::optional<std::string> HwVerifier::take_report_if_due(long long now_ms, const HwBackendCounters &backend) {
    if (window_start_ms_ < 0) {
        window_start_ms_ = now_ms;
        backend_at_window_start_ = backend;
        return std::nullopt;
    }
    if (now_ms - window_start_ms_ < 10'000) return std::nullopt;
    const auto grew = [](std::uint64_t now, std::uint64_t start) {
        return static_cast<unsigned long long>(now >= start ? now - start : 0u);
    };
    char line[400];
    std::snprintf(line, sizeof(line),
                  "[hw-verify] draws=%llu compared=%llu clipped=%llu pos_bad=%llu depth_bad=%llu uv_bad=%llu "
                  "color_bad=%llu fog_bad=%llu max_pos_err_px=%.2f hw_rejected=%llu vertex_overflows=%llu "
                  "texture_missing=%llu",
                  static_cast<unsigned long long>(counts_.draws), static_cast<unsigned long long>(counts_.compared),
                  static_cast<unsigned long long>(counts_.clipped), static_cast<unsigned long long>(counts_.pos_bad),
                  static_cast<unsigned long long>(counts_.depth_bad), static_cast<unsigned long long>(counts_.uv_bad),
                  static_cast<unsigned long long>(counts_.color_bad), static_cast<unsigned long long>(counts_.fog_bad),
                  static_cast<double>(counts_.max_pos_err_px),
                  grew(backend.rejected_draws, backend_at_window_start_.rejected_draws),
                  grew(backend.vertex_overflows, backend_at_window_start_.vertex_overflows),
                  grew(backend.textured_without_texture, backend_at_window_start_.textured_without_texture));
    counts_ = {};
    backend_at_window_start_ = backend;
    window_start_ms_ = now_ms;
    return std::string(line);
}

std::pair<float, float> hw_verify_frame_extent() noexcept {
    static const std::pair<float, float> extent = [] {
        std::pair<float, float> value{512.0f, 320.0f};
        if (const char *text = std::getenv("PSPRECOMP_VULKAN_FRAME"); text != nullptr && *text != '\0') {
            unsigned w = 0u;
            unsigned h = 0u;
            if (std::sscanf(text, "%ux%u", &w, &h) == 2 && w >= 240u && w <= 2048u && h >= 136u && h <= 2048u)
                value = {static_cast<float>(w), static_cast<float>(h)};
        }
        return value;
    }();
    return extent;
}

bool PeriodicDump::due(std::uint64_t vblank) noexcept {
    if (every_ == 0u || vblank < next_) return false;
    next_ = (vblank / every_ + 1u) * every_;
    return true;
}

std::uint64_t gpu_dump_every_vblanks() noexcept {
    static const std::uint64_t every = [] {
        const char *text = std::getenv("VCSNX_GPU_DUMP_EVERY");
        if (text == nullptr || *text == '\0') return std::uint64_t{0};
        const long long value = std::atoll(text);
        return value > 0 ? static_cast<std::uint64_t>(value) : std::uint64_t{0};
    }();
    return every;
}

VertexTypeFilter::VertexTypeFilter(const char *list) {
    if (list == nullptr) return;
    const char *cursor = list;
    while (*cursor != '\0') {
        while (*cursor == ',' || *cursor == ' ') ++cursor;
        if (*cursor == '\0') break;
        char *end = nullptr;
        const unsigned long value = std::strtoul(cursor, &end, 16);
        if (end != cursor && (*end == ',' || *end == ' ' || *end == '\0')) {
            excluded_.push_back(static_cast<std::uint32_t>(value));
            cursor = end;
        } else {
            while (*cursor != '\0' && *cursor != ',') ++cursor; // skip a bad entry
        }
    }
}

bool VertexTypeFilter::allows(std::uint32_t vertex_type) const noexcept {
    return std::find(excluded_.begin(), excluded_.end(), vertex_type) == excluded_.end();
}

bool hw_transform_allows_vertex_type(std::uint32_t vertex_type) noexcept {
    static const VertexTypeFilter filter(std::getenv("VCSNX_HW_EXCLUDE_VTYPES"));
    return filter.allows(vertex_type);
}

int hw_verify_level() noexcept {
    static const int level = [] {
        const char *text = std::getenv("VCSNX_HW_VERIFY");
        if (text == nullptr || *text == '\0') return 0;
        return std::clamp(std::atoi(text), 0, 2);
    }();
    return level;
}

} // namespace vcsnx
