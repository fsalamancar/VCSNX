#include "hw_verify.hpp"
#include "test_support.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace {

constexpr float kFrameW = 512.0f; // ge_gpu_backend_vulkan.cpp frame_extent()
constexpr float kFrameH = 320.0f;

// Identity model->clip with the usual PSP viewport: centre 2048, half extents 240/-136, offsets
// 2048-240 and 2048-136, depth scale/centre 32767.5.
vcs::GeGpuHardwareTransform psp_identity() {
    vcs::GeGpuHardwareTransform t{};
    t.model_to_clip = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                       0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    t.viewport_scale_x = 240.0f;
    t.viewport_scale_y = -136.0f;
    t.viewport_scale_z = 32767.5f;
    t.viewport_center_x = 2048.0f;
    t.viewport_center_y = 2048.0f;
    t.viewport_center_z = 32767.5f;
    t.viewport_offset_x = 2048.0f - 240.0f;
    t.viewport_offset_y = 2048.0f - 136.0f;
    return t;
}

vcs::GeGpuVertex model(float x, float y, float z) {
    vcs::GeGpuVertex v{};
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = 1.0f;
    return v;
}

bool near(float a, float b, float tolerance = 1.0e-3f) { return std::fabs(a - b) <= tolerance; }

vcsnx::HwReference reference_from(const vcsnx::HwVertexOut &out) {
    vcsnx::HwReference ref{};
    ref.x = out.x;
    ref.y = out.y;
    ref.z = out.z;
    ref.u = out.u;
    ref.v = out.v;
    ref.fog = out.fog;
    ref.rgba = out.rgba;
    ref.q = out.q;
    return ref;
}

} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"origin lands on the PSP screen centre at mid depth", [] {
             const auto out = vcsnx::emulate_hw_vertex(model(0.0f, 0.0f, 0.0f), psp_identity(), kFrameW, kFrameH);
             require(out.valid, "valid");
             require(near(out.x, 240.0f) && near(out.y, 136.0f), "x/y " + std::to_string(out.x) + "," + std::to_string(out.y));
             require(near(out.z, 32767.5f, 0.05f), "depth " + std::to_string(out.z));
         }},
        {"model x=1, y=0.5 reaches the right edge and a quarter down", [] {
             const auto out = vcsnx::emulate_hw_vertex(model(1.0f, 0.5f, 0.0f), psp_identity(), kFrameW, kFrameH);
             require(near(out.x, 480.0f) && near(out.y, 68.0f), "x/y " + std::to_string(out.x) + "," + std::to_string(out.y));
         }},
        {"perspective divide by clip w", [] {
             auto t = psp_identity();
             t.model_to_clip[15] = 2.0f; // clip w = 2
             const auto out = vcsnx::emulate_hw_vertex(model(2.0f, 0.0f, 0.0f), t, kFrameW, kFrameH);
             require(near(out.w, 2.0f) && near(out.x, 480.0f), "x " + std::to_string(out.x));
         }},
        {"uv scale and offset", [] {
             auto t = psp_identity();
             t.uv_scale_u = 2.0f;
             t.uv_scale_v = 4.0f;
             t.uv_offset_u = 0.1f;
             t.uv_offset_v = 0.2f;
             auto v = model(0.0f, 0.0f, 0.0f);
             v.u = 0.5f;
             v.v = 0.25f;
             const auto out = vcsnx::emulate_hw_vertex(v, t, kFrameW, kFrameH);
             require(near(out.u, 1.1f, 1.0e-5f) && near(out.v, 1.2f, 1.0e-5f), "uv");
         }},
        {"fog from view-space z", [] {
             auto t = psp_identity();
             t.model_to_view_z = {0.0f, 0.0f, 1.0f, 0.0f};
             t.fog_end = 20.0f;
             t.fog_slope = 0.1f;
             require(near(vcsnx::emulate_hw_vertex(model(0.0f, 0.0f, -15.0f), t, kFrameW, kFrameH).fog, 0.5f), "half fog");
             require(near(vcsnx::emulate_hw_vertex(model(0.0f, 0.0f, -10.0f), t, kFrameW, kFrameH).fog, 1.0f), "clamped");
         }},
        {"packed 0x0115 bytes: u8 uv /128, 5551 colour, s16 xyz /32768", [] {
             // u=64 v=128, colour 0x801F (red, alpha set), x=16384 y=-32768 z=0.
             const std::array<std::uint8_t, 10> raw{64u, 128u, 0x1Fu, 0x80u, 0x00u, 0x40u, 0x00u, 0x80u, 0x00u, 0x00u};
             const auto out = vcsnx::emulate_hw_packed_0115(raw.data(), psp_identity(), kFrameW, kFrameH);
             require(near(out.x, 360.0f) && near(out.y, 272.0f), "x/y " + std::to_string(out.x) + "," + std::to_string(out.y));
             require(near(out.u, 0.5f, 1.0e-6f) && near(out.v, 1.0f, 1.0e-6f), "uv");
             require(out.rgba == std::array<std::uint8_t, 4>{255u, 0u, 0u, 255u}, "colour");
         }},
        {"vertices within tolerance count as compared, not bad", [] {
             vcsnx::HwVerifier verifier;
             verifier.begin_draw();
             const auto out = vcsnx::emulate_hw_vertex(model(0.0f, 0.0f, 0.0f), psp_identity(), kFrameW, kFrameH);
             auto ref = reference_from(out);
             ref.x += 0.4f;
             ref.z += 1.5f;
             require(verifier.add_vertex(out, ref) == 0u, "inside tolerance");
             require(verifier.counts().draws == 1u && verifier.counts().compared == 1u, "counted");
         }},
        {"each kind of difference has its own counter", [] {
             vcsnx::HwVerifier verifier;
             verifier.begin_draw();
             const auto out = vcsnx::emulate_hw_vertex(model(0.0f, 0.0f, 0.0f), psp_identity(), kFrameW, kFrameH);
             auto ref = reference_from(out);
             ref.x += 3.0f;
             ref.z += 10.0f;
             ref.u += 0.01f;
             ref.rgba[1] = 7u;
             ref.fog = out.fog - 0.5f;
             const unsigned mask = verifier.add_vertex(out, ref);
             const auto &c = verifier.counts();
             require(mask == (vcsnx::kHwBadPosition | vcsnx::kHwBadDepth | vcsnx::kHwBadUv |
                              vcsnx::kHwBadColor | vcsnx::kHwBadFog), "mask");
             require(c.pos_bad == 1u && c.depth_bad == 1u && c.uv_bad == 1u && c.color_bad == 1u && c.fog_bad == 1u,
                     "counters");
             require(near(c.max_pos_err_px, 3.0f), "max error");
         }},
        {"a colour off by one LSB is rounding, two is a mismatch", [] {
             // The shaders floor lit colour to RGBA8 while the CPU path rounds.
             vcsnx::HwVerifier verifier;
             verifier.begin_draw();
             const auto out = vcsnx::emulate_hw_vertex(model(0.0f, 0.0f, 0.0f), psp_identity(), kFrameW, kFrameH);
             auto ref = reference_from(out);
             ref.rgba[0] = static_cast<std::uint8_t>(out.rgba[0] - 1u);
             require(verifier.add_vertex(out, ref) == 0u, "one LSB");
             ref.rgba[0] = static_cast<std::uint8_t>(out.rgba[0] - 2u);
             require(verifier.add_vertex(out, ref) == vcsnx::kHwBadColor, "two LSB");
         }},
        {"details: four per vertex format and mismatch kind, 64 in all", [] {
             vcsnx::HwVerifier verifier;
             int first = 0, second = 0;
             for (int i = 0; i < 10; ++i) first += verifier.want_detail(0x000115u, true, vcsnx::kHwBadColor) ? 1 : 0;
             for (int i = 0; i < 10; ++i) second += verifier.want_detail(0x00011Cu, false, vcsnx::kHwBadColor) ? 1 : 0;
             require(first == 4 && second == 4, "per kind");
             int total = 8;
             for (std::uint32_t vtype = 0u; vtype < 40u; ++vtype)
                 for (int i = 0; i < 4; ++i) total += verifier.want_detail(vtype, false, vcsnx::kHwBadUv) ? 1 : 0;
             require(total == 64, "capped at 64, got " + std::to_string(total));
         }},
        {"projected texture coordinates are compared after dividing by q", [] {
             // GE texture-matrix UVs (shadows) carry q; psp_ge.frag samples at (u/q, v/q).
             vcsnx::HwVerifier verifier;
             verifier.begin_draw();
             auto textured = model(0.0f, 0.0f, 0.0f);
             textured.u = 0.25f;
             textured.v = 0.5f;
             auto out = vcsnx::emulate_hw_vertex(textured, psp_identity(), kFrameW, kFrameH);
             auto ref = reference_from(out);
             ref.u = out.u * 2.0f;
             ref.v = out.v * 2.0f;
             ref.q = 2.0f; // same projected coordinates
             out.q = 1.0f;
             out.u = ref.u / 2.0f;
             out.v = ref.v / 2.0f;
             require(verifier.add_vertex(out, ref) == 0u, "same u/q");
             out.u = ref.u; // q dropped: twice as far
             out.v = ref.v;
             out.u += 0.5f;
             require(verifier.add_vertex(out, ref) == vcsnx::kHwBadUv, "q ignored");
         }},
        {"the hardware vertex keeps the decoded q, as psp_ge_hw.vert must", [] {
             auto v = model(0.0f, 0.0f, 0.0f);
             v.q = 3.0f;
             require(vcsnx::emulate_hw_vertex(v, psp_identity(), kFrameW, kFrameH).q == 3.0f, "q");
         }},
        {"far off-screen vertices use a relative position tolerance", [] {
             // Float rounding at x ~ 3e5 px is ~1e-6 relative; such vertices are clipped anyway.
             vcsnx::HwVerifier verifier;
             verifier.begin_draw();
             auto out = vcsnx::emulate_hw_vertex(model(0.0f, 0.0f, 0.0f), psp_identity(), kFrameW, kFrameH);
             auto ref = reference_from(out);
             out.x = 327402.72f;
             ref.x = 327400.75f;
             require(verifier.add_vertex(out, ref) == 0u, "1e-6 relative");
             ref.x = 327000.0f;
             require(verifier.add_vertex(out, ref) == vcsnx::kHwBadPosition, "1e-3 relative is a mismatch");
         }},
        {"periodic frame dumps: every N vblanks, once per period, off when N is 0", [] {
             vcsnx::PeriodicDump dump(600u);
             require(!dump.due(0u) && !dump.due(599u), "not before the first period");
             require(dump.due(600u) && !dump.due(601u), "once at 600");
             require(dump.due(1350u) && !dump.due(1799u) && dump.due(1800u), "skipped periods do not pile up");
             vcsnx::PeriodicDump off(0u);
             require(!off.due(600u) && !off.due(123456u), "disabled");
         }},
        {"excluded vertex types: comma-separated hex list keeps those draws on the CPU path", [] {
             const vcsnx::VertexTypeFilter filter("0x00119f, 0x0011E1,bogus,0x000121");
             require(!filter.allows(0x00119Fu) && !filter.allows(0x0011E1u) && !filter.allows(0x000121u), "excluded");
             require(filter.allows(0x000115u), "others still allowed");
             require(vcsnx::VertexTypeFilter(nullptr).allows(0x00119Fu), "empty list allows all");
         }},
        {"vertices behind the camera are clipped, not compared", [] {
             vcsnx::HwVerifier verifier;
             verifier.begin_draw();
             vcsnx::HwVertexOut out{};
             vcsnx::HwReference ref{};
             ref.clipped = true;
             require(verifier.add_vertex(out, ref) == 0u, "no mismatch");
             require(verifier.counts().clipped == 1u && verifier.counts().compared == 0u, "clipped");
         }},
        {"frame size defaults to the Vulkan backend's 512x320 guest frame", [] {
             const auto [w, h] = vcsnx::hw_verify_frame_extent();
             require(w == 512.0f && h == 320.0f, "frame " + std::to_string(w) + "x" + std::to_string(h));
         }},
        {"the summary counts draws the backend rejected or dropped in that window", [] {
             vcsnx::HwVerifier verifier;
             require(!verifier.take_report_if_due(0, {5u, 1u, 7u}).has_value(), "starts the window");
             const auto line = verifier.take_report_if_due(10'000, {8u, 1u, 9u});
             require(line.has_value() && line->find(" hw_rejected=3 vertex_overflows=0 texture_missing=2") !=
                                             std::string::npos,
                     "summary: " + line.value_or(""));
             const auto next = verifier.take_report_if_due(20'000, {8u, 3u, 9u});
             require(next.has_value() && next->find(" hw_rejected=0 vertex_overflows=2 texture_missing=0") !=
                                             std::string::npos,
                     "next window: " + next.value_or(""));
         }},
        {"one summary line per 10 s window, then the counters restart", [] {
             vcsnx::HwVerifier verifier;
             require(!verifier.take_report_if_due(0).has_value(), "starts the window");
             verifier.begin_draw();
             require(!verifier.take_report_if_due(9'999).has_value(), "not yet");
             const auto line = verifier.take_report_if_due(10'000);
             require(line.has_value() && line->rfind("[hw-verify] draws=1 ", 0) == 0u, "summary: " + line.value_or(""));
             require(verifier.counts().draws == 0u, "restarted");
         }},
    });
}
