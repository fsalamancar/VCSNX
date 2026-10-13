#pragma once
// Reads an AArch64 shared object (ET_DYN) and prepares it to run at a given address: copies its loadable
// segments into an image and applies its dynamic relocations, resolving imports from a name table. Pure
// (no memory mapping), so the Mac tests it; platform/switch/game_module maps the image on the console.
// Adapted from gtalcs_nx/source/so_util.c (MIT, (C) 2021 Andy Nguyen, fgsfds).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vcsnx {

struct ModuleImport {
    std::string_view name;
    std::uint64_t address;
};

struct ModuleSegment {
    std::uint64_t offset; // in the image, page-aligned
    std::uint64_t size;   // page-aligned
    bool executable;
    bool writable;
};

struct RelocationStats {
    std::size_t relative{};
    std::size_t symbolic{};
};

class ElfModule {
public:
    static std::optional<ElfModule> parse(std::vector<std::uint8_t> file, std::string &error);

    [[nodiscard]] std::uint64_t image_size() const noexcept { return image_size_; }
    [[nodiscard]] const std::vector<ModuleSegment> &segments() const noexcept { return segments_; }
    void copy_into(std::span<std::uint8_t> image) const;
    bool relocate(std::span<std::uint8_t> image, std::uint64_t load_address, std::span<const ModuleImport> imports,
                  RelocationStats &stats, std::string &error) const;
    [[nodiscard]] std::optional<std::uint64_t> symbol_offset(std::string_view name) const;
    [[nodiscard]] std::optional<std::pair<std::uint64_t, std::uint64_t>> eh_frame() const { return eh_frame_; }

private:
    struct Load {
        std::uint64_t file_offset, vaddr, file_size;
    };
    struct RelaTable {
        std::uint64_t offset, count;
    };

    std::vector<std::uint8_t> file_;
    std::uint64_t image_size_{};
    std::vector<ModuleSegment> segments_;
    std::vector<Load> loads_;
    std::uint64_t dynsym_offset_{}, dynsym_count_{}, dynstr_offset_{}, dynstr_size_{};
    std::vector<RelaTable> relas_;
    std::optional<std::pair<std::uint64_t, std::uint64_t>> eh_frame_;
};

} // namespace vcsnx
