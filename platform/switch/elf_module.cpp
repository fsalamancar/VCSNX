#include "elf_module.hpp"

#include <algorithm>
#include <cstring>
#include <unordered_map>

namespace vcsnx {
namespace {

// ELF64 structures, little-endian (the Mac has no <elf.h>).
struct Ehdr {
    unsigned char ident[16];
    std::uint16_t type, machine;
    std::uint32_t version;
    std::uint64_t entry, phoff, shoff;
    std::uint32_t flags;
    std::uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
};
struct Phdr {
    std::uint32_t type, flags;
    std::uint64_t offset, vaddr, paddr, filesz, memsz, align;
};
struct Shdr {
    std::uint32_t name, type;
    std::uint64_t flags, addr, offset, size;
    std::uint32_t link, info;
    std::uint64_t addralign, entsize;
};
struct Sym {
    std::uint32_t name;
    unsigned char info, other;
    std::uint16_t shndx;
    std::uint64_t value, size;
};
struct Rela {
    std::uint64_t offset, info;
    std::int64_t addend;
};
static_assert(sizeof(Ehdr) == 64 && sizeof(Phdr) == 56 && sizeof(Shdr) == 64 && sizeof(Sym) == 24 && sizeof(Rela) == 24);

constexpr std::uint16_t kEtDyn = 3, kEmAarch64 = 183;
constexpr std::uint32_t kPtLoad = 1, kPfX = 1, kPfW = 2;
constexpr std::uint32_t kShtRela = 4, kShtDynsym = 11;
constexpr std::uint64_t kShfAlloc = 2;
constexpr std::uint32_t kRNone = 0, kRAbs64 = 257, kRGlobDat = 1025, kRJumpSlot = 1026, kRRelative = 1027;
constexpr unsigned char kStbWeak = 2;
constexpr std::uint64_t kPage = 0x1000;

std::uint64_t page_down(std::uint64_t v) { return v & ~(kPage - 1); }
std::uint64_t page_up(std::uint64_t v) { return (v + kPage - 1) & ~(kPage - 1); }

template <typename T>
bool read(const std::vector<std::uint8_t> &file, std::uint64_t offset, T &out) {
    if (offset > file.size() || file.size() - offset < sizeof(T)) return false;
    std::memcpy(&out, file.data() + offset, sizeof(T));
    return true;
}

std::string_view c_string(const std::vector<std::uint8_t> &file, std::uint64_t table, std::uint64_t table_size, std::uint32_t index) {
    if (index >= table_size || table + table_size > file.size()) return {};
    const auto *start = reinterpret_cast<const char *>(file.data() + table + index);
    const auto *end = static_cast<const char *>(std::memchr(start, 0, table_size - index));
    return {start, end != nullptr ? static_cast<std::size_t>(end - start) : static_cast<std::size_t>(table_size - index)};
}

// libgcc walks a registered .eh_frame entry by entry until a zero length word (a program's comes from
// crtend.o, a module's from module_eh_end.S linked last). Without it the walk runs past the module and the
// first exception thrown crashes inside the unwinder; with one in the middle the entries after it are lost.
bool frames_end_marked(const std::vector<std::uint8_t> &file, const Shdr &section) {
    std::uint64_t offset = 0;
    while (offset + 4 <= section.size) {
        std::uint32_t length = 0;
        if (!read(file, section.offset + offset, length) || length == 0xffffffffu) return false; // no 64-bit DWARF
        if (length == 0) {
            if (section.offset + section.size > file.size()) return false;
            const auto *rest = file.data() + section.offset + offset;
            return std::all_of(rest, file.data() + section.offset + section.size, [](std::uint8_t b) { return b == 0; });
        }
        offset += 4 + std::uint64_t{length};
    }
    return false;
}

} // namespace

std::optional<ElfModule> ElfModule::parse(std::vector<std::uint8_t> file, std::string &error) {
    Ehdr header{};
    if (!read(file, 0, header) || std::memcmp(header.ident, "\x7f" "ELF", 4) != 0 || header.ident[4] != 2 || header.ident[5] != 1) {
        error = "not a 64-bit little-endian ELF file";
        return std::nullopt;
    }
    if (header.type != kEtDyn || header.machine != kEmAarch64) {
        error = "not an AArch64 shared object";
        return std::nullopt;
    }
    ElfModule module;
    for (std::uint16_t i = 0; i < header.phnum; ++i) {
        Phdr ph{};
        if (!read(file, header.phoff + std::uint64_t{i} * header.phentsize, ph)) {
            error = "truncated program headers";
            return std::nullopt;
        }
        if (ph.type != kPtLoad) continue;
        if (ph.offset + ph.filesz > file.size() || ph.filesz > ph.memsz) {
            error = "a segment lies outside the file";
            return std::nullopt;
        }
        module.loads_.push_back({ph.offset, ph.vaddr, ph.filesz});
        const std::uint64_t first = page_down(ph.vaddr), last = page_up(ph.vaddr + ph.memsz);
        module.segments_.push_back({first, last - first, (ph.flags & kPfX) != 0, (ph.flags & kPfW) != 0});
        module.image_size_ = std::max(module.image_size_, last);
    }
    if (module.loads_.empty()) {
        error = "no loadable segments";
        return std::nullopt;
    }
    std::vector<Shdr> sections(header.shnum);
    for (std::uint16_t i = 0; i < header.shnum; ++i) {
        if (!read(file, header.shoff + std::uint64_t{i} * header.shentsize, sections[i])) {
            error = "truncated section headers";
            return std::nullopt;
        }
    }
    std::uint32_t dynsym = 0;
    for (std::uint32_t i = 0; i < sections.size(); ++i) {
        if (sections[i].type != kShtDynsym || sections[i].link >= sections.size()) continue;
        dynsym = i;
        module.dynsym_offset_ = sections[i].offset;
        module.dynsym_count_ = sections[i].size / sizeof(Sym);
        module.dynstr_offset_ = sections[sections[i].link].offset;
        module.dynstr_size_ = sections[sections[i].link].size;
    }
    if (dynsym == 0) {
        error = "no dynamic symbol table";
        return std::nullopt;
    }
    const Shdr *names = header.shstrndx < sections.size() ? &sections[header.shstrndx] : nullptr;
    for (const Shdr &section : sections) {
        if (section.type == kShtRela && (section.flags & kShfAlloc) != 0 && section.link == dynsym)
            module.relas_.push_back({section.offset, section.size / sizeof(Rela)});
        if (names == nullptr || c_string(file, names->offset, names->size, section.name) != ".eh_frame" || section.size == 0)
            continue;
        if (!frames_end_marked(file, section)) {
            error = "the module's .eh_frame has no end marker (module_eh_end.S must be linked last)";
            return std::nullopt;
        }
        module.eh_frame_ = std::pair{section.addr, section.size};
    }
    module.file_ = std::move(file);
    return module;
}

void ElfModule::copy_into(std::span<std::uint8_t> image) const {
    for (const Load &load : loads_) {
        if (load.vaddr + load.file_size > image.size()) continue;
        std::memcpy(image.data() + load.vaddr, file_.data() + load.file_offset, load.file_size);
    }
}

bool ElfModule::relocate(std::span<std::uint8_t> image, std::uint64_t load_address, std::span<const ModuleImport> imports,
                         RelocationStats &stats, std::string &error) const {
    std::unordered_map<std::string_view, std::uint64_t> by_name;
    for (const ModuleImport &import : imports) by_name.emplace(import.name, import.address);
    for (const RelaTable &table : relas_) {
        for (std::uint64_t i = 0; i < table.count; ++i) {
            Rela rela{};
            if (!read(file_, table.offset + i * sizeof(Rela), rela)) {
                error = "truncated relocation table";
                return false;
            }
            const auto type = static_cast<std::uint32_t>(rela.info);
            const auto symbol = static_cast<std::uint32_t>(rela.info >> 32);
            if (type == kRNone) continue;
            if (rela.offset + sizeof(std::uint64_t) > image.size()) {
                error = "a relocation lies outside the module";
                return false;
            }
            std::uint64_t value = 0;
            if (type == kRRelative) {
                value = load_address + static_cast<std::uint64_t>(rela.addend);
                ++stats.relative;
            } else if (type == kRAbs64 || type == kRGlobDat || type == kRJumpSlot) {
                Sym sym{};
                if (symbol >= dynsym_count_ || !read(file_, dynsym_offset_ + std::uint64_t{symbol} * sizeof(Sym), sym)) {
                    error = "a relocation names a symbol that does not exist";
                    return false;
                }
                const std::string_view name = c_string(file_, dynstr_offset_, dynstr_size_, sym.name);
                std::uint64_t target = 0;
                if (sym.shndx != 0) {
                    target = load_address + sym.value; // defined in the module itself
                } else if (const auto found = by_name.find(name); found != by_name.end()) {
                    target = found->second;
                } else if ((sym.info >> 4) != kStbWeak) {
                    error = "missing import: " + std::string(name);
                    return false;
                }
                value = target + static_cast<std::uint64_t>(rela.addend);
                ++stats.symbolic;
            } else {
                error = "unsupported relocation type " + std::to_string(type);
                return false;
            }
            std::memcpy(image.data() + rela.offset, &value, sizeof value);
        }
    }
    return true;
}

std::optional<std::uint64_t> ElfModule::symbol_offset(std::string_view name) const {
    for (std::uint64_t i = 1; i < dynsym_count_; ++i) {
        Sym sym{};
        if (!read(file_, dynsym_offset_ + i * sizeof(Sym), sym)) break;
        if (sym.shndx != 0 && c_string(file_, dynstr_offset_, dynstr_size_, sym.name) == name) return sym.value;
    }
    return std::nullopt;
}

} // namespace vcsnx
