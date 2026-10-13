// File reads reach the game's memory through a private buffer: same bytes, same counts.
#include "guest_io.hpp"
#include "test_support.hpp"

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string pattern(std::size_t size) {
    std::string text(size, '\0');
    for (std::size_t i = 0; i < size; ++i) text[i] = static_cast<char>((i * 131u + 7u) & 0xFFu);
    return text;
}

bool same(const std::vector<std::uint8_t> &memory, const std::string &text, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i)
        if (memory[i] != static_cast<std::uint8_t>(text[i])) return false;
    return true;
}

} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"a small read arrives whole", [] {
             const std::string file = pattern(1000u);
             std::istringstream input(file);
             std::vector<std::uint8_t> memory(1000u, 0xAAu);
             require(vcsnx::read_into_guest(input, memory.data(), 1000u) == 1000u, "count");
             require(same(memory, file, 1000u), "bytes");
         }},
        {"a read larger than the private buffer arrives whole and in order", [] {
             const std::size_t size = 3u * 1024u * 1024u + 12345u;
             const std::string file = pattern(size);
             std::istringstream input(file);
             std::vector<std::uint8_t> memory(size, 0u);
             require(vcsnx::read_into_guest(input, memory.data(), size) == size, "count");
             require(same(memory, file, size), "bytes");
         }},
        {"a read past the end returns what the file had and leaves the rest alone", [] {
             const std::string file = pattern(700000u);
             std::istringstream input(file);
             std::vector<std::uint8_t> memory(1000000u, 0x55u);
             require(vcsnx::read_into_guest(input, memory.data(), memory.size()) == 700000u, "short count");
             require(same(memory, file, 700000u), "bytes");
             require(memory[700000u] == 0x55u && memory.back() == 0x55u, "untouched past the data");
         }},
        {"reads continue from where the last one stopped", [] {
             const std::string file = pattern(2000000u);
             std::istringstream input(file);
             std::vector<std::uint8_t> memory(2000000u, 0u);
             require(vcsnx::read_into_guest(input, memory.data(), 1500000u) == 1500000u, "first");
             require(vcsnx::read_into_guest(input, memory.data() + 1500000u, 500000u) == 500000u, "second");
             require(same(memory, file, 2000000u), "bytes");
         }},
        {"nothing to read, nothing written", [] {
             std::istringstream input("");
             std::vector<std::uint8_t> memory(16u, 0x11u);
             require(vcsnx::read_into_guest(input, memory.data(), 16u) == 0u, "zero");
             require(memory[0] == 0x11u, "untouched");
         }},
    });
}
