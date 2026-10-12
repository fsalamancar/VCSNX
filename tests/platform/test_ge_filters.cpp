// The red line trail VCS draws into the world target every frame is skipped; other line draws are not.
#include "ge_filters.hpp"
#include "test_support.hpp"

namespace {

// From the Mac [line-diag] log: prim=2 through=1 tex=0 blend=1 eq=0 src=10 dst=10 fix=ffffff/ffffff.
vcsnx::GeDrawState trail() {
    vcsnx::GeDrawState s;
    s.through = true;
    s.primitive = 2u;
    s.textured = false;
    s.blend = true;
    s.blend_equation = 0u;
    s.blend_source = 10u;
    s.blend_dest = 10u;
    s.blend_fix_source = 0xFFFFFFu;
    s.blend_fix_dest = 0xFFFFFFu;
    return s;
}

} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"the additive red line trail is skipped", [] {
             require(vcsnx::is_hidden_line_trail(trail()), "line strip");
             auto lines = trail();
             lines.primitive = 1u;
             require(vcsnx::is_hidden_line_trail(lines), "separate lines");
         }},
        {"other draws are kept", [] {
             auto triangles = trail();
             triangles.primitive = 3u;
             require(!vcsnx::is_hidden_line_trail(triangles), "triangles with the same blend");
             auto textured = trail();
             textured.textured = true;
             require(!vcsnx::is_hidden_line_trail(textured), "textured lines");
             auto alpha_blended = trail();
             alpha_blended.blend_source = 2u;  // source alpha
             alpha_blended.blend_dest = 3u;    // one minus source alpha
             require(!vcsnx::is_hidden_line_trail(alpha_blended), "ordinary blended lines (radar helpers)");
             auto world = trail();
             world.through = false;
             require(!vcsnx::is_hidden_line_trail(world), "3D lines");
             auto unblended = trail();
             unblended.blend = false;
             require(!vcsnx::is_hidden_line_trail(unblended), "opaque lines");
         }},
    });
}
