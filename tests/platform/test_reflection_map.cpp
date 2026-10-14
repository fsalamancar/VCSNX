// The car reflection map is drawn from the GPU's last frame: the right pixels, inside the target only.
#include "reflection_map.hpp"
#include "test_support.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace {

// A host frame of w x h RGBA8 pixels, every pixel the colour `pick(x, y)` returns.
template <typename Pick>
std::vector<std::uint8_t> frame(std::uint32_t w, std::uint32_t h, Pick pick) {
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(w) * h * 4u);
    for (std::uint32_t y = 0; y < h; ++y)
        for (std::uint32_t x = 0; x < w; ++x) {
            const std::array<std::uint8_t, 3> c = pick(x, y);
            const std::size_t i = (static_cast<std::size_t>(y) * w + x) * 4u;
            rgba[i] = c[0];
            rgba[i + 1] = c[1];
            rgba[i + 2] = c[2];
            rgba[i + 3] = 255u;
        }
    return rgba;
}

// The pass the Mac [uvgen] log showed on every car: env-mapped, the 64-wide map, fixed 0x15 / 0xEA.
vcsnx::CarPassDraw observed_car_pass() {
    vcsnx::CarPassDraw draw;
    draw.uv_generation = 2u;
    draw.texture_address = 0x04044000u;
    draw.texture_buffer_width = 64u;
    draw.blend_mode = 0x0000AAu;
    draw.fix_source = 0x151515u;
    draw.fix_dest = 0xEAEAEAu;
    return draw;
}

std::uint16_t rgb565(unsigned r, unsigned g, unsigned b) {
    return static_cast<std::uint16_t>((r >> 3u) | ((g >> 2u) << 5u) | ((b >> 3u) << 11u));
}

std::uint16_t pixel(const std::vector<std::uint8_t> &target, std::uint32_t stride, std::uint32_t x, std::uint32_t y) {
    const std::size_t i = (static_cast<std::size_t>(y) * stride + x) * 2u;
    return static_cast<std::uint16_t>(target[i] | (target[i + 1] << 8u));
}

// Left half red, right half blue.
std::array<std::uint8_t, 3> halves(std::uint32_t x, std::uint32_t width) {
    return x < width / 2u ? std::array<std::uint8_t, 3>{248, 0, 0} : std::array<std::uint8_t, 3>{0, 0, 248};
}

// From the Mac [uvgen-target] log: through sprites from the world target (512 x 320 logical frame) into the
// 64-wide target at the start of the back buffer, 565, scissor (0,0)-(64,64).
vcsnx::ReflectionTarget target_565(std::vector<std::uint8_t> &memory, std::uint32_t stride, std::uint32_t rows) {
    memory.assign(static_cast<std::size_t>(stride) * rows * 2u, 0xAAu);
    return {memory.data(), stride, rows, 0u, 64, 64};
}

vcsnx::ReflectionMapDraw observed_build() {
    vcsnx::ReflectionMapDraw d;
    d.through = true;
    d.primitive = 6u;
    d.textured = true;
    d.target_stride = 64u;
    d.texture_address = 0x04088000u;
    d.gpu_frame_target = 0x04088000u;
    return d;
}

} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"each sprite gets the part of the frame it maps", [] {
             const auto host = frame(8, 4, [](std::uint32_t x, std::uint32_t) { return halves(x, 8); });
             const vcsnx::HostFrameView view{host.data(), 8, 4, 8.0f, 4.0f};
             std::vector<std::uint8_t> memory;
             const auto target = target_565(memory, 4, 2);
             // The whole frame into 2 x 1 pixels: the left one covers the red half, the right one the blue.
             const std::array<vcsnx::ReflectionSprite, 1> sprites{{{0, 0, 0, 0, 8, 4, 2, 1}}};
             require(vcsnx::draw_reflection_sprites(sprites, view, target), "565 is drawn");
             require(pixel(memory, 4, 0, 0) == rgb565(248, 0, 0), "left pixel red");
             require(pixel(memory, 4, 1, 0) == rgb565(0, 0, 248), "right pixel blue");
             require(pixel(memory, 4, 2, 0) == 0xAAAAu, "outside the sprite untouched");
             require(pixel(memory, 4, 0, 1) == 0xAAAAu, "row below the sprite untouched");
         }},
        {"the host frame's resolution does not matter", [] {
             const auto host = frame(32, 16, [](std::uint32_t x, std::uint32_t) { return halves(x, 32); });
             const vcsnx::HostFrameView view{host.data(), 32, 16, 8.0f, 4.0f};
             std::vector<std::uint8_t> memory;
             const auto target = target_565(memory, 4, 2);
             const std::array<vcsnx::ReflectionSprite, 1> sprites{{{0, 0, 0, 0, 8, 4, 2, 1}}};
             require(vcsnx::draw_reflection_sprites(sprites, view, target), "drawn");
             require(pixel(memory, 4, 0, 0) == rgb565(248, 0, 0), "left pixel red");
             require(pixel(memory, 4, 1, 0) == rgb565(0, 0, 248), "right pixel blue");
         }},
        {"a pixel averages the frame it covers", [] {
             // Columns alternate white and black: one target pixel over 4 columns is mid grey.
             const auto host = frame(8, 4, [](std::uint32_t x, std::uint32_t) {
                 return x % 2u == 0u ? std::array<std::uint8_t, 3>{255, 255, 255} : std::array<std::uint8_t, 3>{0, 0, 0};
             });
             const vcsnx::HostFrameView view{host.data(), 8, 4, 8.0f, 4.0f};
             std::vector<std::uint8_t> memory;
             const auto target = target_565(memory, 4, 2);
             const std::array<vcsnx::ReflectionSprite, 1> sprites{{{0, 0, 0, 0, 8, 4, 2, 1}}};
             require(vcsnx::draw_reflection_sprites(sprites, view, target), "drawn");
             const std::uint16_t value = pixel(memory, 4, 0, 0);
             const unsigned green = (value >> 5u) & 0x3Fu;
             require(green >= 28u && green <= 35u, "grey, not one column's colour");
         }},
        {"pixels outside the target's stride, rows and scissor are never written", [] {
             const auto host = frame(8, 8, [](std::uint32_t, std::uint32_t) { return std::array<std::uint8_t, 3>{255, 255, 255}; });
             const vcsnx::HostFrameView view{host.data(), 8, 8, 8.0f, 8.0f};
             std::vector<std::uint8_t> memory;
             auto target = target_565(memory, 4, 3);
             target.clip_x1 = 2;  // inclusive
             // A sprite far larger than the target, and one with its corners given the other way round.
             const std::array<vcsnx::ReflectionSprite, 2> sprites{{{0, 0, -4, -4, 8, 8, 100, 100},
                                                                   {8, 8, 3, 3, 0, 0, 0, 0}}};
             require(vcsnx::draw_reflection_sprites(sprites, view, target), "drawn");
             for (std::uint32_t y = 0; y < 3u; ++y)
                 for (std::uint32_t x = 0; x < 4u; ++x) {
                     const bool inside = x <= 2u;
                     require((pixel(memory, 4, x, y) == 0xFFFFu) == inside, "only inside the scissor");
                 }
         }},
        {"without a host frame the map is black, not what VRAM held", [] {
             std::vector<std::uint8_t> memory;
             const auto target = target_565(memory, 4, 2);
             const std::array<vcsnx::ReflectionSprite, 1> sprites{{{0, 0, 0, 0, 8, 4, 2, 1}}};
             require(vcsnx::draw_reflection_sprites(sprites, vcsnx::HostFrameView{}, target), "drawn");
             require(pixel(memory, 4, 0, 0) == 0u && pixel(memory, 4, 1, 0) == 0u, "black");
         }},
        {"formats other than 565 are left to the renderer", [] {
             const auto host = frame(8, 4, [](std::uint32_t x, std::uint32_t) { return halves(x, 8); });
             const vcsnx::HostFrameView view{host.data(), 8, 4, 8.0f, 4.0f};
             std::vector<std::uint8_t> memory;
             auto target = target_565(memory, 4, 2);
             target.format = 3u;
             const std::array<vcsnx::ReflectionSprite, 1> sprites{{{0, 0, 0, 0, 8, 4, 2, 1}}};
             require(!vcsnx::draw_reflection_sprites(sprites, view, target), "8888 not drawn here");
             require(pixel(memory, 4, 0, 0) == 0xAAAAu, "untouched");
         }},
        {"the build of the reflection map is recognised", [] {
             require(vcsnx::is_reflection_map_build(observed_build()), "observed draw");
         }},
        {"other draws are not", [] {
             auto display = observed_build();
             display.target_stride = 512u;
             require(!vcsnx::is_reflection_map_build(display), "the display composition (stride 512)");
             auto ram_texture = observed_build();
             ram_texture.texture_address = 0x0971DA50u;
             require(!vcsnx::is_reflection_map_build(ram_texture), "the overlays from a texture in RAM");
             auto other_target = observed_build();
             other_target.gpu_frame_target = 0x04110000u;
             require(!vcsnx::is_reflection_map_build(other_target), "a VRAM texture the GPU did not present");
             auto triangles = observed_build();
             triangles.primitive = 3u;
             require(!vcsnx::is_reflection_map_build(triangles), "triangles");
             auto world = observed_build();
             world.through = false;
             require(!vcsnx::is_reflection_map_build(world), "3D");
             auto untextured = observed_build();
             untextured.textured = false;
             require(!vcsnx::is_reflection_map_build(untextured), "no texture");
         }},
        {"small targets are rasterized on the CPU", [] {
             require(vcsnx::cpu_rasterizes_target(64u), "the 64-wide reflection map");
             require(!vcsnx::cpu_rasterizes_target(512u), "the world and the display");
             require(!vcsnx::cpu_rasterizes_target(0u), "no stride");
         }},
        {"the cars' reflection pass is recognised", [] {
             require(vcsnx::is_car_reflection_pass(observed_car_pass()), "observed pass on 0x04044000");
             auto other_buffer = observed_car_pass();
             other_buffer.texture_address = 0x04000000u;
             require(vcsnx::is_car_reflection_pass(other_buffer), "the other back buffer");
         }},
        {"other blended draws are left alone", [] {
             auto not_mapped = observed_car_pass();
             not_mapped.uv_generation = 0u;
             require(!vcsnx::is_car_reflection_pass(not_mapped), "UVs from the vertices");
             auto lighter = observed_car_pass();
             lighter.fix_source = 0x070707u;
             lighter.fix_dest = 0xF8F8F8u;
             require(!vcsnx::is_car_reflection_pass(lighter), "another fixed blend");
             auto ram = observed_car_pass();
             ram.texture_address = 0x096F1230u;
             require(!vcsnx::is_car_reflection_pass(ram), "a texture in RAM");
             auto wide = observed_car_pass();
             wide.texture_buffer_width = 512u;
             require(!vcsnx::is_car_reflection_pass(wide), "a full-size target");
             auto alpha = observed_car_pass();
             alpha.blend_mode = 0x000032u;
             require(!vcsnx::is_car_reflection_pass(alpha), "alpha blending");
         }},
        {"the strength sets complementary fixed factors", [] {
             const vcsnx::ReflectionBlend twenty = vcsnx::car_reflection_blend(0.20f);
             require(twenty.source == 0x333333u && twenty.dest == 0xCCCCCCu, "20%: 0x33 / 0xCC");
             const vcsnx::ReflectionBlend original = vcsnx::car_reflection_blend(0.08f);
             require(original.source == 0x141414u && original.dest == 0xEBEBEBu, "8%: 0x14 / 0xEB");
             const vcsnx::ReflectionBlend over = vcsnx::car_reflection_blend(3.0f);
             require(over.source == 0xFFFFFFu && over.dest == 0u, "clamped to 1");
             const vcsnx::ReflectionBlend under = vcsnx::car_reflection_blend(-1.0f);
             require(under.source == 0u && under.dest == 0xFFFFFFu, "clamped to 0");
         }},
        {"the strength comes from VCSNX_CAR_REFLECTION, 0.20 otherwise", [] {
             require(vcsnx::parse_car_reflection(nullptr) == 0.20f, "unset");
             require(vcsnx::parse_car_reflection("0.08") == 0.08f, "the original");
             require(vcsnx::parse_car_reflection("abc") == 0.20f, "not a number");
             require(vcsnx::parse_car_reflection("1.5") == 1.0f, "above 1");
         }},
    });
}
