// The fork rewrites ENGLISH.GXT so the control prompts name the buttons of the device in use.
// VCSNX has only the Switch pad: every prompt must name the Switch buttons in the PSP buttons'
// buttons with Nintendo's A-confirms layout (cross A, circle B, square Y, triangle X), never keyboard
// keys or Xbox buttons.
#include "test_support.hpp"
#include "vcs_key_prompts.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

void put32(Bytes &out, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFFu));
}

void put_utf16(Bytes &out, const std::u16string &text) {
    for (char16_t c : text) {
        out.push_back(static_cast<std::uint8_t>(c & 0xFFu));
        out.push_back(static_cast<std::uint8_t>(c >> 8u));
    }
}

// A one-table (MAIN) GXT: TABL list, then TKEY entries and TDAT UTF-16 strings, as the game ships it.
Bytes make_gxt(const std::vector<std::pair<std::string, std::u16string>> &strings) {
    Bytes data;
    std::vector<std::uint32_t> offsets;
    for (const auto &[key, text] : strings) {
        offsets.push_back(static_cast<std::uint32_t>(data.size()));
        put_utf16(data, text + u'\0');
    }
    Bytes out{'T', 'A', 'B', 'L'};
    put32(out, 12u);
    const char main_name[8] = {'M', 'A', 'I', 'N', 0, 0, 0, 0};
    out.insert(out.end(), main_name, main_name + 8);
    put32(out, 20u);
    out.insert(out.end(), {'T', 'K', 'E', 'Y'});
    put32(out, static_cast<std::uint32_t>(strings.size() * 12u));
    for (std::size_t i = 0; i < strings.size(); ++i) {
        put32(out, offsets[i]);
        char key[8] = {};
        strings[i].first.copy(key, 8);
        out.insert(out.end(), key, key + 8);
    }
    out.insert(out.end(), {'T', 'D', 'A', 'T'});
    put32(out, static_cast<std::uint32_t>(data.size()));
    out.insert(out.end(), data.begin(), data.end());
    return out;
}

bool contains_utf16(const Bytes &file, const std::u16string &text) {
    Bytes needle;
    put_utf16(needle, text + u'\0');
    return std::search(file.begin(), file.end(), needle.begin(), needle.end()) != file.end();
}

Bytes rewrite(const std::vector<std::pair<std::string, std::u16string>> &strings) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "vcsnx_test_switch_prompts";
    std::filesystem::create_directories(dir);
    const std::filesystem::path original = dir / "ENGLISH.GXT";
    const Bytes gxt = make_gxt(strings);
    std::ofstream(original, std::ios::binary).write(reinterpret_cast<const char *>(gxt.data()),
                                                    static_cast<std::streamsize>(gxt.size()));
    const auto patched = vcs::vcs_keyboard_prompt_gxt(original);
    vcsnx_test::require(patched.has_value(), "the rewrite produced no file");
    std::ifstream in(*patched, std::ios::binary);
    return Bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"button glyphs name the Switch buttons, A confirming", [] {
             const Bytes out = rewrite({{"HELP1", u"Press ~X~ to jump, ~O~ to attack, ~S~ to sprint, ~T~ to enter"}});
             require(contains_utf16(out, u"Press A to jump, B to attack, Y to sprint, X to enter"),
                     "glyphs not rewritten to A/B/Y/X");
         }},
        {"control names use Switch wording, L and R stay", [] {
             const Bytes out = rewrite({{"C0LOOK", u"L button + analog stick left"},
                                        {"C1MAP", u"SELECT button"},
                                        {"C2RADIO", u"up button and down button"},
                                        {"C3FIRE", u"CIRCLE"}});
             require(contains_utf16(out, u"L button + Left stick left"), "analog stick / L button");
             require(contains_utf16(out, u"- button"), "SELECT is the Minus button");
             require(contains_utf16(out, u"D-pad up and down"), "directional buttons");
             require(contains_utf16(out, u"B"), "circle is B");
         }},
        {"accelerate and brake name the triggers, as the pad mapping does", [] {
             const Bytes out = rewrite({{"C0VEACC", u"CROSS"},
                                        {"C1VEBRK", u"SQUARE"},
                                        {"C0VEHB", u"SQUARE + CROSS"},
                                        {"C2VEHB", u"R button"},
                                        {"C0VEWE2", u"SQUARE + CROSS"}});
             require(contains_utf16(out, u"ZR"), "accelerate is ZR");
             require(contains_utf16(out, u"ZL"), "brake is ZL");
             require(contains_utf16(out, u"ZL + ZR"), "brake + accelerate is ZL + ZR");
             require(contains_utf16(out, u"R button"), "a handbrake on R stays R");
             require(contains_utf16(out, u"Y + A"), "the helicopter cannon keeps its face buttons");
         }},
        {"ordinary text keeps its words, only glyphs change", [] {
             const Bytes out = rewrite({{"MSG1", u"Use the analog stick and ~X~"}});
             require(contains_utf16(out, u"Use the analog stick and A"), "dialogue phrases must stay");
         }},
    });
}
