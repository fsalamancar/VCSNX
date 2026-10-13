// The crash report names every address by where it lives and says what the faulting page is.
#include "crash_log.hpp"
#include "test_support.hpp"

#include <string>

namespace {

vcsnx::CrashFacts facts_of_2026_10_13() {
    // The GE worker's data abort of 2026-10-13 (Atmosphère report 01791897587).
    vcsnx::CrashFacts facts;
    facts.error_desc = 0x101u;
    facts.esr = 0x92000007u;
    facts.far = 0x1f50b45002u;
    facts.pc = 0x3bb93487f0u;
    facts.lr = 0x3bb93487c8u;
    facts.sp = 0xba8ee5bd0u;
    facts.fp = 0xba8ee5bf0u;
    facts.x[0] = 0x1f50b44da0u;
    facts.x[28] = 0xba8ee6e90u;
    facts.runtime_base = 0x3bb9248000u;
    facts.runtime_size = 0x14bd000u;
    facts.module_base = 0x31fda56000u;
    facts.module_size = 0x4479000u;
    facts.thread_id = 0xe81u;
    facts.fault_page = {0x1f50b45000u, 0x20000u, 0x5u, 0x1u, 0x0u, 0u, 0u, true};
    facts.page_before = {0x1f40000000u, 0x10b45000u, 0x5u, 0x0u, 0x3u, 0u, 0u, true};
    facts.stack = {0xba8ae8000u, 0x402000u, 0xbu, 0x0u, 0x3u, 0u, 0u, true};
    facts.frames[0] = 0x3bb92fa5b8u;
    facts.frames[1] = 0x31fda57010u;
    facts.frames[2] = 0x12345u;
    facts.frame_count = 3u;
    return facts;
}

bool has(const std::string &text, const std::string &part) { return text.find(part) != std::string::npos; }

} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"the report names the fault and where each address lives", [] {
             char out[4096];
             const std::size_t length = vcsnx::format_crash_report(facts_of_2026_10_13(), out, sizeof out);
             const std::string text(out, length);
             require(has(text, "[vcsnx-crash] thread=0xe81 desc=0x101 esr=0x92000007 far=0x1f50b45002"), text);
             require(has(text, "pc=vcsnx+0x1007f0"), "pc in vcsnx.nro: " + text);
             require(has(text, "lr=vcsnx+0x1007c8"), "lr: " + text);
             require(has(text, "x0=0x1f50b44da0"), "registers: " + text);
             require(has(text, "x28=0xba8ee6e90"), "the last register: " + text);
             require(has(text, "fault page: base=0x1f50b45000 size=0x20000 type=0x5 attr=0x1 perm=0x0 ipc=0 device=0"),
                     "the faulting page as the kernel sees it: " + text);
             require(has(text, "page before: base=0x1f40000000"), "its neighbour: " + text);
             require(has(text, "stack: base=0xba8ae8000"), "the stack region: " + text);
             require(has(text, "frames: vcsnx+0xb25b8 vcs_game.so+0x1010 0x12345"), "frames: " + text);
         }},
        {"a region the kernel did not describe says so", [] {
             vcsnx::CrashFacts facts = facts_of_2026_10_13();
             facts.fault_page.known = false;
             char out[4096];
             const std::string text(out, vcsnx::format_crash_report(facts, out, sizeof out));
             require(has(text, "fault page: unknown"), text);
         }},
        {"a small buffer is cut, never overrun", [] {
             char out[64];
             out[63] = 'X';
             const std::size_t length = vcsnx::format_crash_report(facts_of_2026_10_13(), out, 40u);
             require(length < 40u && out[length] == '\0', "NUL inside the capacity");
             require(out[63] == 'X', "nothing past the capacity");
         }},
    });
}
