// Test double for the GE GPU backend: reports an active, graphics-ready Vulkan backend and keeps
// what the hardware-transform path hands it, so tests can drive vcs::render_ge_primitive()'s GPU
// branches without a device. Built from platform/switch/ge_gpu_backend_null.cpp.

#include "fake_gpu_backend.hpp"
#include "ge_gpu_backend.hpp"

namespace vcsnx_test {
std::vector<vcs::GeGpuVertex> &fake_hw_vertices() {
    static std::vector<vcs::GeGpuVertex> vertices;
    return vertices;
}
} // namespace vcsnx_test

namespace vcs {

// Success means "software rendering is ready": main.cpp throws when this returns false, and the
// real backends also return true (requested=active=Software) when the ini asks for Software.
bool initialize_ge_gpu_backend(std::string &error) {
    error.clear();
    return true;
}
void shutdown_ge_gpu_backend() noexcept {}
void ge_gpu_backend_lock() noexcept {}
void ge_gpu_backend_unlock() noexcept {}
bool ge_gpu_backend_active() noexcept { return true; }
bool ge_gpu_backend_transfer_ready() noexcept { return true; }
bool ge_gpu_backend_graphics_ready() noexcept { return true; }
void ge_gpu_backend_record_draw(const GeGpuDrawDescriptor &) noexcept {}
void ge_gpu_backend_observe_camera(const std::array<float, 12> &, const std::array<float, 16> &,
                                   const std::array<float, 6> &, const std::array<float, 3> &,
                                   const GeGpuDrawDescriptor &, std::uint32_t) noexcept {}
bool ge_gpu_backend_stage_vertices(const GeGpuDrawDescriptor &, std::span<const GeGpuVertex>) noexcept {
    return false;
}
bool ge_gpu_backend_texture_needed(const GeGpuDrawDescriptor &) noexcept { return false; }
void ge_gpu_backend_prepare_texture_keys(GeGpuDrawDescriptor &) noexcept {}
bool ge_gpu_backend_texture_signature_needed(const GeGpuDrawDescriptor &) noexcept { return false; }
bool ge_gpu_backend_is_framebuffer_feedback_texture(const GeGpuDrawDescriptor &) noexcept { return false; }
GeGpuWidescreenHud ge_gpu_backend_widescreen_hud(const GeGpuDrawDescriptor &) noexcept { return {}; }
void ge_gpu_backend_note_through_extent(const GeGpuDrawDescriptor &, float, float) noexcept {}
bool ge_gpu_backend_adopt_shared_texture(const GeGpuDrawDescriptor &) noexcept { return false; }
bool ge_gpu_backend_texture_available(const GeGpuDrawDescriptor &) noexcept { return false; }
bool ge_gpu_backend_upload_decoded_texture(const GeGpuDrawDescriptor &, std::uint32_t, std::uint32_t,
                                           std::span<const std::byte>) noexcept {
    return false;
}
bool ge_gpu_backend_upload_decoded_texture_chain(const GeGpuDrawDescriptor &,
                                                 std::span<const GeGpuDecodedMipLevel>) noexcept {
    return false;
}
bool ge_gpu_backend_upload_decoded_texture_chain_packed(const GeGpuDrawDescriptor &, std::uint32_t,
                                                        std::uint32_t, std::uint32_t,
                                                        std::vector<std::byte>) noexcept {
    return false;
}
bool ge_gpu_backend_copy_last_texture_rgba(std::span<std::byte>) noexcept { return false; }
void ge_gpu_backend_accumulate_color_triangles(const GeGpuDrawDescriptor &,
                                               std::span<const GeGpuVertex>) noexcept {}
void ge_gpu_backend_accumulate_hardware_triangles(const GeGpuDrawDescriptor &, const GeGpuHardwareTransform &,
                                                  std::span<const GeGpuVertex> vertices,
                                                  std::span<const std::uint32_t>) noexcept {
    auto &kept = vcsnx_test::fake_hw_vertices();
    kept.insert(kept.end(), vertices.begin(), vertices.end());
}
bool ge_gpu_backend_accumulate_hardware_packed_0115(const GeGpuDrawDescriptor &, const GeGpuHardwareTransform &,
                                                    std::span<const std::byte>, std::uint32_t,
                                                    std::span<const std::uint32_t>) noexcept {
    return false;
}
void ge_gpu_backend_set_native_window(void *) noexcept {}
bool ge_gpu_backend_is_vulkan() noexcept { return true; }
void ge_gpu_backend_set_display_framebuffer(std::uint32_t) noexcept {}
bool ge_gpu_backend_finish_color_frame(std::uint64_t) noexcept { return false; }
bool ge_gpu_backend_copy_game_frame_rgba(std::span<std::byte>) noexcept { return false; }
bool ge_gpu_backend_presents_directly() noexcept { return false; }
bool ge_gpu_backend_owns_window() noexcept { return false; }
bool ge_gpu_backend_present_rgba(std::span<const std::byte>, std::uint32_t, std::uint32_t, bool) noexcept {
    return false;
}
std::uint32_t ge_gpu_backend_owned_framebuffer() noexcept { return 0u; }
std::uint32_t ge_gpu_backend_display_framebuffer() noexcept { return 0u; }
std::uint32_t ge_gpu_backend_last_winner_target() noexcept { return 0u; }
std::span<const std::byte> ge_gpu_backend_game_frame_rgba() noexcept { return {}; }
bool ge_gpu_backend_copy_offscreen_rgba(std::span<std::byte>) noexcept { return false; }
void ge_gpu_backend_mark_window_presented() noexcept {}
GeGpuBackendReport ge_gpu_backend_report() {
    GeGpuBackendReport report{};
    report.requested = GeGpuBackendKind::Vulkan;
    report.active = GeGpuBackendKind::Vulkan;
    report.message = "fake Vulkan backend for tests";
    return report;
}
const char *ge_gpu_backend_name(GeGpuBackendKind kind) noexcept {
    switch (kind) {
    case GeGpuBackendKind::Software: return "Software";
    case GeGpuBackendKind::DirectX12: return "DirectX12";
    case GeGpuBackendKind::Vulkan: return "Vulkan";
    }
    return "Unknown";
}

} // namespace vcs
