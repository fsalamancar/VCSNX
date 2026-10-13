#include "game_module.hpp"

#include <switch.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" void __register_frame(const void *begin); // libgcc: makes the module's frames unwindable

namespace vcsnx {
namespace {

constexpr std::uint64_t kPage = 0x1000;

bool read_file(const std::string &path, std::vector<std::uint8_t> &out) {
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) return false;
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    out.resize(size > 0 ? static_cast<std::size_t>(size) : 0u);
    const bool ok = size > 0 && std::fread(out.data(), 1, out.size(), file) == out.size();
    std::fclose(file);
    return ok;
}

std::string hex(Result rc) {
    char text[16];
    std::snprintf(text, sizeof text, "0x%08X", rc);
    return text;
}

} // namespace

std::optional<LoadedModule> load_module(const std::string &path, std::span<const ModuleImport> imports,
                                        std::span<const std::string_view> exports, std::string &error) {
    std::vector<std::uint8_t> file;
    if (!read_file(path, file)) {
        error = "cannot read " + path;
        return std::nullopt;
    }
    auto elf = ElfModule::parse(std::move(file), error);
    if (!elf) return std::nullopt;
    const std::uint64_t size = elf->image_size();
    // The source of svcMapProcessCodeMemory: heap, page-aligned. Once mapped it stays reserved for the
    // module for the rest of the run, so it is never freed.
    auto *image = static_cast<std::uint8_t *>(std::aligned_alloc(kPage, size));
    if (image == nullptr) {
        error = "not enough memory for " + std::to_string(size >> 20) + " MB of game code";
        return std::nullopt;
    }
    std::memset(image, 0, size);
    elf->copy_into({image, size});

    virtmemLock();
    void *target = virtmemFindCodeMemory(size, kPage);
    VirtmemReservation *reservation = target != nullptr ? virtmemAddReservation(target, size) : nullptr;
    virtmemUnlock();
    if (reservation == nullptr) {
        std::free(image);
        error = "no address space for " + std::to_string(size >> 20) + " MB of game code";
        return std::nullopt;
    }
    const auto base = reinterpret_cast<std::uint64_t>(target);
    RelocationStats stats;
    if (!elf->relocate({image, size}, base, imports, stats, error)) {
        virtmemLock();
        virtmemRemoveReservation(reservation);
        virtmemUnlock();
        std::free(image);
        return std::nullopt;
    }
    LoadedModule loaded{base, size, {}};
    for (std::string_view name : exports) {
        const auto offset = elf->symbol_offset(name);
        if (!offset) {
            error = "the module does not export " + std::string(name);
            return std::nullopt;
        }
        loaded.symbols.push_back(reinterpret_cast<void *>(base + *offset));
    }
    const auto eh_frame = elf->eh_frame();
    const auto segments = elf->segments();
    elf.reset(); // the file copy is no longer needed

    const Handle self = envGetOwnProcessHandle();
    Result rc = svcMapProcessCodeMemory(self, base, reinterpret_cast<std::uint64_t>(image), size);
    if (R_FAILED(rc)) {
        error = "svcMapProcessCodeMemory failed: " + hex(rc);
        return std::nullopt;
    }
    // The kernel forbids W->X on code memory: RX pages first, then the rest RW (as so_util.c's so_finalize).
    std::vector<bool> executable(size / kPage, false);
    for (const ModuleSegment &segment : segments)
        if (segment.executable)
            for (std::uint64_t page = segment.offset / kPage; page < (segment.offset + segment.size) / kPage; ++page)
                executable[page] = true;
    for (const bool want_x : {true, false}) {
        for (std::size_t page = 0; page < executable.size();) {
            if (executable[page] != want_x) {
                ++page;
                continue;
            }
            std::size_t end = page;
            while (end < executable.size() && executable[end] == want_x) ++end;
            rc = svcSetProcessMemoryPermission(self, base + page * kPage, (end - page) * kPage, want_x ? Perm_Rx : Perm_Rw);
            if (R_FAILED(rc)) {
                error = "svcSetProcessMemoryPermission failed: " + hex(rc);
                return std::nullopt;
            }
            page = end;
        }
    }
    armDCacheFlush(target, size);
    armICacheInvalidate(target, size);
    if (eh_frame) __register_frame(reinterpret_cast<const void *>(base + eh_frame->first));
    return loaded;
}

} // namespace vcsnx
