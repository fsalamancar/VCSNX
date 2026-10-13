// The game module loader's pure part: a devkitA64 shared object copied and relocated into a buffer.
#include "elf_module.hpp"
#include "test_support.hpp"

#include <cstring>
#include <fstream>
#include <iterator>

namespace {

std::vector<std::uint8_t> read_file(const char *path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::vector<std::uint8_t> fixture() { return read_file(VCSNX_FIXTURE_MODULE); }

std::uint64_t read64(const std::vector<std::uint8_t> &image, std::uint64_t offset) {
    std::uint64_t value = 0;
    std::memcpy(&value, image.data() + offset, sizeof value);
    return value;
}

constexpr std::uint64_t kLoad = 0x0000'0080'0000'0000ull;
constexpr std::uint64_t kImport = 0x0000'0012'3450'0000ull;

} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"a devkitA64 shared object parses into page-aligned segments", [] {
             std::string error;
             const auto module = vcsnx::ElfModule::parse(fixture(), error);
             require(module.has_value(), "parse: " + error);
             require(module->image_size() > 0 && module->image_size() % 0x1000 == 0, "image size");
             bool code = false, data = false;
             for (const auto &segment : module->segments()) {
                 require(segment.offset % 0x1000 == 0 && segment.size % 0x1000 == 0, "aligned segment");
                 code = code || segment.executable;
                 data = data || segment.writable;
             }
             require(code && data, "a code and a data segment");
         }},
        {"something that is not an AArch64 shared object is refused", [] {
             std::string error;
             require(!vcsnx::ElfModule::parse(std::vector<std::uint8_t>(64, 0x41), error).has_value(), "garbage");
             require(!error.empty(), "with a reason");
         }},
        {"relocation points the module at itself and at its imports", [] {
             std::string error;
             const auto module = vcsnx::ElfModule::parse(fixture(), error);
             require(module.has_value(), error);
             std::vector<std::uint8_t> image(module->image_size());
             module->copy_into(image);
             const vcsnx::ModuleImport imports[] = {{"fixture_import", kImport}};
             vcsnx::RelocationStats stats;
             require(module->relocate(image, kLoad, imports, stats, error), "relocate: " + error);
             const auto counter = module->symbol_offset("fixture_counter");
             const auto counter_address = module->symbol_offset("fixture_counter_address");
             const auto import_address = module->symbol_offset("fixture_import_address");
             require(counter && counter_address && import_address, "exported symbols");
             std::int32_t value = 0;
             std::memcpy(&value, image.data() + *counter, sizeof value);
             require(value == 7, "data copied");
             require(read64(image, *counter_address) == kLoad + *counter, "pointer into the module");
             require(read64(image, *import_address) == kImport, "pointer to the import");
             require(stats.relative >= 1 && stats.symbolic >= 2, "relative and symbolic relocations applied");
             require(module->symbol_offset("fixture_call").has_value(), "functions are found too");
             require(!module->symbol_offset("fixture_import").has_value(), "an import is not an export");
         }},
        {"a missing import is an error that names it", [] {
             std::string error;
             const auto module = vcsnx::ElfModule::parse(fixture(), error);
             require(module.has_value(), error);
             std::vector<std::uint8_t> image(module->image_size());
             module->copy_into(image);
             vcsnx::RelocationStats stats;
             require(!module->relocate(image, kLoad, {}, stats, error), "fails");
             require(error.find("fixture_import") != std::string::npos, "names the symbol: " + error);
         }},
        {"the module's unwind tables are found", [] {
             std::string error;
             const auto module = vcsnx::ElfModule::parse(fixture(), error);
             require(module.has_value(), error);
             const auto eh = module->eh_frame();
             require(eh.has_value() && eh->second > 0 && eh->first + eh->second <= module->image_size(), ".eh_frame");
         }},
        {"unwind tables without their end marker are refused", [] {
             // libgcc walks a registered .eh_frame until a zero word; without one it reads past the module
             // and crashes on the first exception thrown.
             std::string error;
             const auto module = vcsnx::ElfModule::parse(read_file(VCSNX_FIXTURE_MODULE_UNTERMINATED), error);
             require(!module.has_value(), "refused");
             require(error.find(".eh_frame") != std::string::npos, "says why: " + error);
         }},
    });
}
