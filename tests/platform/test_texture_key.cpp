// The replacement key the console computes; Builder.Core/Hd/TextureKey.cs must give the same value.
#include "vcs_texture_replace.hpp"
#include "test_support.hpp"

#include <array>
#include <cstddef>

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"a 4x4 image of bytes 0..63 has the key shared with the Builder test", [] {
             std::array<std::byte, 64> rgba{};
             for (std::size_t i = 0; i < rgba.size(); ++i) rgba[i] = static_cast<std::byte>(i);
             require(vcs::texture_content_hash(rgba, 4u, 4u) == 0x7b21ac69b86eb9d5ull, "shared key");
         }},
    });
}
