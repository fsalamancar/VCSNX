// The hardware-transform path of vcs::render_ge_primitive decodes big draws on the raster worker
// pool (PSPRECOMP_GE_PARALLEL_VERTEX_DECODE, on with Vulkan). Every decoded vertex must reach the
// backend once and in order, whichever thread decoded it.
#include "fake_gpu_backend.hpp"
#include "ge_renderer.hpp"
#include "test_support.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace {

std::uint32_t ge_command(std::uint32_t command, std::uint32_t data) {
    return (command << 24u) | (data & 0x00FFFFFFu);
}

std::uint32_t ge_float24(float value) {
    return (std::bit_cast<std::uint32_t>(value) >> 8u) & 0x00FFFFFFu;
}

// Colour 8888 + float XYZ: 16 bytes per vertex.
void put_3d_vertex(psprecomp::GuestMemory &memory, std::uint32_t address, std::uint32_t color,
                   float x, float y, float z) {
    memory.store32(address + 0u, color);
    memory.store32(address + 4u, std::bit_cast<std::uint32_t>(x));
    memory.store32(address + 8u, std::bit_cast<std::uint32_t>(y));
    memory.store32(address + 12u, std::bit_cast<std::uint32_t>(z));
}

} // namespace

int main() {
    // Read once by the renderer's static caches, so set before the first draw.
    setenv("PSPRECOMP_GE_GPU_HW_TRANSFORM", "1", 1);
    setenv("PSPRECOMP_GE_GPU_SKIP_SOFTWARE_RASTER", "1", 1);
    setenv("PSPRECOMP_GE_PARALLEL_VERTEX_DECODE", "1", 1);
    setenv("PSPRECOMP_RASTER_THREADS", "4", 1);
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"a 600-vertex draw decoded on four threads reaches the backend whole and in order", [] {
             constexpr std::uint32_t kVertices = 0x08040000u;
             constexpr std::uint32_t kCount = 600u; // above the 256-vertex parallel threshold
             psprecomp::GuestMemory memory;
             for (std::uint32_t i = 0u; i < kCount; ++i)
                 put_3d_vertex(memory, kVertices + i * 16u, 0xFF000000u | i, static_cast<float>(i),
                               static_cast<float>(i % 7u), -1.0f);
             std::array<std::uint32_t, 256> commands{};
             commands[0x12] = ge_command(0x12u, (7u << 2u) | (3u << 7u)); // colour 8888, float XYZ
             commands[0x42] = ge_command(0x42u, ge_float24(240.0f));
             commands[0x43] = ge_command(0x43u, ge_float24(-136.0f));
             commands[0x44] = ge_command(0x44u, ge_float24(32767.5f));
             commands[0x45] = ge_command(0x45u, ge_float24(2048.0f));
             commands[0x46] = ge_command(0x46u, ge_float24(2048.0f));
             commands[0x47] = ge_command(0x47u, ge_float24(32767.5f));
             commands[0x4C] = ge_command(0x4Cu, (2048u - 240u) << 4u);
             commands[0x4D] = ge_command(0x4Du, (2048u - 136u) << 4u);
             commands[0x9C] = ge_command(0x9Cu, 0u);
             commands[0x9D] = ge_command(0x9Du, 512u);
             commands[0xD2] = ge_command(0xD2u, 3u);
             commands[0xD4] = ge_command(0xD4u, 0u);
             commands[0xD5] = ge_command(0xD5u, 479u | (271u << 10u));
             vcs::GeTransformState transform{};
             vcs::reset_ge_transform_state(transform);
             vcs::GeRenderStats stats{};
             std::string error;
             vcsnx_test::fake_hw_vertices().clear();
             require(vcs::render_ge_primitive(memory, commands, transform, kVertices, 0u, (3u << 16u) | kCount,
                                              stats, error),
                     error.empty() ? "render_ge_primitive failed" : error);
             const auto &got = vcsnx_test::fake_hw_vertices();
             require(got.size() == kCount, "backend got " + std::to_string(got.size()) + " of 600 vertices");
             for (std::uint32_t i = 0u; i < kCount; ++i)
                 require(got[i].x == static_cast<float>(i) && got[i].y == static_cast<float>(i % 7u),
                         "vertex " + std::to_string(i) + " lost or out of order");
         }},
    });
}
