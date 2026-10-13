// The world archives are read into RAM once, in the background; reads come from there when a file is in.
#include "ram_files.hpp"
#include "test_support.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::filesystem::path write_file(const std::string &name, std::size_t size) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    for (std::size_t i = 0; i < size; ++i) out.put(static_cast<char>((i * 7u + 3u) & 0xFFu));
    return path;
}

} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"a file is in RAM once loaded, byte for byte", [] {
             const auto a = write_file("vcsnx_ram_a.img", 5u * 1024u * 1024u + 123u);
             const auto b = write_file("vcsnx_ram_b.img", 4096u);
             vcsnx::RamFiles files;
             files.start({a, b});
             files.wait();
             const auto in_ram = files.find(a);
             require(in_ram.size() == 5u * 1024u * 1024u + 123u, "size " + std::to_string(in_ram.size()));
             bool same = true;
             for (std::size_t i = 0; i < in_ram.size(); i += 4099u)
                 same = same && in_ram[i] == static_cast<std::uint8_t>((i * 7u + 3u) & 0xFFu);
             require(same, "contents");
             require(files.find(b).size() == 4096u, "the second file too");
             std::string upper = a.generic_string();
             upper.replace(upper.size() - 3u, 3u, "IMG");
             require(files.find(upper).size() == in_ram.size(), "the name's case does not matter");
             std::filesystem::remove(a);
             std::filesystem::remove(b);
         }},
        {"a file it was not asked for, or that does not exist, is not in RAM", [] {
             const auto a = write_file("vcsnx_ram_c.img", 1000u);
             vcsnx::RamFiles files;
             files.start({a, std::filesystem::temp_directory_path() / "vcsnx_ram_missing.img"});
             files.wait();
             require(files.find(std::filesystem::temp_directory_path() / "vcsnx_ram_missing.img").empty(), "missing");
             require(files.find(std::filesystem::temp_directory_path() / "vcsnx_ram_other.img").empty(), "not asked");
             require(files.find(a).size() == 1000u, "the one that exists");
             std::filesystem::remove(a);
         }},
        {"before start nothing is in RAM", [] {
             vcsnx::RamFiles files;
             require(files.find("/x/MAINLA.IMG").empty(), "empty");
             files.wait(); // nothing to wait for
         }},
        {"the setting turns it off", [] {
             require(vcsnx::ram_streaming_enabled(nullptr), "on by default");
             require(vcsnx::ram_streaming_enabled("1"), "1");
             require(!vcsnx::ram_streaming_enabled("0"), "0 turns it off");
         }},
    });
}
