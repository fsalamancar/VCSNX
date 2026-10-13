#pragma once
// Loads a shared object into code memory on the console (spec §2.4): copies and relocates it with
// ElfModule, maps it with svcMapProcessCodeMemory, sets RX/RW per page with svcSetProcessMemoryPermission,
// flushes the caches and registers its .eh_frame so exceptions cross it. Adapted from
// so_util.c (MIT, (C) 2021 Andy Nguyen, fgsfds; notice in builder/licenses/so_util-LICENSE.txt).

#include "elf_module.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace vcsnx {

// The table tools/make_module_imports.py writes into the runtime, ended by {nullptr, 0}.
struct RawImport {
    const char *name;
    std::uint64_t address;
};

[[nodiscard]] inline std::vector<ModuleImport> module_imports(const RawImport *table) {
    std::vector<ModuleImport> imports;
    for (; table->name != nullptr; ++table) imports.push_back({table->name, table->address});
    return imports;
}

struct LoadedModule {
    std::uint64_t base{};
    std::uint64_t size{};
    std::vector<void *> symbols; // the requested exports, in order
};

#if defined(__SWITCH__)
[[nodiscard]] std::optional<LoadedModule> load_module(const std::string &path, std::span<const ModuleImport> imports,
                                                      std::span<const std::string_view> exports, std::string &error);
#endif

// game_module_bridge.cpp: where main.cpp's register_generated_functions call goes on the Switch.
void set_game_module_entry(void *entry) noexcept;

} // namespace vcsnx
