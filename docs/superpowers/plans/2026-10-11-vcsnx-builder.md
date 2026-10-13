# VCSNX v1.0.0 (runtime publicable + `vcs_game.so` + VCSNX Builder) — plan de implementación

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Publicar un `vcsnx.nro` sin nada del juego y un VCSNX Builder para Windows que crea `vcs_game.so` (el código traducido del juego) en el PC de cada jugador, desde su propia copia de ULUS-10160.

**Architecture:** En la Switch, el runtime carga `vcs_game.so` con un cargador adaptado del de `gtalcs_nx` (MIT): parte pura (`elf_module`, probada en el Mac) + mapeo de memoria de código (`game_module`, solo Switch); el `main.cpp` del vendor no cambia porque `register_generated_functions` pasa por un puente. CMake parte el build de la Switch en runtime + módulo. El Builder (`Builder.Core` .NET 10 probado en el Mac, `Builder.Cli`, `Builder.App` WinForms) compila los 235 archivos traducidos con el toolchain oficial de devkitPro para Windows y enlaza el módulo con un kit pequeño.

**Tech Stack:** C++20 / libnx (cargador), CMake, Python 3 (scripts de `tools/`, unittest), C# / .NET 10 (WinForms, xUnit, SkiaSharp), devkitA64 GCC 16.1.0, Wine 11.

**Spec:** `docs/superpowers/specs/2026-10-11-vcsnx-builder-design.md`

## Global Constraints

- Nunca entra en el repo ni en el release: `generated/`, `vcs_game.so`, `PSP_DATA`, `local/`, ni un archivo cuyo SHA-256 esté en `builder/hashes/`. `vcsnx.nro` se publica solo si el `vcsnx.elf` del runtime no define ningún símbolo del código traducido (Task 4).
- Toolchain del `.zip` (paquetes oficiales sin modificar): `devkita64-gcc-16.1.0-1-x86_64`, `devkita64-binutils-2.46.0-1-x86_64` (de `https://pkg.devkitpro.org/packages/windows/x86_64/`) y `devkita64-newlib-4.6.0.20260123-4-any` (de `https://pkg.devkitpro.org/packages/`), con los SHA-256 de la Task 1.
- Código traducido: `https://codeload.github.com/jessicanataliagta/PSPRecomp/zip/f6e7d415c7f447b934cc3865a31eb725f353d659`, carpeta `PSPRecomp-f6e7d415c7f447b934cc3865a31eb725f353d659/profiles/vcs/generated/`, 237 archivos.
- Ejecutables válidos (SHA-256): `85c5e172fa0207086b72a37649e9a10d47845f0866b0dee179c48d930e43d17f` (EBOOT desencriptado) y `fee2e86c7fe457ab6da463d9fdc16f156a0900adcdaa2fd260206c11907c5d65` (`PSP_GAME/SYSDIR/BOOT.BIN`).
- Módulo: `switch/vcsnx/vcs_game.so`, ELF compartido AArch64 enlazado con `-shared -nostdlib -Wl,-Bsymbolic` + `-lgcc`; exporta `vcsnx_register_generated_functions(psprecomp::Runtime*)` y `vcsnx_module_abi`.
- Huella de interfaz: SHA-256 de los headers de `vendor/` de los que depende el código traducido (`tools/module_abi.py`); la misma en el kit, el módulo y el runtime.
- El cargador se adapta de `gtalcs_nx/source/so_util.c` (MIT, © 2021 Andy Nguyen y fgsfds) con crédito en el archivo y en `THIRD_PARTY.md`.
- Mensajes de la consola: solo ASCII (la consola de libnx no muestra otra cosa).
- La ventana dice solo **`bAlo`** en el encabezado. Idioma: español si `CurrentUICulture` es `es-*`, si no inglés.
- Carpetas junto al `.exe`: `toolchain\`, `kit\`, `hashes\`, `trabajo\` (objetos en `trabajo\g\`) y `salida\`. La carpeta del programa debe ser solo ASCII (las herramientas de Windows de devkitPro no aceptan otras rutas); los espacios están permitidos.
- El build del Mac no cambia (enlaza el código traducido dentro del ejecutable).
- Ruling respecto del spec: .NET 10 (LTS) en lugar de .NET 8 (soporte hasta el 2026-11-10).
- Wine: `/Applications/Wine Stable.app/Contents/Resources/wine/bin/wine`, `WINEPREFIX=$HOME/.local/share/wineprefixes/vcsnx`, `WINEDEBUG=-all`, `WINEDLLOVERRIDES="mscoree,mshtml="`.
- Subir a la Switch solo con "ftp listo" del usuario, una conexión a la vez, desde copias estables en el scratchpad.
- Rama `feat/builder`; commits Conventional Commits terminados en `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`; nunca se sube `main`.

## Review Focus

1. La carpeta del programa con acentos (`C:\Users\José\Downloads\VCSNX-Builder\`): mensaje claro antes de compilar; con espacios funciona. Pruebas en la Task 8 (`Toolchain`) y la Task 10 (`Pipeline`).
2. El jugador elige la carpeta que contiene `PSP_DATA`, o `PSP_GAME`: encuentra el juego igual. Prueba en la Task 7.
3. Se cierra a mitad de la compilación: un objeto a medio escribir nunca se reutiliza. Prueba en la Task 9.
4. Un `vcs_game.so` viejo con un `vcsnx.nro` nuevo de otra interfaz: mensaje en la consola, nunca un cuelgue. Pruebas en la Task 4 (`startup_rules`) y la Task 2 (`relocate` con un símbolo que falta).
5. Repetir solo con otro ícono o SD: no vuelve a bajar, compilar ni enlazar. Prueba en la Task 10.

---

## Estructura de archivos

```
.gitignore                                   + builder/**/bin/, builder/**/obj/, /dist/, vcs_game.so
platform/switch/elf_module.{hpp,cpp}         lee un .so AArch64, lo copia y lo reubica en un buffer (pura)
platform/switch/game_module.{hpp,cpp}        mapea el módulo como código en la Switch (solo Switch)
platform/switch/game_module_bridge.cpp       register_generated_functions → el módulo (runtime de Switch)
platform/switch/module_entry.cpp             entradas del módulo (va dentro de vcs_game.so)
platform/switch/module_eh_end.S              fin de .eh_frame del módulo
platform/switch/startup_rules.{hpp,cpp}      + mensajes del módulo
platform/switch/nx_startup.cpp               + carga el módulo antes de main
tests/fixtures/fixture_module.c              módulo de prueba para elf_module
tests/platform/test_elf_module.cpp
tools/modprobe/modprobe.cpp, modtest.cpp     prueba del cargador en la consola
cmake/vcsnx_modprobe.cmake, cmake/vcsnx_game_module.cmake
tools/fetch_windows_toolchain.py, smoke_windows_toolchain.sh, compare_elf_code.sh
tools/module_abi.py, make_module_imports.py, check_runtime_clean.sh
tools/make_builder_hashes.py, make_builder_kit.py, package_builder.py (+ tests en tools/tests/)
builder/ (solución .NET: Builder.Core, Builder.Core.Tests, Builder.Cli, Builder.App; hashes/, licenses/,
          README.txt, RELEASE_NOTES.md)
README.md                                    sección "Easy way"
```

---

### Task 1: Toolchain de Windows y prueba con Wine (B0)

**Files:**
- Create: `tools/fetch_windows_toolchain.py`, `tools/tests/test_fetch_windows_toolchain.py`, `tools/smoke_windows_toolchain.sh`, `tools/compare_elf_code.sh`
- Modify: `.gitignore`

**Interfaces:**
- Produces: `python3 -I tools/fetch_windows_toolchain.py <dir>` deja en `<dir>` el árbol `opt/devkitpro` (`devkitA64/bin/aarch64-none-elf-g++.exe`, `devkitA64/aarch64-none-elf/include/math.h`, ...). `verify(path: Path, sha256: str) -> bool`, `PACKAGES: list[tuple[str, str, str]]`.
- Produces: `tools/compare_elf_code.sh <a> <b> [sections...]` (por defecto `.text .rodata .data .data.rel.ro`).

- [ ] **Step 1: Instalar Wine y .NET**

Run:
```bash
brew reinstall --cask wine-stable && brew install dotnet
"/Applications/Wine Stable.app/Contents/Resources/wine/bin/wine" --version; dotnet --version
```
Expected: `wine-11.0` y `10.0.4xx`. Si macOS dice que Wine "está dañado" o lo cierra, `xattr -dr com.apple.quarantine "/Applications/Wine Stable.app"` y repetir (Ruling en el ledger).

- [ ] **Step 2: Escribir el test que falla**

```python
"""Tests for tools/fetch_windows_toolchain.py (stdlib unittest). Run: python3 -I tools/tests/test_fetch_windows_toolchain.py"""
import hashlib
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from fetch_windows_toolchain import PACKAGES, verify  # noqa: E402


class FetchWindowsToolchainTest(unittest.TestCase):
    def test_verify_accepts_the_right_hash(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "pkg"
            path.write_bytes(b"package")
            self.assertTrue(verify(path, hashlib.sha256(b"package").hexdigest()))

    def test_verify_rejects_a_wrong_hash(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "pkg"
            path.write_bytes(b"tampered")
            self.assertFalse(verify(path, hashlib.sha256(b"package").hexdigest()))

    def test_only_what_building_the_module_needs_is_pinned(self) -> None:
        names = [name for _, name, _ in PACKAGES]
        self.assertEqual(names, ["devkita64-gcc-16.1.0-1-x86_64.pkg.tar.zst",
                                 "devkita64-binutils-2.46.0-1-x86_64.pkg.tar.zst",
                                 "devkita64-newlib-4.6.0.20260123-4-any.pkg.tar.zst"])
        for _, _, sha in PACKAGES:
            self.assertRegex(sha, r"^[0-9a-f]{64}$")


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 3: Correrlo y ver que falla**

Run: `python3 -I tools/tests/test_fetch_windows_toolchain.py`
Expected: `ModuleNotFoundError: No module named 'fetch_windows_toolchain'`

- [ ] **Step 4: Implementar el script**

```python
"""Downloads devkitPro's official Windows toolchain for the VCSNX Builder, pinned and verified: what
compiling and linking vcs_game.so needs (GCC, binutils, and newlib for the C headers <cmath> includes),
at the versions VCSNX is built with on the Mac (GCC 16.1.0). Every package is checked against the
SHA-256 of devkitPro's package database before it is unpacked, unmodified.

Usage: python3 -I tools/fetch_windows_toolchain.py <output folder>
The output folder gets the packages' opt/devkitpro tree (devkitA64/).
"""
import hashlib
import shutil
import subprocess
import sys
import tempfile
import urllib.request
from pathlib import Path

WINDOWS = "https://pkg.devkitpro.org/packages/windows/x86_64/"
LIBS = "https://pkg.devkitpro.org/packages/"
PACKAGES = [
    (WINDOWS, "devkita64-gcc-16.1.0-1-x86_64.pkg.tar.zst", "4ccf7ee6f3d85248f3b58708e44aa3e9f777479ff90cd4409a95862752e85400"),
    (WINDOWS, "devkita64-binutils-2.46.0-1-x86_64.pkg.tar.zst", "c516a70a31521b61e18759669c6f5c6718796a6cbb0b3c1c023a2ac418f79988"),
    (LIBS, "devkita64-newlib-4.6.0.20260123-4-any.pkg.tar.zst", "fc97ba1009a94b68f8714c3cc548ef697ca82687b72fe0c05a7ca3df81beb53e"),
]
USER_AGENT = "pacman/6.0"  # devkitPro's server answers package downloads made the way pacman makes them


def verify(path: Path, sha256: str) -> bool:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest() == sha256


def download(url: str, destination: Path) -> None:
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request) as response, destination.open("wb") as out:
        shutil.copyfileobj(response, out)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: fetch_windows_toolchain.py <output folder>", file=sys.stderr)
        return 2
    out = Path(sys.argv[1]).resolve()
    out.mkdir(parents=True, exist_ok=True)
    cache = out.parent / "packages"
    cache.mkdir(exist_ok=True)
    for base, name, sha in PACKAGES:
        package = cache / name
        if not package.exists() or not verify(package, sha):
            download(base + name, package)
        if not verify(package, sha):
            print(f"{name}: SHA-256 does not match devkitPro's database", file=sys.stderr)
            return 3
        with tempfile.TemporaryDirectory() as staging:
            subprocess.run(["tar", "--zstd", "-xf", str(package), "-C", staging], check=True)
            shutil.copytree(Path(staging) / "opt" / "devkitpro", out, dirs_exist_ok=True)
        print(f"{name}: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 5: Correr los tests y bajar el toolchain**

Run: `python3 -I tools/tests/test_fetch_windows_toolchain.py && python3 -I tools/fetch_windows_toolchain.py build/builder/toolchain-win && ls build/builder/toolchain-win/devkitA64/bin/aarch64-none-elf-g++.exe build/builder/toolchain-win/devkitA64/aarch64-none-elf/include/math.h`
Expected: `Ran 3 tests ... OK`, 3 líneas `...: ok` y los dos archivos listados.

- [ ] **Step 6: Escribir la comparación de código y la prueba con Wine**

`tools/compare_elf_code.sh`:
```bash
#!/usr/bin/env bash
# Compares the code and data two AArch64 ELFs load (symbol tables and build ids may differ when only file
# names did). Usage: tools/compare_elf_code.sh <a> <b> [sections...] (default .text .rodata .data .data.rel.ro)
set -euo pipefail
OBJCOPY=/opt/devkitpro/devkitA64/bin/aarch64-none-elf-objcopy
A="$1"; B="$2"; shift 2
SECTIONS=("$@"); [ ${#SECTIONS[@]} -gt 0 ] || SECTIONS=(.text .rodata .data .data.rel.ro)
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
status=0
for section in "${SECTIONS[@]}"; do
    "$OBJCOPY" -O binary --only-section="$section" "$A" "$WORK/a"
    "$OBJCOPY" -O binary --only-section="$section" "$B" "$WORK/b"
    if cmp -s "$WORK/a" "$WORK/b"; then echo "$section identical ($(wc -c < "$WORK/a" | tr -d ' ') bytes)"
    else echo "$section DIFFERS" >&2; status=1; fi
done
exit $status
```

`tools/smoke_windows_toolchain.sh`:
```bash
#!/usr/bin/env bash
# Builds a small shared module, the way the Builder builds vcs_game.so, with devkitPro's Windows toolchain
# under Wine, and checks it carries the same code as the Mac toolchain's. Proves the Windows compiler,
# its C/C++ headers and its linker run on their own (no MSYS2).
# Usage: tools/smoke_windows_toolchain.sh <toolchain from fetch_windows_toolchain.py>
set -euo pipefail
TC="$(cd "${1:?usage: smoke_windows_toolchain.sh <toolchain folder>}" && pwd)"
WINE="${WINE:-/Applications/Wine Stable.app/Contents/Resources/wine/bin/wine}"
export WINEPREFIX="${WINEPREFIX:-$HOME/.local/share/wineprefixes/vcsnx}" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml="
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cat > "$WORK/smoke.cpp" <<'EOF'
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
int host_add(int a, int b);
extern "C" int smoke_entry(int x) { return host_add(x, std::numeric_limits<std::uint8_t>::max()); }
extern "C" float smoke_math(float v) { return std::sqrt(v) + std::bit_cast<float>(std::uint32_t{0x3f800000u}); }
EOF
FLAGS=(-march=armv8-a+crc+crypto -mtune=cortex-a57 -fPIC -O2 -std=c++20 -D__SWITCH__)
LINK=(-shared -nostdlib -Wl,-Bsymbolic)
win() { echo "Z:$1"; }   # Wine maps the Mac's / to Z:
"$WINE" "$TC/devkitA64/bin/aarch64-none-elf-g++.exe" "${FLAGS[@]}" -c "$(win "$WORK/smoke.cpp")" -o "$(win "$WORK/win.o")"
"$WINE" "$TC/devkitA64/bin/aarch64-none-elf-g++.exe" "${FLAGS[@]}" "${LINK[@]}" "$(win "$WORK/win.o")" -lgcc -o "$(win "$WORK/win.so")"
MAC=/opt/devkitpro/devkitA64/bin
"$MAC/aarch64-none-elf-g++" "${FLAGS[@]}" -c "$WORK/smoke.cpp" -o "$WORK/mac.o"
"$MAC/aarch64-none-elf-g++" "${FLAGS[@]}" "${LINK[@]}" "$WORK/mac.o" -lgcc -o "$WORK/mac.so"
"$(dirname "$0")/compare_elf_code.sh" "$WORK/win.so" "$WORK/mac.so" .text .rodata
echo "smoke ok: the Windows toolchain runs under Wine and builds the same module as the Mac's"
```

Run: `chmod +x tools/compare_elf_code.sh tools/smoke_windows_toolchain.sh && tools/smoke_windows_toolchain.sh build/builder/toolchain-win`
Expected: `.text identical`, `.rodata identical` y `smoke ok: ...`. Si un `.exe` pide una DLL que falta, anotarla y detener la tarea (spec §8: volver a hablar con el usuario).

- [ ] **Step 7: Ignorar las salidas y hacer commit**

Agregar al final de `.gitignore`:
```
# VCSNX Builder: .NET outputs, the release zip, and the game module (built from the player's copy: never commit)
builder/**/bin/
builder/**/obj/
/dist/
vcs_game.so
```

```bash
git add .gitignore tools/fetch_windows_toolchain.py tools/tests/test_fetch_windows_toolchain.py tools/smoke_windows_toolchain.sh tools/compare_elf_code.sh
git commit -m "feat(tools): fetch devkitPro's Windows toolchain and smoke-test it under Wine

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: El cargador, parte pura (B1)

**Files:**
- Create: `platform/switch/elf_module.hpp`, `platform/switch/elf_module.cpp`
- Create: `tests/fixtures/fixture_module.c`, `tests/platform/test_elf_module.cpp`
- Modify: `cmake/vcsnx_tests.cmake`

**Interfaces:**
- Produces:
```cpp
namespace vcsnx {
struct ModuleImport { std::string_view name; std::uint64_t address; };
struct ModuleSegment { std::uint64_t offset; std::uint64_t size; bool executable; bool writable; };
struct RelocationStats { std::size_t relative{}; std::size_t symbolic{}; };
class ElfModule {
public:
    static std::optional<ElfModule> parse(std::vector<std::uint8_t> file, std::string &error);
    std::uint64_t image_size() const noexcept;                       // multiple of 0x1000
    const std::vector<ModuleSegment> &segments() const noexcept;     // page-aligned
    void copy_into(std::span<std::uint8_t> image) const;             // image zero-filled, >= image_size()
    bool relocate(std::span<std::uint8_t> image, std::uint64_t load_address, std::span<const ModuleImport> imports,
                  RelocationStats &stats, std::string &error) const;
    std::optional<std::uint64_t> symbol_offset(std::string_view name) const;          // defined dynamic symbol
    std::optional<std::pair<std::uint64_t, std::uint64_t>> eh_frame() const;          // offset, size
};
}
```

- [ ] **Step 1: Escribir el módulo de prueba, el test y la interfaz con stub**

`tests/fixtures/fixture_module.c`:
```c
// Test module for platform/switch/elf_module: the relocation kinds vcs_game.so uses, built by the test
// build with devkitA64 (-shared -nostdlib -Wl,-Bsymbolic).
extern int fixture_import(int);                              // resolved by the loader (JUMP_SLOT / GLOB_DAT)
extern int fixture_weak_missing(int) __attribute__((weak));  // stays 0
int fixture_counter = 7;
int *fixture_counter_address = &fixture_counter;             // R_AARCH64_RELATIVE
int (*fixture_import_address)(int) = &fixture_import;        // symbolic, to the import
int fixture_call(int x) { return fixture_import(x) + 1; }
int fixture_has_weak(void) { return fixture_weak_missing != 0; }
```

`tests/platform/test_elf_module.cpp`:
```cpp
// The game module loader's pure part: a devkitA64 shared object copied and relocated into a buffer.
#include "elf_module.hpp"
#include "test_support.hpp"

#include <cstring>
#include <fstream>
#include <iterator>

namespace {

std::vector<std::uint8_t> fixture() {
    std::ifstream file(VCSNX_FIXTURE_MODULE, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

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
    });
}
```

Agregar al final de `cmake/vcsnx_tests.cmake`:
```cmake
# The game module loader's pure part (platform/switch/elf_module) against a devkitA64 shared object.
find_program(VCSNX_A64_GCC aarch64-none-elf-gcc HINTS /opt/devkitpro/devkitA64/bin)
if(VCSNX_A64_GCC)
    set(VCSNX_FIXTURE_MODULE "${CMAKE_BINARY_DIR}/fixture_module.so")
    add_custom_command(OUTPUT "${VCSNX_FIXTURE_MODULE}"
        COMMAND "${VCSNX_A64_GCC}" -O2 -fPIC -shared -nostdlib -Wl,-Bsymbolic
                "${CMAKE_SOURCE_DIR}/tests/fixtures/fixture_module.c" -o "${VCSNX_FIXTURE_MODULE}"
        DEPENDS "${CMAKE_SOURCE_DIR}/tests/fixtures/fixture_module.c" VERBATIM)
    add_custom_target(fixture_module DEPENDS "${VCSNX_FIXTURE_MODULE}")
    vcsnx_add_test(test_elf_module tests/platform/test_elf_module.cpp platform/switch/elf_module.cpp)
    add_dependencies(test_elf_module fixture_module)
    target_compile_definitions(test_elf_module PRIVATE VCSNX_FIXTURE_MODULE="${VCSNX_FIXTURE_MODULE}")
else()
    message(STATUS "VCSNX: no devkitA64, test_elf_module skipped")
endif()
```

`platform/switch/elf_module.hpp`:
```cpp
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
```

`platform/switch/elf_module.cpp` (stub, se reemplaza en el Step 3):
```cpp
#include "elf_module.hpp"

namespace vcsnx {

std::optional<ElfModule> ElfModule::parse(std::vector<std::uint8_t>, std::string &error) {
    error = "not implemented";
    return std::nullopt;
}
void ElfModule::copy_into(std::span<std::uint8_t>) const {}
bool ElfModule::relocate(std::span<std::uint8_t>, std::uint64_t, std::span<const ModuleImport>, RelocationStats &, std::string &) const { return false; }
std::optional<std::uint64_t> ElfModule::symbol_offset(std::string_view) const { return std::nullopt; }

} // namespace vcsnx
```

- [ ] **Step 2: Correrlo y ver que falla**

Run: `cmake build/mac && cmake --build build/mac --target test_elf_module && ./build/mac/bin/test_elf_module`
Expected: `FAIL a devkitA64 shared object parses into page-aligned segments: parse: not implemented` (y los demás, salvo "something that is not..." que pasa).

- [ ] **Step 3: Implementar**

`platform/switch/elf_module.cpp`:
```cpp
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
    return {start, strnlen(start, table_size - index)};
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
        if (names != nullptr && c_string(file, names->offset, names->size, section.name) == ".eh_frame" && section.size > 0)
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
```

- [ ] **Step 4: Correr los tests**

Run: `cmake --build build/mac --target test_elf_module && ./build/mac/bin/test_elf_module && (cd build/mac && ctest 2>&1 | tail -3)`
Expected: 5 `ok` y `100% tests passed out of 30`.

- [ ] **Step 5: Commit**

```bash
git add platform/switch/elf_module.hpp platform/switch/elf_module.cpp tests/fixtures/fixture_module.c tests/platform/test_elf_module.cpp cmake/vcsnx_tests.cmake
git commit -m "feat(platform): relocate an AArch64 shared object into an image (game module loader, pure part)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: El cargador en la consola y `modprobe` (B1)

**Files:**
- Create: `platform/switch/game_module.hpp`, `platform/switch/game_module.cpp`
- Create: `tools/make_module_imports.py`, `tools/tests/test_make_module_imports.py`
- Create: `tools/modprobe/modprobe.cpp`, `tools/modprobe/modtest.cpp`, `cmake/vcsnx_modprobe.cmake`
- Modify: `CMakeLists.txt` (incluir `cmake/vcsnx_modprobe.cmake` junto a `cmake/vcsnx_vkprobe.cmake`)

**Interfaces:**
- Consumes: `ElfModule`, `ModuleImport`, `RelocationStats` (Task 2).
- Produces:
```cpp
namespace vcsnx {
struct RawImport { const char *name; std::uint64_t address; };     // the .S table, ends with {nullptr, 0}
std::vector<ModuleImport> module_imports(const RawImport *table);
struct LoadedModule { std::uint64_t base{}; std::uint64_t size{}; std::vector<void *> symbols; };
std::optional<LoadedModule> load_module(const std::string &path, std::span<const ModuleImport> imports,
                                        std::span<const std::string_view> exports, std::string &error);  // Switch
void set_game_module_entry(void *entry) noexcept;                   // defined in Task 4
}
```
- Produces: `python3 -I tools/make_module_imports.py <nm> <module.so> <out.S>` (símbolo `vcsnx_module_imports`); `undefined_symbols(nm_output: str) -> list[str]`, `assembly(names: list[str], source: str) -> str`.

- [ ] **Step 1: Test del generador de la tabla (falla)**

```python
"""Tests for tools/make_module_imports.py (stdlib unittest). Run: python3 -I tools/tests/test_make_module_imports.py"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from make_module_imports import assembly, undefined_symbols  # noqa: E402

NM = """
                 U _ZN9psprecomp11GuestMemory15aot_store8_slowEjh
                 U memcpy
                 w fixture_weak_missing
0000000000001000 T vcsnx_register_generated_functions
                 U memcpy
"""


class MakeModuleImportsTest(unittest.TestCase):
    def test_only_strong_undefined_symbols_once_each(self) -> None:
        self.assertEqual(undefined_symbols(NM), ["_ZN9psprecomp11GuestMemory15aot_store8_slowEjh", "memcpy"])

    def test_the_table_pairs_names_and_addresses_and_ends_with_zeros(self) -> None:
        text = assembly(["memcpy", "sinf"], "vcs_game.so")
        self.assertIn(".global vcsnx_module_imports", text)
        self.assertIn(".quad .Lvcsnx_import_0, memcpy", text)
        self.assertIn(".quad .Lvcsnx_import_1, sinf", text)
        self.assertIn('.Lvcsnx_import_1: .asciz "sinf"', text)
        self.assertLess(text.index(".quad .Lvcsnx_import_1, sinf"), text.index(".quad 0, 0"))


if __name__ == "__main__":
    unittest.main()
```

Run: `python3 -I tools/tests/test_make_module_imports.py`
Expected: `ModuleNotFoundError: No module named 'make_module_imports'`

- [ ] **Step 2: Implementar el generador**

```python
"""Writes the import table vcsnx.nro hands the game-module loader (platform/switch/game_module.hpp): every
symbol the module leaves undefined, paired with its address in the runtime. It holds only names of the
runtime's and of the C/C++ libraries' functions, nothing of the game; referencing them also makes the
linker keep those functions in the runtime.
Usage: python3 -I tools/make_module_imports.py <aarch64-none-elf-nm> <module.so> <output .S>
"""
import subprocess
import sys
from pathlib import Path


def undefined_symbols(nm_output: str) -> list[str]:
    names = set()
    for line in nm_output.splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[0] == "U":
            names.add(parts[1])
    return sorted(names)


def assembly(names: list[str], source: str) -> str:
    lines = [
        f"// Generated by tools/make_module_imports.py from {source}: the runtime symbols the module calls.",
        '    .section .data.rel.ro.vcsnx_module_imports,"aw"',
        "    .balign 8",
        "    .global vcsnx_module_imports",
        "vcsnx_module_imports:",
    ]
    lines += [f"    .quad .Lvcsnx_import_{i}, {name}" for i, name in enumerate(names)]
    lines += ["    .quad 0, 0", '    .section .rodata.vcsnx_module_import_names,"a"']
    lines += [f'.Lvcsnx_import_{i}: .asciz "{name}"' for i, name in enumerate(names)]
    return "\n".join(lines) + "\n"


def main() -> int:
    if len(sys.argv) != 4:
        print("usage: make_module_imports.py <nm> <module.so> <output .S>", file=sys.stderr)
        return 2
    nm, module, out = sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3])
    output = subprocess.run([nm, "-D", "--undefined-only", str(module)], capture_output=True, text=True, check=True).stdout
    names = undefined_symbols(output)
    if not names:
        print(f"{module} imports nothing: is it a shared object?", file=sys.stderr)
        return 3
    out.write_text(assembly(names, module.name))
    print(f"{module.name}: {len(names)} imports")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

Run: `python3 -I tools/tests/test_make_module_imports.py`
Expected: `Ran 2 tests ... OK`

- [ ] **Step 3: El cargador de la Switch**

`platform/switch/game_module.hpp`:
```cpp
#pragma once
// Loads a shared object into code memory on the console (spec §2.4): copies and relocates it with
// ElfModule, maps it with svcMapProcessCodeMemory, sets RX/RW per page with svcSetProcessMemoryPermission,
// flushes the caches and registers its .eh_frame so exceptions cross it. Adapted from
// gtalcs_nx/source/so_util.c (MIT, (C) 2021 Andy Nguyen, fgsfds).

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
```

`platform/switch/game_module.cpp`:
```cpp
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
    // The kernel forbids W->X on code memory: RX pages first, then the rest RW (as gtalcs_nx's so_finalize).
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
```

- [ ] **Step 4: `modprobe` y su módulo de prueba**

`tools/modprobe/modtest.cpp`:
```cpp
// modprobe's test module (built -shared -nostdlib -Wl,-Bsymbolic, like vcs_game.so): calls into the
// runtime, and exceptions both ways across the boundary, with a destructor run while unwinding.
#include <stdexcept>

int modprobe_host_add(int a, int b);  // defined in modprobe.nro
void modprobe_host_throw(int code);   // defined in modprobe.nro: throws std::runtime_error

namespace {
struct Cleanup {
    int &count;
    ~Cleanup() { ++count; }
};
} // namespace

extern "C" __attribute__((visibility("default"))) int modtest_add(int a, int b) { return modprobe_host_add(a, b) * 2; }

extern "C" __attribute__((visibility("default"))) int modtest_catch_host_throw() {
    int cleanups = 0;
    try {
        Cleanup cleanup{cleanups};
        modprobe_host_throw(7);
    } catch (const std::runtime_error &) {
        return 100 + cleanups;
    }
    return -1;
}

extern "C" __attribute__((visibility("default"))) void modtest_throw() { throw std::runtime_error("from the module"); }

extern "C" __attribute__((visibility("default"))) const char modtest_abi[] = "modprobe-1";
```

`tools/modprobe/modprobe.cpp`:
```cpp
// modprobe.nro: proves on the console that the game-module loader works before the game depends on it
// (spec §6.2): loads sdmc:/switch/vcsnx/modtest.so, calls it, and throws both ways across it. Prints PASS
// or FAIL per check and writes sdmc:/switch/vcsnx/modprobe.log.
#include "game_module.hpp"

#include <switch.h>

#include <array>
#include <cstdarg>
#include <cstdio>
#include <stdexcept>
#include <string>

extern "C" const vcsnx::RawImport vcsnx_module_imports[];

int modprobe_host_add(int a, int b) { return a + b; }
void modprobe_host_throw(int code) { throw std::runtime_error("from the runtime " + std::to_string(code)); }

namespace {
std::FILE *g_log = nullptr;
int g_failures = 0;

void say(const char *format, ...) {
    va_list args;
    va_start(args, format);
    char line[512];
    std::vsnprintf(line, sizeof line, format, args);
    va_end(args);
    std::printf("%s\n", line);
    if (g_log != nullptr) std::fprintf(g_log, "%s\n", line);
    consoleUpdate(nullptr);
}

void check(bool ok, const std::string &what) {
    say("%s %s", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++g_failures;
}
} // namespace

int main() {
    consoleInit(nullptr);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    g_log = std::fopen("sdmc:/switch/vcsnx/modprobe.log", "w");
    say("VCSNX module probe");
    std::string error;
    constexpr std::array<std::string_view, 4> exports{"modtest_add", "modtest_catch_host_throw", "modtest_throw", "modtest_abi"};
    const auto imports = vcsnx::module_imports(vcsnx_module_imports);
    const auto module = vcsnx::load_module("sdmc:/switch/vcsnx/modtest.so", imports, exports, error);
    check(module.has_value(), module ? "load modtest.so" : "load modtest.so: " + error);
    if (module) {
        say("base 0x%llx, %llu KB, %zu imports", static_cast<unsigned long long>(module->base),
            static_cast<unsigned long long>(module->size >> 10), imports.size());
        check(reinterpret_cast<int (*)(int, int)>(module->symbols[0])(2, 3) == 10, "module calls the runtime");
        check(reinterpret_cast<int (*)()>(module->symbols[1])() == 101, "runtime exception caught in the module, destructor run");
        bool caught = false;
        try {
            reinterpret_cast<void (*)()>(module->symbols[2])();
        } catch (const std::runtime_error &e) {
            caught = std::string(e.what()) == "from the module";
        }
        check(caught, "module exception caught in the runtime");
        check(std::string(static_cast<const char *>(module->symbols[3])) == "modprobe-1", "module data read");
    }
    if (g_failures == 0) say("ALL PASS");
    else say("%d FAILED", g_failures);
    if (g_log != nullptr) std::fclose(g_log);
    say("Press + to exit.");
    while (appletMainLoop()) {
        padUpdate(&pad);
        if ((padGetButtonsDown(&pad) & HidNpadButton_Plus) != 0u) break;
        consoleUpdate(nullptr);
    }
    consoleExit(nullptr);
    return 0;
}
```

`cmake/vcsnx_modprobe.cmake`:
```cmake
# modprobe.nro + modtest.so (v1, Task 3): the game-module loader on the console before the game uses it.
enable_language(ASM)
find_program(VCSNX_NM aarch64-none-elf-nm HINTS "$ENV{DEVKITPRO}/devkitA64/bin" /opt/devkitpro/devkitA64/bin REQUIRED)
set(VCSNX_MODTEST "${CMAKE_BINARY_DIR}/modtest.so")
add_custom_command(OUTPUT "${VCSNX_MODTEST}"
    COMMAND "${CMAKE_CXX_COMPILER}" -march=armv8-a+crc+crypto -mtune=cortex-a57 -fPIC -O2 -std=c++20
            -shared -nostdlib -Wl,-Bsymbolic "${CMAKE_SOURCE_DIR}/tools/modprobe/modtest.cpp" -lgcc -o "${VCSNX_MODTEST}"
    DEPENDS "${CMAKE_SOURCE_DIR}/tools/modprobe/modtest.cpp" VERBATIM)
set(VCSNX_MODPROBE_IMPORTS "${CMAKE_BINARY_DIR}/modprobe_imports.S")
add_custom_command(OUTPUT "${VCSNX_MODPROBE_IMPORTS}"
    COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/make_module_imports.py" "${VCSNX_NM}" "${VCSNX_MODTEST}" "${VCSNX_MODPROBE_IMPORTS}"
    DEPENDS "${VCSNX_MODTEST}" "${CMAKE_SOURCE_DIR}/tools/make_module_imports.py" VERBATIM)
add_executable(modprobe "${CMAKE_SOURCE_DIR}/tools/modprobe/modprobe.cpp" "${VCSNX_PLATFORM_DIR}/game_module.cpp"
    "${VCSNX_PLATFORM_DIR}/elf_module.cpp" "${VCSNX_MODPROBE_IMPORTS}")
target_include_directories(modprobe PRIVATE "${VCSNX_PLATFORM_DIR}")
nx_generate_nacp(modprobe.nacp NAME "VCSNX module probe" AUTHOR "VCSNX" VERSION "${PROJECT_VERSION}")
nx_create_nro(modprobe NACP modprobe.nacp)
```

En `CMakeLists.txt`, junto a `include(cmake/vcsnx_vkprobe.cmake)`, agregar `include(cmake/vcsnx_modprobe.cmake)`.

- [ ] **Step 5: Compilar**

Run: `export DEVKITPRO=/opt/devkitpro DEVKITA64=/opt/devkitpro/devkitA64 && cmake build/switch && cmake --build build/switch && ls -la build/switch/modprobe.nro build/switch/modtest.so && head -1 build/switch/modprobe_imports.S`
Expected: los dos archivos, y el `.S` empieza con `// Generated by tools/make_module_imports.py from modtest.so`.

- [ ] **Step 6: Probar en la consola**

Copiar `build/switch/modprobe.nro` y `build/switch/modtest.so` a una carpeta estable del scratchpad, pedir "ftp listo", subirlos a `switch/vcsnx/` y pedir al usuario que abra `modprobe.nro` desde hbmenu (manteniendo R al abrir un juego, como siempre) y diga qué ve. Después bajar `switch/vcsnx/modprobe.log`.
Expected: `PASS load modtest.so`, 4 `PASS` más y `ALL PASS`. Un `FAIL` detiene el plan: systematic-debugging, y si no hay arreglo, volver a hablar con el usuario (spec §8).

- [ ] **Step 7: Commit**

```bash
git add platform/switch/game_module.hpp platform/switch/game_module.cpp tools/make_module_imports.py tools/tests/test_make_module_imports.py tools/modprobe cmake/vcsnx_modprobe.cmake CMakeLists.txt
git commit -m "feat(platform): map a shared object as code on the console, proven with modprobe.nro

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Partir el build de la Switch (B2)

**Files:**
- Create: `tools/module_abi.py`, `tools/tests/test_module_abi.py`, `tools/check_runtime_clean.sh`
- Create: `platform/switch/module_entry.cpp`, `platform/switch/module_eh_end.S`, `platform/switch/game_module_bridge.cpp`, `cmake/vcsnx_game_module.cmake`
- Modify: `platform/switch/startup_rules.hpp`, `platform/switch/startup_rules.cpp`, `tests/platform/test_startup_rules.cpp`, `platform/switch/nx_startup.cpp`, `cmake/vcsnx_game.cmake`, `tools/prepare_sd.sh`

**Interfaces:**
- Consumes: `load_module`, `module_imports`, `RawImport`, `set_game_module_entry` (Task 3); `tools/make_module_imports.py` (Task 3).
- Produces: `python3 -I tools/module_abi.py <g++> <repo> [--list]`; `parse_depfile(text) -> list[str]`, `generated_dependencies(gxx, repo) -> list[Path]`, `abi_of(headers, repo) -> str`.
- Produces: `vcsnx::module_missing_message()`, `vcsnx::module_mismatch_message()`, `vcsnx::module_load_message(std::string_view)` (ASCII).
- Produces: build de Switch con `VCSNX_GAME_MODULE=ON`: `build/switch/vcsnx.nro` (runtime), `build/switch/vcs_game.so` (target `vcs_game_module`), objetos `CMakeFiles/vcsnx_module_entry.dir/...`, `VCSNX_MODULE_ABI:INTERNAL=<hex>` en `CMakeCache.txt`.

- [ ] **Step 1: Tests de la huella y de los mensajes (fallan)**

`tools/tests/test_module_abi.py`:
```python
"""Tests for tools/module_abi.py (stdlib unittest). Run: python3 -I tools/tests/test_module_abi.py"""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from module_abi import abi_of, parse_depfile  # noqa: E402


class ModuleAbiTest(unittest.TestCase):
    def test_parse_depfile_lists_every_dependency_once(self) -> None:
        text = "a.o: /r/generated/a.cpp /r/vendor/x.hpp \\\n /r/vendor/y.hpp\nb.o: /r/generated/b.cpp \\\n /r/vendor/x.hpp\n"
        self.assertEqual(parse_depfile(text), ["/r/generated/a.cpp", "/r/generated/b.cpp", "/r/vendor/x.hpp", "/r/vendor/y.hpp"])

    def test_the_fingerprint_follows_header_contents_not_their_order(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp)
            (repo / "vendor").mkdir()
            x, y = repo / "vendor" / "x.hpp", repo / "vendor" / "y.hpp"
            x.write_text("struct X { int a; };")
            y.write_text("struct Y {};")
            first = abi_of([x, y], repo)
            self.assertEqual(first, abi_of([y, x], repo))
            self.assertRegex(first, r"^[0-9a-f]{64}$")
            x.write_text("struct X { int a; int b; };")
            self.assertNotEqual(first, abi_of([x, y], repo))


if __name__ == "__main__":
    unittest.main()
```

En `tests/platform/test_startup_rules.cpp`, agregar a la lista de casos (antes del cierre `});`):
```cpp
        {"the game module messages are ASCII and send the player to the Builder", [] {
             const std::string messages[] = {vcsnx::module_missing_message(), vcsnx::module_mismatch_message(),
                                             vcsnx::module_load_message("missing import: memcpy")};
             for (const std::string &message : messages) {
                 require(message.find("VCSNX Builder") != std::string::npos, "names the Builder");
                 for (const char c : message) require(static_cast<unsigned char>(c) < 128, "ASCII only");
             }
             require(messages[0].find("vcs_game.so") != std::string::npos, "says which file is missing");
             require(messages[2].find("missing import: memcpy") != std::string::npos, "includes the loader's reason");
             require(vcsnx::module_load_message("caf\xc3\xa9").find("caf??") != std::string::npos, "non-ASCII replaced");
         }},
```

En `platform/switch/startup_rules.hpp`, antes del cierre del namespace (y `#include <string_view>` si falta):
```cpp
// The game module (vcs_game.so, spec §2): missing, made for another runtime, or not loadable.
[[nodiscard]] std::string module_missing_message();
[[nodiscard]] std::string module_mismatch_message();
[[nodiscard]] std::string module_load_message(std::string_view error);
```

Run: `python3 -I tools/tests/test_module_abi.py; cmake --build build/mac --target test_startup_rules 2>&1 | grep -m1 -i "undefined"`
Expected: `ModuleNotFoundError: No module named 'module_abi'` y un error de enlace `Undefined symbols ... module_missing_message`.

- [ ] **Step 2: Implementar la huella y los mensajes**

`tools/module_abi.py`:
```python
"""The game module's interface fingerprint (spec §2.3): SHA-256 of the vendor headers the translated code
is compiled against. CMake bakes it into vcsnx.nro and module_entry.o; tools/make_builder_kit.py writes the
same value to the kit. A runtime update that keeps these headers keeps the fingerprint, so players' existing
vcs_game.so still loads.
Usage: python3 -I tools/module_abi.py <aarch64-none-elf-g++> <repo> [--list]
"""
import hashlib
import subprocess
import sys
from pathlib import Path


def parse_depfile(text: str) -> list[str]:
    deps: set[str] = set()
    for rule in text.replace("\\\n", " ").splitlines():
        if ":" in rule:
            deps.update(rule.split(":", 1)[1].split())
    return sorted(deps)


def generated_dependencies(gxx: str, repo: Path) -> list[Path]:
    sources = sorted(str(p) for p in (repo / "generated").glob("*.cpp"))
    if not sources:
        raise SystemExit(f"no translated code in {repo / 'generated'}: run tools/restore_generated.sh first")
    flags = ["-std=c++20", "-D__SWITCH__", f"-I{repo}/generated", f"-I{repo}/vendor/vcs-recomp/profiles/vcs/host",
             f"-I{repo}/vendor/vcs-recomp/include"]
    text = subprocess.run([gxx, *flags, "-MM", *sources], capture_output=True, text=True, check=True).stdout
    vendor = f"{repo}/vendor/"
    return [Path(dep) for dep in parse_depfile(text) if dep.startswith(vendor)]


def abi_of(headers: list[Path], repo: Path) -> str:
    digest = hashlib.sha256()
    for header in sorted(headers, key=lambda p: p.relative_to(repo).as_posix()):
        digest.update(f"{header.relative_to(repo).as_posix()}\0{hashlib.sha256(header.read_bytes()).hexdigest()}\n".encode())
    return digest.hexdigest()


def main() -> int:
    if len(sys.argv) not in (3, 4):
        print("usage: module_abi.py <g++> <repo> [--list]", file=sys.stderr)
        return 2
    repo = Path(sys.argv[2]).resolve()
    headers = generated_dependencies(sys.argv[1], repo)
    print(";".join(str(h) for h in headers) if sys.argv[3:] == ["--list"] else abi_of(headers, repo))
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

En `platform/switch/startup_rules.cpp`, antes del cierre del namespace:
```cpp
std::string module_missing_message() {
    return "The game module is missing: sdmc:/switch/vcsnx/vcs_game.so\n"
           "Create it on your PC with VCSNX Builder, from your own copy of the\n"
           "game, and copy it next to vcsnx.nro.\n\n"
           "Press + to exit.";
}

std::string module_mismatch_message() {
    return "vcs_game.so was made for another version of vcsnx.nro.\n"
           "Create it again with the VCSNX Builder of this release.\n\n"
           "Press + to exit.";
}

std::string module_load_message(std::string_view error) {
    std::string ascii(error);
    for (char &c : ascii)
        if (static_cast<unsigned char>(c) >= 128) c = '?';
    return "Could not load vcs_game.so:\n" + ascii +
           "\nCreate it again with VCSNX Builder.\n\n"
           "Press + to exit.";
}
```

Run: `python3 -I tools/tests/test_module_abi.py && cmake --build build/mac --target test_startup_rules && ./build/mac/bin/test_startup_rules`
Expected: `Ran 2 tests ... OK` y todos los casos `ok`.

- [ ] **Step 3: Las entradas del módulo, el puente y la carga al arrancar**

`platform/switch/module_entry.cpp`:
```cpp
// The game module's entry points (spec §2.2), compiled into vcs_game.so with the translated code: the
// runtime's loader finds them by name. Our code, nothing of the game.
#include "psprecomp/runtime.hpp"

extern "C" __attribute__((visibility("default"))) void vcsnx_register_generated_functions(psprecomp::Runtime *runtime) {
    psprecomp::register_generated_functions(*runtime);
}

extern "C" __attribute__((visibility("default"))) const char vcsnx_module_abi[] = VCSNX_MODULE_ABI;
```

`platform/switch/module_eh_end.S`:
```asm
// Ends the game module's .eh_frame, as crtend.o ends a program's: linked last into vcs_game.so, so
// __register_frame knows where the module's unwind tables stop.
    .section .eh_frame,"a",%progbits
    .4byte 0
```

`platform/switch/game_module_bridge.cpp`:
```cpp
// On the Switch the game's translated code lives in vcs_game.so: main.cpp's call to
// register_generated_functions lands here and goes on into the module that nx_startup loaded (spec §2.4).
#include "game_module.hpp"
#include "psprecomp/runtime.hpp"

namespace vcsnx {
namespace {
void (*g_module_entry)(psprecomp::Runtime *) = nullptr;
} // namespace

void set_game_module_entry(void *entry) noexcept {
    g_module_entry = reinterpret_cast<void (*)(psprecomp::Runtime *)>(entry);
}
} // namespace vcsnx

namespace psprecomp {
void register_generated_functions(Runtime &runtime) {
    if (vcsnx::g_module_entry == nullptr) throw Error("vcs_game.so is not loaded");
    vcsnx::g_module_entry(&runtime);
}
} // namespace psprecomp
```

En `platform/switch/nx_startup.cpp`:
- junto a los demás includes de VCSNX:
```cpp
#if defined(VCSNX_GAME_MODULE)
#include "game_module.hpp"
extern "C" const vcsnx::RawImport vcsnx_module_imports[];
#endif
```
- antes de `struct StartupChecks {`:
```cpp
#if defined(VCSNX_GAME_MODULE)
// Loads sdmc:/switch/vcsnx/vcs_game.so before main(), while the libnx console can still show a message.
void load_game_module_or_exit(const std::string &data) {
    const std::string path = data + "/vcs_game.so";
    if (!file_exists(path)) show_message_and_exit(vcsnx::module_missing_message());
    std::string error;
    constexpr std::string_view kExports[] = {"vcsnx_register_generated_functions", "vcsnx_module_abi"};
    const auto module = vcsnx::load_module(path, vcsnx::module_imports(vcsnx_module_imports), kExports, error);
    if (!module) show_message_and_exit(vcsnx::module_load_message(error));
    if (std::string_view(static_cast<const char *>(module->symbols[1])) != VCSNX_MODULE_ABI)
        show_message_and_exit(vcsnx::module_mismatch_message());
    vcsnx::set_game_module_entry(module->symbols[0]);
}
#endif
```
- en `StartupChecks()`, justo después de `if (const auto problem = vcsnx::startup_problem(facts)) show_message_and_exit(*problem);`:
```cpp
#if defined(VCSNX_GAME_MODULE)
        load_game_module_or_exit(data);
#endif
```

- [ ] **Step 4: CMake del módulo y del runtime**

`cmake/vcsnx_game_module.cmake`:
```cmake
# The Switch build in two (spec v1 §2): vcsnx.nro without the game's translated code, and vcs_game.so with
# it. The fingerprint of the headers the translated code is compiled against goes into both.
enable_language(ASM)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(VCSNX_NM aarch64-none-elf-nm HINTS "$ENV{DEVKITPRO}/devkitA64/bin" /opt/devkitpro/devkitA64/bin REQUIRED)
execute_process(COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_SOURCE_DIR}/tools/module_abi.py" "${CMAKE_CXX_COMPILER}" "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE vcsnx_abi OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_SOURCE_DIR}/tools/module_abi.py" "${CMAKE_CXX_COMPILER}" "${CMAKE_SOURCE_DIR}" --list
    OUTPUT_VARIABLE vcsnx_abi_headers OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${vcsnx_abi_headers})
set(VCSNX_MODULE_ABI "${vcsnx_abi}" CACHE INTERNAL "Game module interface fingerprint (tools/module_abi.py)")
message(STATUS "VCSNX: game module interface ${VCSNX_MODULE_ABI}")

# The module's own entry points, compiled like the translated code.
add_library(vcsnx_module_entry OBJECT "${VCSNX_PLATFORM_DIR}/module_entry.cpp" "${VCSNX_PLATFORM_DIR}/module_eh_end.S")
target_include_directories(vcsnx_module_entry PRIVATE "${VCSNX_GENERATED_DIR}" "${VCSNX_PROFILE_DIR}/host")
target_link_libraries(vcsnx_module_entry PRIVATE psprecomp_core)
target_compile_definitions(vcsnx_module_entry PRIVATE VCSNX_MODULE_ABI="${VCSNX_MODULE_ABI}")
set_target_properties(vcsnx_module_entry PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(VCSNX_GAME_MODULE_FILE "${CMAKE_BINARY_DIR}/vcs_game.so")
add_custom_command(OUTPUT "${VCSNX_GAME_MODULE_FILE}"
    COMMAND "${CMAKE_CXX_COMPILER}" -march=armv8-a+crc+crypto -mtune=cortex-a57 -fPIC -shared -nostdlib
            -Wl,-Bsymbolic -Wl,--build-id=sha1 $<TARGET_OBJECTS:vcsnx_aot> $<TARGET_OBJECTS:vcsnx_module_entry>
            -lgcc -o "${VCSNX_GAME_MODULE_FILE}"
    DEPENDS vcsnx_aot vcsnx_module_entry $<TARGET_OBJECTS:vcsnx_aot> $<TARGET_OBJECTS:vcsnx_module_entry>
    COMMAND_EXPAND_LISTS VERBATIM)
add_custom_target(vcs_game_module ALL DEPENDS "${VCSNX_GAME_MODULE_FILE}")

set(VCSNX_GAME_MODULE_IMPORTS "${CMAKE_BINARY_DIR}/game_module_imports.S")
add_custom_command(OUTPUT "${VCSNX_GAME_MODULE_IMPORTS}"
    COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_SOURCE_DIR}/tools/make_module_imports.py" "${VCSNX_NM}"
            "${VCSNX_GAME_MODULE_FILE}" "${VCSNX_GAME_MODULE_IMPORTS}"
    DEPENDS "${VCSNX_GAME_MODULE_FILE}" "${CMAKE_SOURCE_DIR}/tools/make_module_imports.py" VERBATIM)

set(VCSNX_GAME_MODULE_RUNTIME_SOURCES
    "${VCSNX_PLATFORM_DIR}/elf_module.cpp" "${VCSNX_PLATFORM_DIR}/game_module.cpp"
    "${VCSNX_PLATFORM_DIR}/game_module_bridge.cpp" "${VCSNX_GAME_MODULE_IMPORTS}")
```

En `cmake/vcsnx_game.cmake`:
- después de `add_library(vcsnx_aot OBJECT ...)` y sus `target_*`:
```cmake
option(VCSNX_GAME_MODULE "Switch: the game's translated code in vcs_game.so, loaded by vcsnx.nro (spec v1 §2)" ${VCSNX_TARGET_SWITCH})
if(VCSNX_GAME_MODULE)
    set_target_properties(vcsnx_aot PROPERTIES POSITION_INDEPENDENT_CODE ON)
    include("${CMAKE_CURRENT_LIST_DIR}/vcsnx_game_module.cmake")
endif()
```
- reemplazar `add_executable(vcsnx "${VCSNX_PROFILE_DIR}/host/main.cpp" ${VCSNX_HOST_SOURCES} $<TARGET_OBJECTS:vcsnx_aot>)` por:
```cmake
if(VCSNX_GAME_MODULE)
    add_executable(vcsnx "${VCSNX_PROFILE_DIR}/host/main.cpp" ${VCSNX_HOST_SOURCES} ${VCSNX_GAME_MODULE_RUNTIME_SOURCES})
    target_compile_definitions(vcsnx PRIVATE VCSNX_GAME_MODULE=1 VCSNX_MODULE_ABI="${VCSNX_MODULE_ABI}")
else()
    add_executable(vcsnx "${VCSNX_PROFILE_DIR}/host/main.cpp" ${VCSNX_HOST_SOURCES} $<TARGET_OBJECTS:vcsnx_aot>)
endif()
```

En `tools/prepare_sd.sh`, después de `cp "$NRO" "$DEST/vcsnx.nro"`:
```bash
MODULE="$(find "$ROOT/build/switch" -maxdepth 1 -name vcs_game.so -print -quit)"
[ -z "$MODULE" ] || cp "$MODULE" "$DEST/vcs_game.so"   # the split build (VCSNX_GAME_MODULE=ON)
```

- [ ] **Step 5: La comprobación de que el runtime no lleva el juego**

`tools/check_runtime_clean.sh`:
```bash
#!/usr/bin/env bash
# Proves vcsnx.elf (the runtime published as vcsnx.nro) defines none of the translated code's own symbols:
# every strong function or data symbol the generated objects define must be absent from the runtime, except
# the bridge that forwards to the module (register_generated_functions). Usage:
#   tools/check_runtime_clean.sh <build/switch>
set -euo pipefail
BUILD="${1:?usage: check_runtime_clean.sh <build/switch>}"
NM=/opt/devkitpro/devkitA64/bin/aarch64-none-elf-nm
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
"$NM" --defined-only "$BUILD"/CMakeFiles/vcsnx_aot.dir/generated/*.o | awk 'NF==3 && $2 ~ /^[TDRB]$/ {print $3}' | sort -u \
    | grep -v '^_ZN9psprecomp28register_generated_functionsERNS_7RuntimeE$' > "$WORK/game"
"$NM" --defined-only "$BUILD/bin/vcsnx.elf" | awk 'NF==3 {print $3}' | sort -u > "$WORK/runtime"
leaks="$(comm -12 "$WORK/game" "$WORK/runtime" | wc -l | tr -d ' ')"
if [ "$leaks" != 0 ]; then
    echo "vcsnx.elf defines $leaks symbols of the translated code:" >&2
    comm -12 "$WORK/game" "$WORK/runtime" | head -20 >&2
    exit 1
fi
echo "runtime is clean: none of the $(wc -l < "$WORK/game" | tr -d ' ') symbols of the translated code are in vcsnx.elf"
```

Run:
```bash
chmod +x tools/check_runtime_clean.sh && export DEVKITPRO=/opt/devkitpro DEVKITA64=/opt/devkitpro/devkitA64 && \
cmake build/switch && cmake --build build/switch && ls -la build/switch/vcsnx.nro build/switch/vcs_game.so && \
grep -c "^    .quad .Lvcsnx_import" build/switch/game_module_imports.S && tools/check_runtime_clean.sh build/switch && \
(cmake --build build/mac >/dev/null && cd build/mac && ctest 2>&1 | tail -3)
```
Expected: `vcsnx.nro` de ~38 MB y `vcs_game.so` de ~75 MB; `56` importaciones; `runtime is clean: ...`; y en el Mac `100% tests passed out of 30`.

- [ ] **Step 6: Commit**

```bash
git add tools/module_abi.py tools/tests/test_module_abi.py tools/check_runtime_clean.sh platform/switch/module_entry.cpp platform/switch/module_eh_end.S platform/switch/game_module_bridge.cpp cmake/vcsnx_game_module.cmake cmake/vcsnx_game.cmake platform/switch/startup_rules.hpp platform/switch/startup_rules.cpp tests/platform/test_startup_rules.cpp platform/switch/nx_startup.cpp tools/prepare_sd.sh
git commit -m "feat(platform): split the Switch build into a publishable runtime and vcs_game.so

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: El juego con el módulo en la consola (B2)

**Files:** ninguno nuevo; resultados en el ledger.

- [ ] **Step 1: El mensaje cuando falta el módulo**

Copiar `build/switch/vcsnx.nro` a una carpeta estable del scratchpad, pedir "ftp listo", subirlo a `switch/vcsnx/vcsnx.nro` (todavía no hay `vcs_game.so` en la SD) y pedir al usuario que lo abra.
Expected: la pantalla "The game module is missing: sdmc:/switch/vcsnx/vcs_game.so ... VCSNX Builder ..." y + sale.

- [ ] **Step 2: Jugar con el módulo**

Subir `build/switch/vcs_game.so` (copia estable) a `switch/vcsnx/vcs_game.so` y pedir al usuario que juegue unos minutos: splash con sonido, intro, partida, menú de pausa, HUD.
Expected: igual que la run 17. Bajar `vcsnx_stderr.log`, confirmar que no hay errores de carga y anotar en el ledger.

---

### Task 6: Hashes y kit (B3)

**Files:**
- Create: `tools/make_builder_hashes.py`, `builder/hashes/executable.sha256`, `builder/hashes/generated.sha256`
- Create: `tools/make_builder_kit.py`, `tools/tests/test_make_builder_kit.py`

**Interfaces:**
- Consumes: `tools/module_abi.py` (`generated_dependencies`, `abi_of`) (Task 4); el build de Switch partido (Task 4).
- Produces: `build/builder/kit/` con `kit.json` (`{"version": "1.0.0", "gcc": "16.1.0", "abi": "<hex>"}`), `compile.json`, `module_link.json` (`{"args": [...], "generated_order": [...]}`), `obj/module_entry.o`, `obj/module_eh_end.o`, `include/host/...`, `include/psprecomp/...`, `vcsnx.nro`, `VCSNative.ini`.
- Produces (Python): `compile_template(args, repo) -> list[str]`, `module_link_template(args) -> tuple[list[str], list[tuple[str, str]], list[str]]`.

- [ ] **Step 1: Hashes**

```python
"""Writes the hash lists the VCSNX Builder checks against: the two known ULUS-10160 executables and the
237 files of PSPRecomp's published generated/ (commit f6e7d41). They are only numbers: nothing of the game.
Usage: python3 -I tools/make_builder_hashes.py <PSPRecomp checkout>/profiles/vcs/generated builder/hashes
"""
import hashlib
import sys
from pathlib import Path

KNOWN_EXECUTABLES = {
    "85c5e172fa0207086b72a37649e9a10d47845f0866b0dee179c48d930e43d17f": "EBOOT.BIN",
    "fee2e86c7fe457ab6da463d9fdc16f156a0900adcdaa2fd260206c11907c5d65": "BOOT.BIN",
}


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: make_builder_hashes.py <generated folder> <output folder>", file=sys.stderr)
        return 2
    generated, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    files = sorted(p for p in generated.iterdir() if p.is_file())
    if len(files) != 237:
        print(f"expected 237 files in {generated}, found {len(files)}", file=sys.stderr)
        return 3
    lines = [f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}" for p in files]
    (out / "generated.sha256").write_text("\n".join(lines) + "\n")
    (out / "executable.sha256").write_text("".join(f"{sha}  {name}\n" for sha, name in KNOWN_EXECUTABLES.items()))
    print(f"{len(lines)} generated hashes, {len(KNOWN_EXECUTABLES)} executable hashes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

Run: `python3 -I tools/make_builder_hashes.py .cache/upstream/profiles/vcs/generated builder/hashes && wc -l builder/hashes/*.sha256`
Expected: `237 generated hashes, 2 executable hashes`; 237 y 2 líneas.

- [ ] **Step 2: Test del kit (falla)**

```python
"""Tests for tools/make_builder_kit.py (stdlib unittest). Run: python3 -I tools/tests/test_make_builder_kit.py"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from make_builder_kit import compile_template, module_link_template  # noqa: E402

REPO = "/Users/x/VCSNX"
GXX = "/opt/devkitpro/devkitA64/bin/aarch64-none-elf-g++"
AOT_COMPILE = [
    GXX, f"-I{REPO}/generated", f"-I{REPO}/vendor/vcs-recomp/profiles/vcs/host", f"-I{REPO}/vendor/vcs-recomp/include",
    "-isystem", "/opt/devkitpro/libnx/include", "-march=armv8-a+crc+crypto", "-std=c++20", "-fPIC", "-O2", "-g0", "-w",
    "-ffp-contract=off", "-MD", "-MT", "CMakeFiles/vcsnx_aot.dir/generated/generated_unit_0000.o",
    "-MF", "CMakeFiles/vcsnx_aot.dir/generated/generated_unit_0000.o.d",
    "-o", "CMakeFiles/vcsnx_aot.dir/generated/generated_unit_0000.o", "-c", f"{REPO}/generated/generated_unit_0000.cpp",
]
BUILD = "/Users/x/VCSNX/build/switch"
MODULE_LINK = [
    "cd", BUILD, "&&", GXX, "-march=armv8-a+crc+crypto", "-mtune=cortex-a57", "-fPIC", "-shared", "-nostdlib",
    "-Wl,-Bsymbolic", "-Wl,--build-id=sha1",
    f"{BUILD}/CMakeFiles/vcsnx_aot.dir/generated/generated_registry.o", f"{BUILD}/CMakeFiles/vcsnx_aot.dir/generated/generated_unit_0000.o",
    f"{BUILD}/CMakeFiles/vcsnx_module_entry.dir/platform/switch/module_entry.o",
    f"{BUILD}/CMakeFiles/vcsnx_module_entry.dir/platform/switch/module_eh_end.o",
    "-lgcc", "-o", f"{BUILD}/vcs_game.so",
]


class MakeBuilderKitTest(unittest.TestCase):
    def test_compile_template_keeps_the_flags_and_names_the_paths(self) -> None:
        self.assertEqual(compile_template(AOT_COMPILE, REPO), [
            "-I{generated}", "-I{kit}/include/host", "-I{kit}/include/psprecomp", "-isystem", "{toolchain}/libnx/include",
            "-march=armv8-a+crc+crypto", "-std=c++20", "-fPIC", "-O2", "-g0", "-w", "-ffp-contract=off",
            "-o", "{output}", "-c", "{source}",
        ])

    def test_the_module_link_puts_the_translated_code_in_one_placeholder_and_our_objects_in_the_kit(self) -> None:
        args, objects, order = module_link_template(MODULE_LINK)
        self.assertEqual(order, ["generated_registry", "generated_unit_0000"])
        self.assertEqual(args, ["-march=armv8-a+crc+crypto", "-mtune=cortex-a57", "-fPIC", "-shared", "-nostdlib",
                                "-Wl,-Bsymbolic", "-Wl,--build-id=sha1", "{generated_objects}",
                                "{kit}/obj/module_entry.o", "{kit}/obj/module_eh_end.o", "-lgcc", "-o", "{output}"])
        self.assertEqual(objects, [("obj/module_entry.o", f"{BUILD}/CMakeFiles/vcsnx_module_entry.dir/platform/switch/module_entry.o"),
                                   ("obj/module_eh_end.o", f"{BUILD}/CMakeFiles/vcsnx_module_entry.dir/platform/switch/module_eh_end.o")])

    def test_an_unknown_path_in_the_link_is_refused(self) -> None:
        with self.assertRaises(SystemExit):
            module_link_template(MODULE_LINK[:-3] + ["/Users/x/elsewhere/libfoo.a", "-o", f"{BUILD}/vcs_game.so"])


if __name__ == "__main__":
    unittest.main()
```

Run: `python3 -I tools/tests/test_make_builder_kit.py`
Expected: `ModuleNotFoundError: No module named 'make_builder_kit'`

- [ ] **Step 3: Implementar el kit**

```python
"""Builds the VCSNX Builder's kit from the split Switch build (spec §3): the exact compile line of the
translated code and link line of vcs_game.so (with placeholders), the module's own entry objects, the
headers the translated code includes, the interface fingerprint, and the runtime vcsnx.nro the Builder puts
the player's icon into. Nothing in the kit comes from the game.
Usage: python3 -I tools/make_builder_kit.py <build/switch> <repo> <output kit folder>
"""
import json
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from module_abi import abi_of, generated_dependencies  # noqa: E402

DKP = "/opt/devkitpro"
GXX = f"{DKP}/devkitA64/bin/aarch64-none-elf-g++"
GCC_VERSION = "16.1.0"


def compile_template(args: list[str], repo: str) -> list[str]:
    out: list[str] = []
    it = iter(args[1:])  # the compiler itself is the Builder's toolchain
    for arg in it:
        if arg == "-MD":
            continue
        if arg in ("-MT", "-MF"):
            next(it)
            continue
        if arg == "-o":
            next(it)
            out += ["-o", "{output}"]
        elif arg == "-c":
            next(it)
            out += ["-c", "{source}"]
        elif arg == f"-I{repo}/generated":
            out.append("-I{generated}")
        elif arg == f"-I{repo}/vendor/vcs-recomp/profiles/vcs/host":
            out.append("-I{kit}/include/host")
        elif arg == f"-I{repo}/vendor/vcs-recomp/include":
            out.append("-I{kit}/include/psprecomp")
        else:
            out.append(arg.replace(DKP, "{toolchain}"))
    return out


def module_link_template(args: list[str]):
    start = next(i for i, a in enumerate(args) if a.endswith("aarch64-none-elf-g++"))
    out: list[str] = []
    objects: list[tuple[str, str]] = []
    order: list[str] = []
    it = iter(args[start + 1:])
    for arg in it:
        if "vcsnx_aot.dir/generated/" in arg:
            if not order:
                out.append("{generated_objects}")
            order.append(Path(arg).stem)
        elif "vcsnx_module_entry.dir/" in arg:
            dest = f"obj/{Path(arg).name}"
            objects.append((dest, arg))
            out.append("{kit}/" + dest)
        elif arg == "-o":
            next(it)
            out += ["-o", "{output}"]
        else:
            out.append(arg)
    leftovers = [a for a in out if a.startswith("/") or DKP in a]
    if leftovers:
        raise SystemExit(f"module link line has paths the kit does not know: {leftovers}")
    return out, objects, order


def main() -> int:
    if len(sys.argv) != 4:
        print("usage: make_builder_kit.py <build/switch> <repo> <output kit folder>", file=sys.stderr)
        return 2
    build, repo, kit = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve(), Path(sys.argv[3]).resolve()
    gcc = subprocess.run([GXX, "-dumpfullversion"], capture_output=True, text=True, check=True).stdout.strip()
    if gcc != GCC_VERSION:
        raise SystemExit(f"the kit must be built with GCC {GCC_VERSION} (the Windows toolchain's), found {gcc}")
    commands = subprocess.run(["ninja", "-C", str(build), "-t", "commands", "vcs_game.so"],
                              capture_output=True, text=True, check=True).stdout.splitlines()
    aot = next(shlex.split(c) for c in commands if "vcsnx_aot.dir/generated/generated_unit_0000.o" in c and " -c " in c)
    link = shlex.split(commands[-1])
    headers = generated_dependencies(GXX, repo)
    abi = abi_of(headers, repo)
    baked = re.search(r"^VCSNX_MODULE_ABI:INTERNAL=([0-9a-f]{64})$", (build / "CMakeCache.txt").read_text(), re.M)
    if not baked or baked.group(1) != abi:
        raise SystemExit("build/switch was configured with another interface fingerprint: reconfigure and rebuild it first")
    if kit.exists():
        shutil.rmtree(kit)
    (kit / "obj").mkdir(parents=True)
    link_args, objects, order = module_link_template(link)
    for dest, source in objects:
        shutil.copy2(source if Path(source).is_absolute() else build / source, kit / dest)
    roots = {f"{repo}/vendor/vcs-recomp/profiles/vcs/host/": "include/host/", f"{repo}/vendor/vcs-recomp/include/": "include/psprecomp/"}
    for header in headers:
        prefix = next((p for p in roots if str(header).startswith(p)), None)
        if prefix is None:
            raise SystemExit(f"the translated code includes a header outside the kit's include roots: {header}")
        target = kit / roots[prefix] / str(header)[len(prefix):]
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(header, target)
    (kit / "compile.json").write_text(json.dumps(compile_template(aot, str(repo)), indent=1))
    (kit / "module_link.json").write_text(json.dumps({"args": link_args, "generated_order": order}, indent=1))
    version = re.search(r"project\(VCSNX VERSION (\d+\.\d+\.\d+)", (repo / "CMakeLists.txt").read_text()).group(1)
    (kit / "kit.json").write_text(json.dumps({"version": version, "gcc": gcc, "abi": abi}, indent=1))
    shutil.copy2(build / "vcsnx.nro", kit / "vcsnx.nro")
    shutil.copy2(repo / "config" / "VCSNative.ini", kit / "VCSNative.ini")
    size = sum(p.stat().st_size for p in kit.rglob("*") if p.is_file()) // (1 << 20)
    print(f"kit: {len(order)} translated units to build, {len(objects)} module objects, {len(headers)} headers, abi {abi[:12]}, {size} MiB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: Correr los tests y armar el kit**

Run: `python3 -I tools/tests/test_make_builder_kit.py && python3 -I tools/make_builder_kit.py build/switch . build/builder/kit && ls build/builder/kit build/builder/kit/obj`
Expected: `Ran 3 tests ... OK`; `kit: 235 translated units to build, 2 module objects, ... headers, abi ..., ~38 MiB`; `VCSNative.ini compile.json include kit.json module_link.json obj vcsnx.nro` y `module_eh_end.o module_entry.o`.

- [ ] **Step 5: Commit**

```bash
git add tools/make_builder_hashes.py tools/make_builder_kit.py tools/tests/test_make_builder_kit.py builder/hashes
git commit -m "feat(tools): the Builder's hash lists and kit

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Builder.Core — hashes, textos y la copia del juego (B3)

**Files:**
- Create: `builder/VCSNX.Builder.sln`, `builder/Builder.Core/Builder.Core.csproj`, `builder/Builder.Core.Tests/Builder.Core.Tests.csproj`
- Create: `builder/Builder.Core/HashList.cs`, `builder/Builder.Core/Strings.cs`, `builder/Builder.Core/GameCopy.cs`
- Test: `builder/Builder.Core.Tests/TestFiles.cs`, `builder/Builder.Core.Tests/GameCopyTests.cs`

**Interfaces:**
- Produces: `HashList.Load(string) -> IReadOnlyDictionary<string,string>`, `HashList.Sha256Of(string) -> string`.
- Produces: `Strings.Get(string key, params object[] args)`, `Strings.Get(CultureInfo, string, params object[])`, `BuilderException(string key, params object[] args)` con `Key`, `Args`, `Localized`.
- Produces: `enum ExecutableState { Missing, Encrypted, Elf }`, `record GameCheck(string? DataRoot, string? Executable, ExecutableState State, bool KnownVersion, string? DiscVersion)` con `Ok`; `GameCopy.Candidates`, `FindDataRoot(string)`, `Check(string, IReadOnlySet<string>)`, `Classify(string)`, `ReadDiscVersion(string)`.
- Produces (tests): `TestFiles.TempDir()`, `TestFiles.Write(string, byte[])`, `TestFiles.Elf(byte[])`, `TestFiles.Sfo(params (string Key, string Value)[])`, `TestFiles.Nro(byte[] icon, byte[] nacp)`.

- [ ] **Step 1: Crear la solución y los proyectos**

Run:
```bash
cd builder && dotnet new sln -n VCSNX.Builder && \
dotnet new classlib -n Builder.Core -f net10.0 -o Builder.Core && rm Builder.Core/Class1.cs && \
dotnet new xunit -n Builder.Core.Tests -f net10.0 -o Builder.Core.Tests && rm Builder.Core.Tests/UnitTest1.cs && \
dotnet sln add Builder.Core Builder.Core.Tests && dotnet add Builder.Core.Tests reference Builder.Core && \
dotnet add Builder.Core package SkiaSharp && dotnet add Builder.Core package SkiaSharp.NativeAssets.macOS && \
dotnet add Builder.Core package SkiaSharp.NativeAssets.Win32 && cd ..
```
Expected: cada comando termina sin error.

- [ ] **Step 2: Escribir los tests que fallan**

`builder/Builder.Core.Tests/TestFiles.cs`:
```csharp
using System.Text;

namespace Vcsnx.Builder.Core.Tests;

internal static class TestFiles
{
    public static string TempDir()
    {
        var dir = Path.Combine(Path.GetTempPath(), "vcsnx-builder-tests", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        return dir;
    }

    public static string Write(string path, byte[] data)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllBytes(path, data);
        return path;
    }

    public static byte[] Elf(byte[] tail) => [0x7F, (byte)'E', (byte)'L', (byte)'F', .. tail];

    // A PSP PARAM.SFO with UTF-8 string entries.
    public static byte[] Sfo(params (string Key, string Value)[] entries)
    {
        var keys = new MemoryStream();
        var values = new MemoryStream();
        var index = new MemoryStream();
        foreach (var (key, value) in entries)
        {
            var bytes = Encoding.UTF8.GetBytes(value + "\0");
            var length = (bytes.Length + 3) / 4 * 4;
            index.Write(BitConverter.GetBytes((ushort)keys.Length));
            index.Write(BitConverter.GetBytes((ushort)0x0204));
            index.Write(BitConverter.GetBytes(bytes.Length));
            index.Write(BitConverter.GetBytes(length));
            index.Write(BitConverter.GetBytes((int)values.Length));
            keys.Write(Encoding.ASCII.GetBytes(key + "\0"));
            values.Write(bytes);
            values.Write(new byte[length - bytes.Length]);
        }
        while (keys.Length % 4 != 0) keys.WriteByte(0);
        var keyTable = 20 + (int)index.Length;
        var sfo = new MemoryStream();
        sfo.Write([0, (byte)'P', (byte)'S', (byte)'F', 1, 1, 0, 0]);
        sfo.Write(BitConverter.GetBytes(keyTable));
        sfo.Write(BitConverter.GetBytes(keyTable + (int)keys.Length));
        sfo.Write(BitConverter.GetBytes(entries.Length));
        index.WriteTo(sfo);
        keys.WriteTo(sfo);
        values.WriteTo(sfo);
        return sfo.ToArray();
    }

    // A minimal NRO: 0x100 bytes of program (header with "NRO0" and its size) then an ASET with icon and NACP.
    public static byte[] Nro(byte[] icon, byte[] nacp)
    {
        var program = new byte[0x100];
        "NRO0"u8.CopyTo(program.AsSpan(0x10));
        BitConverter.TryWriteBytes(program.AsSpan(0x18), (uint)program.Length);
        for (var i = 0x80; i < program.Length; ++i) program[i] = (byte)i;
        var header = new byte[0x38];
        "ASET"u8.CopyTo(header);
        BitConverter.TryWriteBytes(header.AsSpan(0x08), 0x38UL);
        BitConverter.TryWriteBytes(header.AsSpan(0x10), (ulong)icon.Length);
        BitConverter.TryWriteBytes(header.AsSpan(0x18), (ulong)(0x38 + icon.Length));
        BitConverter.TryWriteBytes(header.AsSpan(0x20), (ulong)nacp.Length);
        BitConverter.TryWriteBytes(header.AsSpan(0x28), (ulong)(0x38 + icon.Length + nacp.Length));
        return [.. program, .. header, .. icon, .. nacp];
    }
}
```

`builder/Builder.Core.Tests/GameCopyTests.cs`:
```csharp
using System.Security.Cryptography;

namespace Vcsnx.Builder.Core.Tests;

public sealed class GameCopyTests
{
    static readonly byte[] KnownElf = TestFiles.Elf([1, 2, 3]);
    static readonly HashSet<string> Known = [Convert.ToHexStringLower(SHA256.HashData(KnownElf))];

    static string DataWith(string executable, byte[] bytes)
    {
        var data = Path.Combine(TestFiles.TempDir(), "PSP_DATA");
        TestFiles.Write(Path.Combine(data, "PSP_GAME", "PARAM.SFO"), TestFiles.Sfo(("DISC_ID", "ULUS10160"), ("DISC_VERSION", "1.03")));
        TestFiles.Write(Path.Combine(data, executable), bytes);
        return data;
    }

    [Fact]
    public void A_decrypted_known_executable_is_ok()
    {
        var check = GameCopy.Check(DataWith("PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF", KnownElf), Known);
        Assert.True(check.Ok);
        Assert.Equal("1.03", check.DiscVersion);
    }

    [Fact]
    public void The_folder_that_holds_PSP_DATA_and_PSP_GAME_inside_it_are_found_too()
    {
        var data = DataWith("PSP_GAME/SYSDIR/BOOT.BIN", KnownElf);
        Assert.Equal(data, GameCopy.FindDataRoot(Path.GetDirectoryName(data)!));
        Assert.Equal(data, GameCopy.FindDataRoot(Path.Combine(data, "PSP_GAME")));
        Assert.Equal(data, GameCopy.FindDataRoot(data));
    }

    [Fact]
    public void A_folder_without_the_game_has_no_data_root()
    {
        Assert.Null(GameCopy.FindDataRoot(TestFiles.TempDir()));
        Assert.Equal(ExecutableState.Missing, GameCopy.Check(TestFiles.TempDir(), Known).State);
    }

    [Fact]
    public void An_encrypted_executable_is_reported()
    {
        var check = GameCopy.Check(DataWith("PSP_GAME/SYSDIR/EBOOT.BIN", "~PSP\0\0\0\0"u8.ToArray()), Known);
        Assert.Equal(ExecutableState.Encrypted, check.State);
        Assert.False(check.Ok);
    }

    [Fact]
    public void Another_game_or_region_is_not_known()
    {
        var check = GameCopy.Check(DataWith("PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF", TestFiles.Elf([9, 9, 9])), Known);
        Assert.Equal(ExecutableState.Elf, check.State);
        Assert.False(check.KnownVersion);
    }

    [Fact]
    public void The_game_s_own_order_picks_the_executable()
    {
        var data = DataWith("PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF", KnownElf);
        TestFiles.Write(Path.Combine(data, "EBOOT.BIN"), TestFiles.Elf([7]));
        Assert.EndsWith("EBOOT_DECRYPTED.ELF", GameCopy.Check(data, Known).Executable);
    }

    [Fact]
    public void Hash_lists_are_read_by_name()
    {
        var list = TestFiles.Write(Path.Combine(TestFiles.TempDir(), "h.sha256"),
            System.Text.Encoding.ASCII.GetBytes($"{new string('a', 64)}  generated_units.hpp\n\n{new string('B', 64)}  BOOT.BIN\n"));
        var byName = HashList.Load(list);
        Assert.Equal(new string('a', 64), byName["generated_units.hpp"]);
        Assert.Equal(new string('b', 64), byName["BOOT.BIN"]);
    }

    [Fact]
    public void Messages_follow_the_language()
    {
        var spanish = Strings.Get(new System.Globalization.CultureInfo("es-CL"), "GameMissing");
        var english = Strings.Get(new System.Globalization.CultureInfo("en-US"), "GameMissing");
        Assert.Contains("PSP_DATA", spanish);
        Assert.NotEqual(spanish, english);
        Assert.Equal(english, new BuilderException("GameMissing").Message);
    }
}
```

- [ ] **Step 3: Correrlos y ver que fallan**

Run: `dotnet test builder/Builder.Core.Tests`
Expected: error de compilación `The name 'GameCopy' does not exist in the current context` (y `HashList`, `Strings`).

- [ ] **Step 4: Implementar**

`builder/Builder.Core/HashList.cs`:
```csharp
using System.Security.Cryptography;

namespace Vcsnx.Builder.Core;

/// <summary>A sha256sum-style list: "&lt;sha256&gt;  &lt;name&gt;" per line (builder/hashes).</summary>
public static class HashList
{
    public static IReadOnlyDictionary<string, string> Load(string path)
    {
        var byName = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var raw in File.ReadAllLines(path))
        {
            var line = raw.Trim();
            if (line.Length == 0 || line.StartsWith('#')) continue;
            if (line.Length < 67 || line.IndexOf("  ", StringComparison.Ordinal) != 64)
                throw new FormatException($"{path}: not a sha256sum line: '{raw}'");
            byName[line[66..]] = line[..64].ToLowerInvariant();
        }
        return byName;
    }

    public static string Sha256Of(string file)
    {
        using var stream = File.OpenRead(file);
        return Convert.ToHexStringLower(SHA256.HashData(stream));
    }
}
```

`builder/Builder.Core/Strings.cs`:
```csharp
using System.Globalization;

namespace Vcsnx.Builder.Core;

/// <summary>Every text the player reads, in Spanish (Windows in Spanish) and English (anything else).</summary>
public static class Strings
{
    static readonly Dictionary<string, (string Es, string En)> Table = new(StringComparer.Ordinal)
    {
        ["GameMissing"] = ("No encontré el ejecutable del juego. Elige la carpeta PSP_DATA (la que tiene PSP_GAME).",
                           "No game executable found. Pick the PSP_DATA folder (the one with PSP_GAME)."),
        ["GameEncrypted"] = ("El ejecutable está cifrado. Desencríptalo con PPSSPP (Ajustes → Herramientas → Herramientas de desarrollo → Volcar EBOOT.BIN desencriptado al iniciar) y guárdalo como PSP_GAME\\SYSDIR\\EBOOT_DECRYPTED.ELF.",
                             "The executable is encrypted. Decrypt it with PPSSPP (Settings → Tools → Developer tools → Dump decrypted EBOOT.BIN on game boot) and save it as PSP_GAME\\SYSDIR\\EBOOT_DECRYPTED.ELF."),
        ["GameUnknown"] = ("Esta copia no es GTA Vice City Stories ULUS-10160 (EE. UU.). Solo esa versión funciona.",
                           "This copy is not GTA Vice City Stories ULUS-10160 (US). Only that version works."),
        ["GameOk"] = ("✓ GTA Vice City Stories ULUS-10160 {0}", "✓ GTA Vice City Stories ULUS-10160 {0}"),
        ["PathNotAscii"] = ("Mueve la carpeta del programa a una ruta sin acentos ni caracteres especiales, por ejemplo C:\\VCSNX-Builder (ahora está en {0}).",
                            "Move the program's folder to a path without accents or special characters, for example C:\\VCSNX-Builder (it is now in {0})."),
        ["NotEnoughSpace"] = ("Faltan {0} GB libres en el disco de la carpeta del programa.", "{0} GB more free space is needed on the program's drive."),
        ["DownloadFailed"] = ("No pude bajar el código traducido ({0}). Revisa tu conexión, o elige el ZIP de PSPRecomp ya bajado.",
                              "Could not download the translated code ({0}). Check your connection, or pick an already downloaded PSPRecomp ZIP."),
        ["CorpusMismatch"] = ("El código traducido no coincide con el esperado. Vuelve a intentarlo más tarde.",
                              "The translated code does not match what is expected. Try again later."),
        ["CompileFailed"] = ("Falló la compilación de {0}. El detalle está en salida\\builder.log.", "Compiling {0} failed. Details are in salida\\builder.log."),
        ["ToolFailed"] = ("Falló {0}. El detalle está en salida\\builder.log.", "{0} failed. Details are in salida\\builder.log."),
        ["SdCopyFailed"] = ("No pude copiar a la SD ({0}). Los archivos están en {1}: cópialos a mano o vuelve a intentarlo.",
                            "Could not copy to the SD card ({0}). The files are in {1}: copy them by hand or try again."),
        ["IconUnreadable"] = ("No pude leer esa imagen. Usa un PNG o JPG.", "Could not read that image. Use a PNG or JPG."),
        ["Done"] = ("Listo: vcs_game.so y vcsnx.nro están en {0}. Cópialos a switch\\vcsnx\\ en tu SD y abre VCSNX manteniendo R al abrir un juego.",
                    "Done: vcs_game.so and vcsnx.nro are in {0}. Copy them to switch\\vcsnx\\ on your SD card and open VCSNX holding R while starting a game."),
        ["DoneSd"] = ("Listo: vcs_game.so y vcsnx.nro ya están en tu SD (y en {0}). Abre VCSNX manteniendo R al abrir un juego.",
                      "Done: vcs_game.so and vcsnx.nro are on your SD card (and in {0}). Open VCSNX holding R while starting a game."),
        ["StepGame"] = ("1. Tu juego (carpeta PSP_DATA)", "1. Your game (PSP_DATA folder)"),
        ["StepIcon"] = ("2. Ícono (opcional)", "2. Icon (optional)"),
        ["StepSd"] = ("3. SD de la Switch (opcional)", "3. Switch SD card (optional)"),
        ["Choose"] = ("Elegir", "Choose"),
        ["ChooseImage"] = ("Elegir imagen…", "Choose image…"),
        ["Remove"] = ("Quitar", "Remove"),
        ["NoSd"] = ("Ninguna", "None"),
        ["Create"] = ("Crear vcs_game.so", "Create vcs_game.so"),
        ["Cancel"] = ("Cancelar", "Cancel"),
        ["Cancelled"] = ("Cancelado. Lo ya hecho se guarda para la próxima vez.", "Cancelled. What is done is kept for next time."),
        ["Details"] = ("Ver detalles", "Show details"),
        ["Remaining"] = ("Tiempo restante: ~{0} min", "Time left: ~{0} min"),
        ["StageChecking"] = ("Revisando…", "Checking…"),
        ["StageDownloading"] = ("Bajando el código traducido…", "Downloading the translated code…"),
        ["StageCompiling"] = ("Compilando el juego {0}/{1}", "Compiling the game {0}/{1}"),
        ["StageLinking"] = ("Enlazando vcs_game.so…", "Linking vcs_game.so…"),
        ["StagePackaging"] = ("Preparando vcsnx.nro…", "Preparing vcsnx.nro…"),
        ["StageCopyingToSd"] = ("Copiando a la SD…", "Copying to the SD card…"),
        ["StageDone"] = ("Listo", "Done"),
    };

    public static string Get(string key, params object[] args) => Get(CultureInfo.CurrentUICulture, key, args);

    public static string Get(CultureInfo culture, string key, params object[] args)
    {
        var (es, en) = Table[key];
        return string.Format(culture, culture.TwoLetterISOLanguageName == "es" ? es : en, args);
    }
}

/// <summary>A problem the player can act on; Message is English (for logs), Localized is for the window.</summary>
public sealed class BuilderException : Exception
{
    public BuilderException(string key, params object[] args) : base(Strings.Get(CultureInfo.InvariantCulture, key, args))
    {
        Key = key;
        Args = args;
    }

    public string Key { get; }
    public object[] Args { get; }
    public string Localized => Strings.Get(Key, Args);
}
```

`builder/Builder.Core/GameCopy.cs`:
```csharp
using System.Text;

namespace Vcsnx.Builder.Core;

public enum ExecutableState { Missing, Encrypted, Elf }

public sealed record GameCheck(string? DataRoot, string? Executable, ExecutableState State, bool KnownVersion, string? DiscVersion)
{
    public bool Ok => State == ExecutableState.Elf && KnownVersion;
}

/// <summary>The player's copy of the game: where its PSP_DATA is and whether its executable is the one VCSNX runs.</summary>
public static class GameCopy
{
    /// <summary>The game's own list and order (vendor profiles/vcs/host/vcs_bootstrap_paths.cpp): the console uses the first that exists.</summary>
    public static readonly string[] Candidates =
    [
        "PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF", "EBOOT_DECRYPTED.ELF", "PSP_GAME/SYSDIR/EBOOT_DECRYPTED.BIN", "EBOOT_DECRYPTED.BIN",
        "PSP_GAME/SYSDIR/BOOT.BIN", "BOOT.BIN", "PSP_GAME/SYSDIR/EBOOT.BIN", "EBOOT.BIN",
    ];

    static readonly byte[] ElfMagic = [0x7F, (byte)'E', (byte)'L', (byte)'F'];

    /// <summary>PSP_DATA for what the player picked: PSP_DATA itself, the folder that holds it, or PSP_GAME inside it.</summary>
    public static string? FindDataRoot(string picked)
    {
        if (string.IsNullOrWhiteSpace(picked) || !Directory.Exists(picked)) return null;
        var full = Path.GetFullPath(picked).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        string[] tries = [full, Path.Combine(full, "PSP_DATA"), Path.GetDirectoryName(full) ?? full];
        return tries.FirstOrDefault(dir => Directory.Exists(Path.Combine(dir, "PSP_GAME")) || Candidates.Any(c => File.Exists(Path.Combine(dir, c))));
    }

    public static GameCheck Check(string picked, IReadOnlySet<string> knownSha256)
    {
        var root = FindDataRoot(picked);
        if (root is null) return new GameCheck(null, null, ExecutableState.Missing, false, null);
        var version = ReadDiscVersion(Path.Combine(root, "PSP_GAME", "PARAM.SFO"));
        var executable = Candidates.Select(c => Path.Combine(root, c)).FirstOrDefault(File.Exists);
        if (executable is null) return new GameCheck(root, null, ExecutableState.Missing, false, version);
        var state = Classify(executable);
        var known = state == ExecutableState.Elf && knownSha256.Contains(HashList.Sha256Of(executable));
        return new GameCheck(root, executable, state, known, version);
    }

    public static ExecutableState Classify(string file)
    {
        Span<byte> head = stackalloc byte[4];
        using var stream = File.OpenRead(file);
        return stream.ReadAtLeast(head, 4, throwOnEndOfStream: false) == 4 && head.SequenceEqual(ElfMagic)
            ? ExecutableState.Elf
            : ExecutableState.Encrypted;
    }

    /// <summary>DISC_VERSION from a PSP PARAM.SFO ("1.03"), or null.</summary>
    public static string? ReadDiscVersion(string paramSfo)
    {
        if (!File.Exists(paramSfo)) return null;
        var data = File.ReadAllBytes(paramSfo);
        if (data.Length < 20 || !data.AsSpan(0, 4).SequenceEqual("\0PSF"u8)) return null;
        var keyTable = BitConverter.ToInt32(data, 8);
        var dataTable = BitConverter.ToInt32(data, 12);
        var count = BitConverter.ToInt32(data, 16);
        for (var i = 0; i < count; ++i)
        {
            var entry = 20 + i * 16;
            if (entry + 16 > data.Length) return null;
            var keyStart = keyTable + BitConverter.ToUInt16(data, entry);
            var length = BitConverter.ToInt32(data, entry + 4);
            var valueStart = dataTable + BitConverter.ToInt32(data, entry + 12);
            if (keyStart >= data.Length || valueStart + length > data.Length) return null;
            var keyEnd = Array.IndexOf(data, (byte)0, keyStart);
            if (keyEnd < 0) return null;
            if (Encoding.ASCII.GetString(data, keyStart, keyEnd - keyStart) == "DISC_VERSION")
                return Encoding.UTF8.GetString(data, valueStart, length).TrimEnd('\0');
        }
        return null;
    }
}
```

- [ ] **Step 5: Correr los tests**

Run: `dotnet test builder/Builder.Core.Tests`
Expected: `Passed!  - Failed: 0, Passed: 8`

- [ ] **Step 6: Commit**

```bash
git add builder/VCSNX.Builder.sln builder/Builder.Core builder/Builder.Core.Tests
git commit -m "feat(builder): find and check the player's copy of the game

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: Código traducido, toolchain y kit en C# (B3)

**Files:**
- Create: `builder/Builder.Core/Corpus.cs`, `builder/Builder.Core/Toolchain.cs`, `builder/Builder.Core/Kit.cs`
- Test: `builder/Builder.Core.Tests/CorpusTests.cs`, `builder/Builder.Core.Tests/ToolchainKitTests.cs`

**Interfaces:**
- Consumes: `HashList`, `TestFiles` (Task 7).
- Produces: `Corpus(string generatedDir, IReadOnlyDictionary<string,string> expected)` con `GeneratedDir`, `Problems()`, `ExtractFrom(string)`, `static Task DownloadAsync(HttpClient, Uri, string, CancellationToken)`, `Commit`, `ArchiveUri`, `ArchivePrefix`.
- Produces: `record Toolchain(string Root)` con `Gxx`, `PathIsUsable`.
- Produces: `record KitInfo(string Version, string Gcc, string Abi)`; `Kit(string root)` con `Root`, `Info`, `GeneratedOrder`, `CompileTemplate`, `ModuleLinkTemplate`, `RuntimeNro`, `Ini`, `CompileArgs(Toolchain, string generatedDir, string source, string output)`, `ModuleLinkArgs(Toolchain, IReadOnlyList<string> generatedObjects, string output)`.
- Produces (tests): `ToolchainKitTests.MakeKit(string dir, string[] compile, string[] link, string[] order) -> Kit`, `CorpusTests.MakeZip(string dir, Dictionary<string,string> files) -> string`.

- [ ] **Step 1: Escribir los tests que fallan**

`builder/Builder.Core.Tests/CorpusTests.cs`:
```csharp
using System.IO.Compression;
using System.Net;
using System.Security.Cryptography;
using System.Text;

namespace Vcsnx.Builder.Core.Tests;

public sealed class CorpusTests
{
    static string Sha(string text) => Convert.ToHexStringLower(SHA256.HashData(Encoding.UTF8.GetBytes(text)));

    static readonly Dictionary<string, string> Expected = new()
    {
        ["generated_unit_0000.cpp"] = Sha("unit 0"),
        ["generated_units.hpp"] = Sha("header"),
    };

    public static string MakeZip(string dir, Dictionary<string, string> files)
    {
        var zipPath = Path.Combine(dir, "pspr.zip");
        using var zip = ZipFile.Open(zipPath, ZipArchiveMode.Create);
        foreach (var (name, text) in files)
        {
            using var writer = new StreamWriter(zip.CreateEntry(name).Open());
            writer.Write(text);
        }
        return zipPath;
    }

    static string GoodZip(string dir) => MakeZip(dir, new()
    {
        [Corpus.ArchivePrefix + "generated_unit_0000.cpp"] = "unit 0",
        [Corpus.ArchivePrefix + "generated_units.hpp"] = "header",
        [$"PSPRecomp-{Corpus.Commit}/README.md"] = "not part of generated/",
    });

    [Fact]
    public void Extracting_the_archive_gives_exactly_the_expected_files()
    {
        var dir = TestFiles.TempDir();
        var corpus = new Corpus(Path.Combine(dir, "generated"), Expected);
        Assert.Equal(2, corpus.Problems().Count);
        corpus.ExtractFrom(GoodZip(dir));
        Assert.Empty(corpus.Problems());
        Assert.False(File.Exists(Path.Combine(dir, "generated", "README.md")));
    }

    [Fact]
    public void A_changed_file_is_a_problem()
    {
        var dir = TestFiles.TempDir();
        var corpus = new Corpus(Path.Combine(dir, "generated"), Expected);
        corpus.ExtractFrom(GoodZip(dir));
        File.WriteAllText(Path.Combine(dir, "generated", "generated_units.hpp"), "tampered");
        Assert.Equal(new[] { "generated_units.hpp" }, corpus.Problems());
    }

    sealed class Serve(HttpStatusCode status, byte[] body) : HttpMessageHandler
    {
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken ct) =>
            Task.FromResult(new HttpResponseMessage(status) { Content = new ByteArrayContent(body) });
    }

    [Fact]
    public async Task A_download_lands_whole_or_not_at_all()
    {
        var dir = TestFiles.TempDir();
        var target = Path.Combine(dir, "pspr.zip");
        await Corpus.DownloadAsync(new HttpClient(new Serve(HttpStatusCode.OK, [1, 2, 3])), Corpus.ArchiveUri, target, default);
        Assert.Equal(new byte[] { 1, 2, 3 }, File.ReadAllBytes(target));
        Assert.False(File.Exists(target + ".part"));
        var missing = Path.Combine(dir, "missing.zip");
        await Assert.ThrowsAsync<HttpRequestException>(() =>
            Corpus.DownloadAsync(new HttpClient(new Serve(HttpStatusCode.NotFound, [])), Corpus.ArchiveUri, missing, default));
        Assert.False(File.Exists(missing));
    }
}
```

`builder/Builder.Core.Tests/ToolchainKitTests.cs`:
```csharp
using System.Text.Json;

namespace Vcsnx.Builder.Core.Tests;

public sealed class ToolchainKitTests
{
    public static Kit MakeKit(string dir, string[] compile, string[] link, string[] order)
    {
        Directory.CreateDirectory(dir);
        File.WriteAllText(Path.Combine(dir, "kit.json"), $$"""{"version": "1.0.0", "gcc": "16.1.0", "abi": "{{new string('a', 64)}}"}""");
        File.WriteAllText(Path.Combine(dir, "compile.json"), JsonSerializer.Serialize(compile));
        File.WriteAllText(Path.Combine(dir, "module_link.json"), JsonSerializer.Serialize(new Dictionary<string, object> { ["args"] = link, ["generated_order"] = order }));
        File.WriteAllText(Path.Combine(dir, "VCSNative.ini"), "[Display]\n");
        File.WriteAllBytes(Path.Combine(dir, "vcsnx.nro"), TestFiles.Nro("default icon"u8.ToArray(), "nacp"u8.ToArray()));
        return new Kit(dir);
    }

    [Theory]
    [InlineData(@"C:\VCSNX-Builder\toolchain", true)]
    [InlineData(@"C:\Program Files\VCSNX\toolchain", true)]
    [InlineData(@"C:\Users\José\VCSNX\toolchain", false)]
    public void The_toolchain_folder_must_be_plain_ASCII(string root, bool usable) =>
        Assert.Equal(usable, new Toolchain(root).PathIsUsable);

    [Fact]
    public void The_compiler_is_inside_the_toolchain()
    {
        var toolchain = new Toolchain(@"C:\VCSNX\toolchain");
        Assert.Contains("aarch64-none-elf-g++", toolchain.Gxx);
        Assert.StartsWith(@"C:\VCSNX\toolchain", toolchain.Gxx);
    }

    [Fact]
    public void The_kit_fills_in_its_placeholders()
    {
        var dir = Path.Combine(TestFiles.TempDir(), "kit");
        var kit = MakeKit(dir, ["-I{generated}", "-isystem", "{toolchain}/libnx/include", "-I{kit}/include/host", "-o", "{output}", "-c", "{source}"],
            ["-shared", "{generated_objects}", "{kit}/obj/module_entry.o", "-lgcc", "-o", "{output}"],
            ["generated_registry", "generated_unit_0000"]);
        var toolchain = new Toolchain("/tc");
        var kitPath = dir.Replace('\\', '/');
        Assert.Equal(new[] { "-I/work/generated", "-isystem", "/tc/libnx/include", $"-I{kitPath}/include/host", "-o", "/work/g/a.o", "-c", "/work/generated/a.cpp" },
            kit.CompileArgs(toolchain, "/work/generated", "/work/generated/a.cpp", "/work/g/a.o"));
        Assert.Equal(new[] { "-shared", "/w/g/r.o", "/w/g/u.o", $"{kitPath}/obj/module_entry.o", "-lgcc", "-o", "/w/vcs_game.so" },
            kit.ModuleLinkArgs(toolchain, ["/w/g/r.o", "/w/g/u.o"], "/w/vcs_game.so"));
        Assert.Equal(new[] { "generated_registry", "generated_unit_0000" }, kit.GeneratedOrder);
        Assert.Equal("1.0.0", kit.Info.Version);
        Assert.Equal(new string('a', 64), kit.Info.Abi);
        Assert.Equal("{toolchain}/libnx/include", kit.CompileTemplate[2]);
        Assert.True(File.Exists(kit.RuntimeNro));
    }
}
```

- [ ] **Step 2: Correrlos y ver que fallan**

Run: `dotnet test builder/Builder.Core.Tests`
Expected: error de compilación `The type or namespace name 'Corpus' could not be found` (y `Toolchain`, `Kit`).

- [ ] **Step 3: Implementar**

`builder/Builder.Core/Corpus.cs`:
```csharp
using System.IO.Compression;

namespace Vcsnx.Builder.Core;

/// <summary>The game's translated code, as PSPRecomp publishes it: downloaded on the player's PC, checked
/// file by file against builder/hashes/generated.sha256, never shipped with the Builder.</summary>
public sealed class Corpus(string generatedDir, IReadOnlyDictionary<string, string> expected)
{
    public const string Commit = "f6e7d415c7f447b934cc3865a31eb725f353d659";
    public static readonly Uri ArchiveUri = new($"https://codeload.github.com/jessicanataliagta/PSPRecomp/zip/{Commit}");
    public const string ArchivePrefix = $"PSPRecomp-{Commit}/profiles/vcs/generated/";

    public string GeneratedDir => generatedDir;

    /// <summary>The expected files that are missing or differ, by name.</summary>
    public IReadOnlyList<string> Problems() =>
        expected.Where(kv =>
            {
                var file = Path.Combine(generatedDir, kv.Key);
                return !File.Exists(file) || HashList.Sha256Of(file) != kv.Value;
            })
            .Select(kv => kv.Key).Order(StringComparer.Ordinal).ToList();

    public void ExtractFrom(string zipPath)
    {
        Directory.CreateDirectory(generatedDir);
        using var zip = ZipFile.OpenRead(zipPath);
        foreach (var entry in zip.Entries)
        {
            if (!entry.FullName.StartsWith(ArchivePrefix, StringComparison.Ordinal)) continue;
            var name = entry.FullName[ArchivePrefix.Length..];
            if (name.Length == 0 || name.Contains('/') || !expected.ContainsKey(name)) continue;
            entry.ExtractToFile(Path.Combine(generatedDir, name), overwrite: true);
        }
    }

    public static async Task DownloadAsync(HttpClient http, Uri uri, string destination, CancellationToken ct)
    {
        var partial = destination + ".part";
        using (var response = await http.GetAsync(uri, HttpCompletionOption.ResponseHeadersRead, ct))
        {
            response.EnsureSuccessStatusCode();
            await using var file = File.Create(partial);
            await response.Content.CopyToAsync(file, ct);
        }
        File.Move(partial, destination, overwrite: true);
    }
}
```

`builder/Builder.Core/Toolchain.cs`:
```csharp
namespace Vcsnx.Builder.Core;

/// <summary>devkitPro's toolchain folder (its opt/devkitpro tree): /opt/devkitpro on the Mac, toolchain\ in the Builder.</summary>
public sealed record Toolchain(string Root)
{
    public string Gxx => Path.Combine(Root, Path.Combine("devkitA64", "bin", "aarch64-none-elf-g++")) + (OperatingSystem.IsWindows() ? ".exe" : "");

    /// <summary>devkitPro's Windows tools take only ASCII paths (spaces are fine: arguments are passed one by one).</summary>
    public bool PathIsUsable => Root.All(c => c >= ' ' && c < 127);
}
```

`builder/Builder.Core/Kit.cs`:
```csharp
using System.Text.Json;

namespace Vcsnx.Builder.Core;

public sealed record KitInfo(string Version, string Gcc, string Abi);

/// <summary>The kit tools/make_builder_kit.py builds: the compile line of the translated code and the link line of
/// vcs_game.so, with placeholders {kit} {toolchain} {generated} {source} {output} {generated_objects}; the module's
/// own entry objects; the runtime vcsnx.nro; VCSNative.ini.</summary>
public sealed class Kit
{
    static readonly JsonSerializerOptions Json = new() { PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower };
    sealed record LinkFile(string[] Args, string[] GeneratedOrder);

    readonly string[] _compile;
    readonly string[] _link;

    public Kit(string root)
    {
        Root = Path.GetFullPath(root);
        Info = JsonSerializer.Deserialize<KitInfo>(File.ReadAllText(Path.Combine(Root, "kit.json")), Json)
               ?? throw new InvalidDataException("kit.json is empty");
        _compile = JsonSerializer.Deserialize<string[]>(File.ReadAllText(Path.Combine(Root, "compile.json")))
                   ?? throw new InvalidDataException("compile.json is empty");
        var link = JsonSerializer.Deserialize<LinkFile>(File.ReadAllText(Path.Combine(Root, "module_link.json")), Json)
                   ?? throw new InvalidDataException("module_link.json is empty");
        _link = link.Args;
        GeneratedOrder = link.GeneratedOrder;
    }

    public string Root { get; }
    public KitInfo Info { get; }
    /// <summary>The translated sources' names without .cpp, in the original link order.</summary>
    public IReadOnlyList<string> GeneratedOrder { get; }
    /// <summary>compile.json as written (what an object's fingerprint depends on).</summary>
    public IReadOnlyList<string> CompileTemplate => _compile;
    /// <summary>module_link.json's arguments as written (what the module's fingerprint depends on).</summary>
    public IReadOnlyList<string> ModuleLinkTemplate => _link;
    public string RuntimeNro => Path.Combine(Root, "vcsnx.nro");
    public string Ini => Path.Combine(Root, "VCSNative.ini");

    public IReadOnlyList<string> CompileArgs(Toolchain toolchain, string generatedDir, string source, string output) =>
        _compile.Select(a => Fill(a, toolchain, generatedDir).Replace("{source}", Fwd(source)).Replace("{output}", Fwd(output))).ToList();

    public IReadOnlyList<string> ModuleLinkArgs(Toolchain toolchain, IReadOnlyList<string> generatedObjects, string output) =>
        _link.SelectMany(a => a == "{generated_objects}"
            ? generatedObjects.Select(Fwd)
            : new[] { Fill(a, toolchain, "").Replace("{output}", Fwd(output)) }).ToList();

    string Fill(string arg, Toolchain toolchain, string generatedDir) =>
        arg.Replace("{kit}", Fwd(Root)).Replace("{toolchain}", Fwd(toolchain.Root)).Replace("{generated}", Fwd(generatedDir));

    static string Fwd(string path) => path.Replace('\\', '/');
}
```

- [ ] **Step 4: Correr los tests**

Run: `dotnet test builder/Builder.Core.Tests`
Expected: `Passed!  - Failed: 0, Passed: 16`

- [ ] **Step 5: Commit**

```bash
git add builder/Builder.Core builder/Builder.Core.Tests
git commit -m "feat(builder): translated code download, toolchain and kit

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 9: Compilar, enlazar el módulo y el ícono del `.nro` (B3)

**Files:**
- Create: `builder/Builder.Core/Processes.cs`, `builder/Builder.Core/Compiler.cs`, `builder/Builder.Core/ModuleLinker.cs`, `builder/Builder.Core/NroIcon.cs`, `builder/Builder.Core/IconMaker.cs`
- Test: `builder/Builder.Core.Tests/FakeRunner.cs`, `builder/Builder.Core.Tests/CompilerLinkerTests.cs`, `builder/Builder.Core.Tests/IconTests.cs`

**Interfaces:**
- Consumes: `Toolchain`, `Kit`, `HashList`, `BuilderException` (Tasks 7–8), `TestFiles.Nro`, `ToolchainKitTests.MakeKit`.
- Produces: `record ProcessResult(int ExitCode, string Output)`; `interface IProcessRunner { Task<ProcessResult> RunAsync(string file, IReadOnlyList<string> args, CancellationToken ct); }`; `SystemProcessRunner`; `LoggingRunner(IProcessRunner inner, TextWriter log)`.
- Produces: `JobPlanner.Jobs(int cores, long memoryBytes)`; `Compiler(Toolchain, Kit, string generatedDir, string objectDir, IProcessRunner)` con `ObjectFor`, `SourceFor`, `Fingerprint`, `UpToDate`, `CompileAllAsync(IReadOnlyList<string> stems, int jobs, IProgress<int>?, CancellationToken)`; `CompileFailedException` (`Unit`, `Output`).
- Produces: `ModuleLinker(Toolchain, Kit, IProcessRunner)` con `UpToDate(string module, IReadOnlyList<string> objects)`, `LinkAsync(IReadOnlyList<string> objects, string module, CancellationToken)`; `ToolFailedException` (`Tool`, `Output`).
- Produces: `NroIcon.Replace(string nro, string jpg, string output)`, `NroIcon.ReadIcon(string nro) -> byte[]`; `IconMaker.Size`, `IconMaker.Make(string image, string jpg)`.
- Produces (tests): `FakeRunner` (`Calls`, `FailWhen`, `MaxConcurrent`), `SyncProgress<T>`.

- [ ] **Step 1: Escribir los tests que fallan**

`builder/Builder.Core.Tests/FakeRunner.cs`:
```csharp
namespace Vcsnx.Builder.Core.Tests;

/// <summary>Stands in for the compiler: writes the file after -o.</summary>
internal sealed class FakeRunner : IProcessRunner
{
    readonly object _lock = new();
    int _running;
    public List<(string File, IReadOnlyList<string> Args)> Calls { get; } = [];
    public Func<IReadOnlyList<string>, bool> FailWhen { get; set; } = _ => false;
    public int MaxConcurrent { get; private set; }

    public async Task<ProcessResult> RunAsync(string file, IReadOnlyList<string> args, CancellationToken ct)
    {
        lock (_lock)
        {
            Calls.Add((file, args));
            MaxConcurrent = Math.Max(MaxConcurrent, ++_running);
        }
        try
        {
            await Task.Delay(5, ct);
            if (FailWhen(args)) return new ProcessResult(1, "error: boom");
            File.WriteAllText(args[args.ToList().IndexOf("-o") + 1], "out " + string.Join(' ', args));
            return new ProcessResult(0, "");
        }
        finally
        {
            lock (_lock) --_running;
        }
    }
}

/// <summary>An IProgress that reports on the calling thread.</summary>
internal sealed class SyncProgress<T>(Action<T> report) : IProgress<T>
{
    readonly object _lock = new();
    public void Report(T value) { lock (_lock) report(value); }
}
```

`builder/Builder.Core.Tests/CompilerLinkerTests.cs`:
```csharp
namespace Vcsnx.Builder.Core.Tests;

public sealed class CompilerLinkerTests
{
    static (Compiler Compiler, FakeRunner Runner, string Generated, string Objects, Kit Kit, Toolchain Toolchain) Setup(params string[] stems)
    {
        var dir = TestFiles.TempDir();
        var generated = Path.Combine(dir, "generated");
        foreach (var stem in stems) TestFiles.Write(Path.Combine(generated, stem + ".cpp"), System.Text.Encoding.UTF8.GetBytes(stem));
        var kit = ToolchainKitTests.MakeKit(Path.Combine(dir, "kit"), ["-O2", "-o", "{output}", "-c", "{source}"],
            ["-shared", "{generated_objects}", "-o", "{output}"], stems);
        var runner = new FakeRunner();
        var toolchain = new Toolchain(Path.Combine(dir, "tc"));
        var objects = Path.Combine(dir, "g");
        return (new Compiler(toolchain, kit, generated, objects, runner), runner, generated, objects, kit, toolchain);
    }

    [Theory]
    [InlineData(8, 8L << 30, 3)]
    [InlineData(8, 32L << 30, 8)]
    [InlineData(4, 16L << 30, 4)]
    [InlineData(16, 4L << 30, 1)]
    [InlineData(8, 1L << 30, 1)]
    public void Parallel_compiles_follow_cores_and_memory(int cores, long memory, int jobs) =>
        Assert.Equal(jobs, JobPlanner.Jobs(cores, memory));

    [Fact]
    public async Task Every_unit_is_compiled_and_objects_come_back_in_link_order()
    {
        var (compiler, runner, _, objects, _, _) = Setup("generated_registry", "generated_unit_0000", "generated_unit_0001");
        var result = await compiler.CompileAllAsync(["generated_registry", "generated_unit_0000", "generated_unit_0001"], 2, null, default);
        Assert.Equal(new[] { Path.Combine(objects, "generated_registry.o"), Path.Combine(objects, "generated_unit_0000.o"), Path.Combine(objects, "generated_unit_0001.o") }, result);
        Assert.Equal(3, runner.Calls.Count);
        Assert.True(runner.MaxConcurrent <= 2);
    }

    [Fact]
    public async Task A_second_run_compiles_nothing_and_a_changed_source_is_recompiled()
    {
        var (compiler, runner, generated, _, _, _) = Setup("a", "b");
        await compiler.CompileAllAsync(["a", "b"], 2, null, default);
        runner.Calls.Clear();
        await compiler.CompileAllAsync(["a", "b"], 2, null, default);
        Assert.Empty(runner.Calls);
        File.WriteAllText(Path.Combine(generated, "b.cpp"), "changed");
        await compiler.CompileAllAsync(["a", "b"], 2, null, default);
        Assert.Single(runner.Calls);
    }

    [Fact]
    public async Task A_half_written_object_from_an_interrupted_run_is_never_reused()
    {
        var (compiler, runner, _, objects, _, _) = Setup("a");
        TestFiles.Write(Path.Combine(objects, "a.o"), [1, 2]);   // no .key: the run died before it finished
        TestFiles.Write(Path.Combine(objects, "a.o.tmp"), [3]);
        Assert.False(compiler.UpToDate("a"));
        await compiler.CompileAllAsync(["a"], 1, null, default);
        Assert.Single(runner.Calls);
        Assert.True(compiler.UpToDate("a"));
    }

    [Fact]
    public async Task A_failing_unit_stops_the_build_and_leaves_no_object()
    {
        var (compiler, runner, _, objects, _, _) = Setup("a", "b", "c");
        runner.FailWhen = args => args.Any(a => a.EndsWith("b.cpp"));
        var error = await Assert.ThrowsAsync<CompileFailedException>(() => compiler.CompileAllAsync(["a", "b", "c"], 1, null, default));
        Assert.Equal("b", error.Unit);
        Assert.Contains("boom", error.Output);
        Assert.False(File.Exists(Path.Combine(objects, "b.o")));
    }

    [Fact]
    public async Task Progress_counts_every_unit_including_the_cached_ones()
    {
        var (compiler, _, _, _, _, _) = Setup("a", "b");
        await compiler.CompileAllAsync(["a"], 1, null, default);
        var seen = new List<int>();
        await compiler.CompileAllAsync(["a", "b"], 1, new SyncProgress<int>(seen.Add), default);
        Assert.Equal(new[] { 1, 2 }, seen.Order());
    }

    [Fact]
    public async Task The_module_is_linked_once_and_again_only_when_an_object_changes()
    {
        var (compiler, runner, generated, objects, kit, toolchain) = Setup("a", "b");
        var objectFiles = await compiler.CompileAllAsync(["a", "b"], 1, null, default);
        var linker = new ModuleLinker(toolchain, kit, runner);
        var module = Path.Combine(Path.GetDirectoryName(objects)!, "salida", "vcs_game.so");
        Directory.CreateDirectory(Path.GetDirectoryName(module)!);
        runner.Calls.Clear();
        await linker.LinkAsync(objectFiles, module, default);
        Assert.Single(runner.Calls);
        Assert.Contains("-shared", runner.Calls[0].Args);
        Assert.True(File.Exists(module));
        await linker.LinkAsync(objectFiles, module, default);
        Assert.Single(runner.Calls);
        File.WriteAllText(Path.Combine(generated, "a.cpp"), "changed");
        await compiler.CompileAllAsync(["a", "b"], 1, null, default);
        await linker.LinkAsync(objectFiles, module, default);
        Assert.Equal(3, runner.Calls.Count); // the first link, compiling a again, linking again
        runner.FailWhen = args => args.Contains("-shared");
        File.Delete(module + ".key");
        await Assert.ThrowsAsync<ToolFailedException>(() => linker.LinkAsync(objectFiles, module, default));
    }
}
```

`builder/Builder.Core.Tests/IconTests.cs`:
```csharp
using SkiaSharp;

namespace Vcsnx.Builder.Core.Tests;

public sealed class IconTests
{
    [Fact]
    public void The_icon_is_the_centre_square_at_256_pixels()
    {
        var dir = TestFiles.TempDir();
        var image = Path.Combine(dir, "art.png");
        using (var bitmap = new SKBitmap(400, 200))
        {
            for (var x = 0; x < 400; ++x)
                for (var y = 0; y < 200; ++y)
                    bitmap.SetPixel(x, y, x < 200 ? SKColors.Red : SKColors.Blue);
            using var data = bitmap.Encode(SKEncodedImageFormat.Png, 100);
            File.WriteAllBytes(image, data.ToArray());
        }
        var jpg = Path.Combine(dir, "icon.jpg");
        IconMaker.Make(image, jpg);
        using var icon = SKBitmap.Decode(jpg);
        Assert.Equal((256, 256), (icon.Width, icon.Height));
        Assert.True(icon.GetPixel(64, 128).Red > 200 && icon.GetPixel(64, 128).Blue < 60, "left half of the centre square is red");
        Assert.True(icon.GetPixel(192, 128).Blue > 200 && icon.GetPixel(192, 128).Red < 60, "right half is blue");
    }

    [Fact]
    public void An_unreadable_image_is_a_player_message()
    {
        var dir = TestFiles.TempDir();
        var bad = TestFiles.Write(Path.Combine(dir, "bad.png"), [1, 2, 3]);
        Assert.Equal("IconUnreadable", Assert.Throws<BuilderException>(() => IconMaker.Make(bad, Path.Combine(dir, "i.jpg"))).Key);
    }

    [Fact]
    public void Replacing_the_nro_icon_keeps_the_program_and_the_nacp()
    {
        var dir = TestFiles.TempDir();
        var original = TestFiles.Nro("old icon"u8.ToArray(), "the nacp"u8.ToArray());
        var nro = TestFiles.Write(Path.Combine(dir, "vcsnx.nro"), original);
        var jpg = TestFiles.Write(Path.Combine(dir, "icon.jpg"), "a much longer new icon"u8.ToArray());
        var output = Path.Combine(dir, "out.nro");
        NroIcon.Replace(nro, jpg, output);
        var result = File.ReadAllBytes(output);
        Assert.Equal(original.AsSpan(0, 0x100).ToArray(), result.AsSpan(0, 0x100).ToArray());
        Assert.Equal("a much longer new icon"u8.ToArray(), NroIcon.ReadIcon(output));
        Assert.EndsWith("the nacp", System.Text.Encoding.ASCII.GetString(result));
        Assert.Throws<InvalidDataException>(() => NroIcon.Replace(jpg, jpg, output));
    }
}
```

- [ ] **Step 2: Correrlos y ver que fallan**

Run: `dotnet test builder/Builder.Core.Tests`
Expected: error de compilación `The type or namespace name 'IProcessRunner' could not be found` (y `Compiler`, `ModuleLinker`, `NroIcon`, `IconMaker`).

- [ ] **Step 3: Implementar**

`builder/Builder.Core/Processes.cs`:
```csharp
using System.Diagnostics;

namespace Vcsnx.Builder.Core;

public sealed record ProcessResult(int ExitCode, string Output);

public interface IProcessRunner
{
    Task<ProcessResult> RunAsync(string file, IReadOnlyList<string> args, CancellationToken ct);
}

public sealed class SystemProcessRunner : IProcessRunner
{
    public async Task<ProcessResult> RunAsync(string file, IReadOnlyList<string> args, CancellationToken ct)
    {
        var start = new ProcessStartInfo(file)
        {
            RedirectStandardOutput = true, RedirectStandardError = true, UseShellExecute = false, CreateNoWindow = true,
        };
        foreach (var arg in args) start.ArgumentList.Add(arg);
        using var process = Process.Start(start) ?? throw new InvalidOperationException($"cannot start {file}");
        var stdout = process.StandardOutput.ReadToEndAsync(CancellationToken.None);
        var stderr = process.StandardError.ReadToEndAsync(CancellationToken.None);
        try
        {
            await process.WaitForExitAsync(ct);
        }
        catch (OperationCanceledException)
        {
            try { process.Kill(entireProcessTree: true); } catch (InvalidOperationException) { }
            throw;
        }
        return new ProcessResult(process.ExitCode, await stdout + await stderr);
    }
}

/// <summary>Writes every command and what it printed to the build log (salida\builder.log).</summary>
public sealed class LoggingRunner(IProcessRunner inner, TextWriter log) : IProcessRunner
{
    public async Task<ProcessResult> RunAsync(string file, IReadOnlyList<string> args, CancellationToken ct)
    {
        var result = await inner.RunAsync(file, args, ct);
        lock (log)
        {
            log.WriteLine($"$ {file} {string.Join(' ', args)}");
            if (result.Output.Length > 0) log.WriteLine(result.Output.TrimEnd());
            if (result.ExitCode != 0) log.WriteLine($"(exit code {result.ExitCode})");
            log.Flush();
        }
        return result;
    }
}
```

`builder/Builder.Core/Compiler.cs`:
```csharp
using System.Security.Cryptography;
using System.Text;

namespace Vcsnx.Builder.Core;

public static class JobPlanner
{
    const long PerJob = 2L << 30; // GCC at -O2 on the biggest translated units, with room for Windows

    /// <summary>One compile per core, but no more than one per 2 GB of RAM after 2 GB for everything else.</summary>
    public static int Jobs(int cores, long memoryBytes) =>
        Math.Clamp((int)Math.Min(cores, (memoryBytes - PerJob) / PerJob), 1, Math.Max(1, cores));
}

public sealed class CompileFailedException(string unit, string output) : Exception($"compiling {unit} failed:\n{output}")
{
    public string Unit { get; } = unit;
    public string Output { get; } = output;
}

/// <summary>Compiles the translated units into trabajo\g\. An object is written to .tmp and renamed, and its
/// .key (source hash + compile template) is written last, so an interrupted run never reuses half an object.</summary>
public sealed class Compiler(Toolchain toolchain, Kit kit, string generatedDir, string objectDir, IProcessRunner runner)
{
    public string ObjectFor(string stem) => Path.Combine(objectDir, stem + ".o");
    public string SourceFor(string stem) => Path.Combine(generatedDir, stem + ".cpp");

    public string Fingerprint(string source)
    {
        var text = HashList.Sha256Of(source) + "\n" + string.Join("\n", kit.CompileTemplate);
        return Convert.ToHexStringLower(SHA256.HashData(Encoding.UTF8.GetBytes(text)));
    }

    public bool UpToDate(string stem)
    {
        var obj = ObjectFor(stem);
        var key = obj + ".key";
        return File.Exists(obj) && File.Exists(key) && File.ReadAllText(key) == Fingerprint(SourceFor(stem));
    }

    public async Task<IReadOnlyList<string>> CompileAllAsync(IReadOnlyList<string> stems, int jobs, IProgress<int>? done, CancellationToken ct)
    {
        Directory.CreateDirectory(objectDir);
        var finished = 0;
        using var gate = new SemaphoreSlim(Math.Max(1, jobs));
        using var stop = CancellationTokenSource.CreateLinkedTokenSource(ct);

        async Task One(string stem)
        {
            await gate.WaitAsync(stop.Token);
            try
            {
                if (!UpToDate(stem))
                {
                    var obj = ObjectFor(stem);
                    var tmp = obj + ".tmp";
                    File.Delete(obj + ".key");
                    var result = await runner.RunAsync(toolchain.Gxx, kit.CompileArgs(toolchain, generatedDir, SourceFor(stem), tmp), stop.Token);
                    if (result.ExitCode != 0)
                    {
                        File.Delete(tmp);
                        stop.Cancel();
                        throw new CompileFailedException(stem, result.Output);
                    }
                    File.Move(tmp, obj, overwrite: true);
                    File.WriteAllText(obj + ".key", Fingerprint(SourceFor(stem)));
                }
                done?.Report(Interlocked.Increment(ref finished));
            }
            finally
            {
                gate.Release();
            }
        }

        var tasks = stems.Select(One).ToList();
        try
        {
            await Task.WhenAll(tasks);
        }
        catch when (tasks.Any(t => t.Exception?.InnerException is CompileFailedException))
        {
            throw tasks.Select(t => t.Exception?.InnerException).OfType<CompileFailedException>().First();
        }
        return stems.Select(ObjectFor).ToList();
    }
}
```

`builder/Builder.Core/ModuleLinker.cs`:
```csharp
using System.Security.Cryptography;
using System.Text;

namespace Vcsnx.Builder.Core;

public sealed class ToolFailedException(string tool, string output) : Exception($"{tool} failed:\n{output}")
{
    public string Tool { get; } = tool;
    public string Output { get; } = output;
}

/// <summary>Links vcs_game.so from the compiled translated code and the kit's entry objects. Like the objects,
/// the module carries a .key (the objects' keys + the link template) and is relinked only when that changes.</summary>
public sealed class ModuleLinker(Toolchain toolchain, Kit kit, IProcessRunner runner)
{
    string Fingerprint(IReadOnlyList<string> objects)
    {
        var text = string.Join("\n", objects.Select(o => File.Exists(o + ".key") ? File.ReadAllText(o + ".key") : "")) + "\n" + string.Join("\n", kit.ModuleLinkTemplate);
        return Convert.ToHexStringLower(SHA256.HashData(Encoding.UTF8.GetBytes(text)));
    }

    public bool UpToDate(string module, IReadOnlyList<string> objects) =>
        File.Exists(module) && File.Exists(module + ".key") && File.ReadAllText(module + ".key") == Fingerprint(objects);

    public async Task LinkAsync(IReadOnlyList<string> objects, string module, CancellationToken ct)
    {
        if (UpToDate(module, objects)) return;
        File.Delete(module + ".key");
        var tmp = module + ".tmp";
        var result = await runner.RunAsync(toolchain.Gxx, kit.ModuleLinkArgs(toolchain, objects, tmp), ct);
        if (result.ExitCode != 0)
        {
            File.Delete(tmp);
            throw new ToolFailedException("link", result.Output);
        }
        File.Move(tmp, module, overwrite: true);
        File.WriteAllText(module + ".key", Fingerprint(objects));
    }
}
```

`builder/Builder.Core/NroIcon.cs`:
```csharp
namespace Vcsnx.Builder.Core;

/// <summary>Puts the player's icon into vcsnx.nro. An NRO is the program (its header at 0x10 says "NRO0", and the
/// word at 0x18 is the program's size) followed by its asset section: "ASET", a version, and the icon, NACP and
/// RomFS as (offset, size) pairs relative to the section. Only the asset section is rewritten.</summary>
public static class NroIcon
{
    const int AsetHeader = 0x38;

    public static void Replace(string nro, string jpg, string output)
    {
        var data = File.ReadAllBytes(nro);
        var program = ProgramSize(data, nro);
        var nacp = Asset(data, program, 1);
        var romfs = Asset(data, program, 2);
        var icon = File.ReadAllBytes(jpg);
        var header = new byte[AsetHeader];
        "ASET"u8.CopyTo(header);
        var offset = (ulong)AsetHeader;
        void Put(int index, int length)
        {
            BitConverter.TryWriteBytes(header.AsSpan(8 + index * 16), offset);
            BitConverter.TryWriteBytes(header.AsSpan(16 + index * 16), (ulong)length);
            offset += (ulong)length;
        }
        Put(0, icon.Length);
        Put(1, nacp.Length);
        Put(2, romfs.Length);
        using var file = File.Create(output);
        file.Write(data, 0, program);
        file.Write(header);
        file.Write(icon);
        file.Write(nacp);
        file.Write(romfs);
    }

    public static byte[] ReadIcon(string nro)
    {
        var data = File.ReadAllBytes(nro);
        return Asset(data, ProgramSize(data, nro), 0);
    }

    static int ProgramSize(byte[] data, string name)
    {
        if (data.Length < 0x80 || !data.AsSpan(0x10, 4).SequenceEqual("NRO0"u8)) throw new InvalidDataException($"{name} is not an NRO");
        var size = BitConverter.ToUInt32(data, 0x18);
        if ((long)size + AsetHeader > data.Length || !data.AsSpan((int)size, 4).SequenceEqual("ASET"u8))
            throw new InvalidDataException($"{name} has no asset section");
        return (int)size;
    }

    static byte[] Asset(byte[] data, int program, int index)
    {
        var offset = BitConverter.ToUInt64(data, program + 8 + index * 16);
        var size = BitConverter.ToUInt64(data, program + 16 + index * 16);
        if (size == 0) return [];
        if ((ulong)program + offset + size > (ulong)data.Length) throw new InvalidDataException("asset outside the NRO");
        return data.AsSpan(program + (int)offset, (int)size).ToArray();
    }
}
```

`builder/Builder.Core/IconMaker.cs`:
```csharp
using SkiaSharp;

namespace Vcsnx.Builder.Core;

/// <summary>The HOME-menu icon from the player's own image: the centre square, 256x256, JPEG.</summary>
public static class IconMaker
{
    public const int Size = 256;

    public static void Make(string image, string jpg)
    {
        using var source = SKBitmap.Decode(image) ?? throw new BuilderException("IconUnreadable");
        var side = Math.Min(source.Width, source.Height);
        var crop = SKRect.Create((source.Width - side) / 2f, (source.Height - side) / 2f, side, side);
        using var square = new SKBitmap(Size, Size);
        using (var canvas = new SKCanvas(square))
        using (var picture = SKImage.FromBitmap(source))
        {
            canvas.Clear(SKColors.Black);
            canvas.DrawImage(picture, crop, SKRect.Create(0, 0, Size, Size), new SKSamplingOptions(SKCubicResampler.Mitchell), null);
        }
        using var data = square.Encode(SKEncodedImageFormat.Jpeg, 92);
        using var file = File.Create(jpg);
        data.SaveTo(file);
    }
}
```

- [ ] **Step 4: Correr los tests**

Run: `dotnet test builder/Builder.Core.Tests`
Expected: `Passed!  - Failed: 0, Passed: 30`

- [ ] **Step 5: Commit**

```bash
git add builder/Builder.Core builder/Builder.Core.Tests
git commit -m "feat(builder): resumable parallel compile, module link and the .nro icon

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 10: SD, pipeline y consola (B3/B4)

**Files:**
- Create: `builder/Builder.Core/SdCard.cs`, `builder/Builder.Core/Eta.cs`, `builder/Builder.Core/Pipeline.cs`
- Create: `builder/Builder.Cli/` (vía `dotnet new console`), `builder/Builder.Cli/Program.cs`
- Test: `builder/Builder.Core.Tests/SdCardTests.cs`, `builder/Builder.Core.Tests/PipelineTests.cs`

**Interfaces:**
- Consumes: todo lo anterior; `CorpusTests.MakeZip`, `ToolchainKitTests.MakeKit`, `FakeRunner`, `SyncProgress` (tests).
- Produces: `record SdTarget(string Root, string Label)`, `record CopyReport(int Copied, int Skipped)`, `SdCard.Candidates()`, `SdCard.GameFolder(string)`, `SdCard.Install(string sdRoot, string nro, string module, string ini, string dataRoot, IProgress<string>?, CancellationToken) -> CopyReport`, `SdCard.Same(string, string)`.
- Produces: `Eta.Remaining(int done, int total, int cached, TimeSpan elapsed) -> TimeSpan?`.
- Produces: `enum Stage { Checking, Downloading, Compiling, Linking, Packaging, CopyingToSd, Done }`, `record BuildProgress(Stage Stage, int Done, int Total, TimeSpan? Remaining)`, `record BuildRequest(string GameFolder, string? IconImage, string? SdRoot, string? CorpusZip = null)`, `record BuildPaths(string Toolchain, string Kit, string Hashes, string Work, string Output)` con `NextTo(string)`, `record BuildResult(string Module, string Nro, CopyReport? Sd)`, `Pipeline(BuildPaths, HttpClient, IProcessRunner, int cores, long memoryBytes, Func<string,long> freeSpace)` con `RunAsync(BuildRequest, IProgress<BuildProgress>, TextWriter, CancellationToken) -> Task<BuildResult>` y `RequiredSpace`.
- Produces: `Builder.Cli --game <PSP_DATA> [--icon <img>] [--sd <root>] [--corpus-zip <zip>] [--base <dir>] [--toolchain <dir>] [--kit <dir>] [--hashes <dir>] [--work <dir>] [--out <dir>]`.

- [ ] **Step 1: Escribir los tests que fallan**

`builder/Builder.Core.Tests/SdCardTests.cs`:
```csharp
namespace Vcsnx.Builder.Core.Tests;

public sealed class SdCardTests
{
    static (string Sd, string Nro, string Module, string Ini, string Data) Setup()
    {
        var dir = TestFiles.TempDir();
        var sd = Path.Combine(dir, "sd");
        Directory.CreateDirectory(Path.Combine(sd, "switch"));
        var nro = TestFiles.Write(Path.Combine(dir, "salida", "vcsnx.nro"), [1, 2, 3]);
        var module = TestFiles.Write(Path.Combine(dir, "salida", "vcs_game.so"), [4, 5, 6]);
        var ini = TestFiles.Write(Path.Combine(dir, "kit", "VCSNative.ini"), "[Display]\n"u8.ToArray());
        var data = Path.Combine(dir, "PSP_DATA");
        TestFiles.Write(Path.Combine(data, "PSP_GAME", "SYSDIR", "BOOT.BIN"), TestFiles.Elf([1]));
        TestFiles.Write(Path.Combine(data, "PSP_GAME", "USRDIR", "DATA.BIN"), [5, 6, 7]);
        TestFiles.Write(Path.Combine(data, "PSP_GAME", "._DATA.BIN"), [0]);
        return (sd, nro, module, ini, data);
    }

    [Fact]
    public void Installing_puts_the_runtime_the_module_and_the_game_in_switch_vcsnx()
    {
        var (sd, nro, module, ini, data) = Setup();
        var report = SdCard.Install(sd, nro, module, ini, data, null, default);
        var game = SdCard.GameFolder(sd);
        Assert.Equal(5, report.Copied);
        Assert.True(File.Exists(Path.Combine(game, "vcsnx.nro")));
        Assert.True(File.Exists(Path.Combine(game, "vcs_game.so")));
        Assert.True(File.Exists(Path.Combine(game, "VCSNative.ini")));
        Assert.True(File.Exists(Path.Combine(game, "PSP_DATA", "PSP_GAME", "USRDIR", "DATA.BIN")));
        Assert.False(File.Exists(Path.Combine(game, "PSP_DATA", "PSP_GAME", "._DATA.BIN")));
    }

    [Fact]
    public void A_second_install_copies_nothing_and_keeps_the_player_s_settings()
    {
        var (sd, nro, module, ini, data) = Setup();
        SdCard.Install(sd, nro, module, ini, data, null, default);
        var sdIni = Path.Combine(SdCard.GameFolder(sd), "VCSNative.ini");
        File.WriteAllText(sdIni, "[Display]\nHudScale=0.9\n");
        Assert.Equal(0, SdCard.Install(sd, nro, module, ini, data, null, default).Copied);
        Assert.Contains("0.9", File.ReadAllText(sdIni));
    }

    [Fact]
    public void A_changed_file_of_the_same_size_is_copied_again()
    {
        var (sd, nro, module, ini, data) = Setup();
        SdCard.Install(sd, nro, module, ini, data, null, default);
        var source = Path.Combine(data, "PSP_GAME", "USRDIR", "DATA.BIN");
        File.WriteAllBytes(source, [9, 9, 9]);
        File.SetLastWriteTimeUtc(source, DateTime.UtcNow.AddHours(1));
        Assert.Equal(1, SdCard.Install(sd, nro, module, ini, data, null, default).Copied);
        Assert.Equal(new byte[] { 9, 9, 9 }, File.ReadAllBytes(Path.Combine(SdCard.GameFolder(sd), "PSP_DATA", "PSP_GAME", "USRDIR", "DATA.BIN")));
    }
}
```

`builder/Builder.Core.Tests/PipelineTests.cs`:
```csharp
using System.Net;
using System.Security.Cryptography;
using System.Text;

namespace Vcsnx.Builder.Core.Tests;

public sealed class PipelineTests
{
    sealed class Serve(byte[] zip) : HttpMessageHandler
    {
        public int Requests;
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken ct)
        {
            ++Requests;
            return Task.FromResult(new HttpResponseMessage(HttpStatusCode.OK) { Content = new ByteArrayContent(zip) });
        }
    }

    static string Sha(byte[] data) => Convert.ToHexStringLower(SHA256.HashData(data));

    sealed record World(BuildPaths Paths, string Game, Serve Server, FakeRunner Runner);

    static World MakeWorld(string? baseDir = null)
    {
        var dir = baseDir ?? TestFiles.TempDir();
        var paths = BuildPaths.NextTo(dir);
        Directory.CreateDirectory(paths.Toolchain);
        ToolchainKitTests.MakeKit(paths.Kit, ["-o", "{output}", "-c", "{source}"], ["-shared", "{generated_objects}", "-o", "{output}"],
            ["generated_registry", "generated_unit_0000"]);
        var files = new Dictionary<string, string> { ["generated_registry.cpp"] = "registry", ["generated_unit_0000.cpp"] = "unit 0", ["generated_units.hpp"] = "header" };
        Directory.CreateDirectory(paths.Hashes);
        File.WriteAllLines(Path.Combine(paths.Hashes, "generated.sha256"), files.Select(f => $"{Sha(Encoding.UTF8.GetBytes(f.Value))}  {f.Key}"));
        var elf = TestFiles.Elf([4, 2]);
        File.WriteAllText(Path.Combine(paths.Hashes, "executable.sha256"), $"{Sha(elf)}  BOOT.BIN\n");
        var game = Path.Combine(dir, "PSP_DATA");
        TestFiles.Write(Path.Combine(game, "PSP_GAME", "SYSDIR", "BOOT.BIN"), elf);
        var zip = CorpusTests.MakeZip(TestFiles.TempDir(), files.ToDictionary(f => Corpus.ArchivePrefix + f.Key, f => f.Value));
        return new World(paths, game, new Serve(File.ReadAllBytes(zip)), new FakeRunner());
    }

    static Pipeline PipelineFor(World w, long free = 10L << 30) =>
        new(w.Paths, new HttpClient(w.Server), w.Runner, cores: 4, memoryBytes: 16L << 30, freeSpace: _ => free);

    static Task<BuildResult> Run(World w, BuildRequest request, long free = 10L << 30) =>
        PipelineFor(w, free).RunAsync(request, new SyncProgress<BuildProgress>(_ => { }), TextWriter.Null, default);

    [Fact]
    public async Task A_first_build_downloads_compiles_links_and_packs_in_order()
    {
        var w = MakeWorld();
        var stages = new List<Stage>();
        var result = await PipelineFor(w).RunAsync(new BuildRequest(w.Game, null, null), new SyncProgress<BuildProgress>(p => stages.Add(p.Stage)), TextWriter.Null, default);
        Assert.True(File.Exists(result.Module));
        Assert.Equal(File.ReadAllBytes(Path.Combine(w.Paths.Kit, "vcsnx.nro")), File.ReadAllBytes(result.Nro));
        Assert.Equal(1, w.Server.Requests);
        Assert.Equal(new[] { Stage.Checking, Stage.Downloading, Stage.Compiling, Stage.Linking, Stage.Packaging, Stage.Done }, stages.Distinct());
        Assert.Null(result.Sd);
    }

    [Fact]
    public async Task Building_again_for_a_new_icon_or_sd_does_not_download_compile_or_link()
    {
        var w = MakeWorld();
        await Run(w, new BuildRequest(w.Game, null, null));
        w.Runner.Calls.Clear();
        var sd = Path.Combine(TestFiles.TempDir(), "sd");
        Directory.CreateDirectory(Path.Combine(sd, "switch"));
        var result = await Run(w, new BuildRequest(w.Game, null, sd));
        Assert.Equal(1, w.Server.Requests);
        Assert.Empty(w.Runner.Calls);
        Assert.True(File.Exists(Path.Combine(SdCard.GameFolder(sd), "vcs_game.so")));
        Assert.True(File.Exists(Path.Combine(SdCard.GameFolder(sd), "vcsnx.nro")));
        Assert.NotNull(result.Sd);
    }

    [Fact]
    public async Task A_folder_with_accents_is_refused_before_anything_runs_and_one_with_spaces_works()
    {
        var accented = MakeWorld(Path.Combine(TestFiles.TempDir(), "VCSNX-Bú"));
        var error = await Assert.ThrowsAsync<BuilderException>(() => Run(accented, new BuildRequest(accented.Game, null, null)));
        Assert.Equal("PathNotAscii", error.Key);
        Assert.Empty(accented.Runner.Calls);
        var spaced = MakeWorld(Path.Combine(TestFiles.TempDir(), "VCSNX Builder"));
        Assert.True(File.Exists((await Run(spaced, new BuildRequest(spaced.Game, null, null))).Module));
    }

    [Fact]
    public async Task Not_enough_space_and_a_wrong_game_are_player_messages()
    {
        var w = MakeWorld();
        Assert.Equal("NotEnoughSpace", (await Assert.ThrowsAsync<BuilderException>(() => Run(w, new BuildRequest(w.Game, null, null), free: 1L << 30))).Key);
        File.WriteAllBytes(Path.Combine(w.Game, "PSP_GAME", "SYSDIR", "BOOT.BIN"), TestFiles.Elf([0]));
        Assert.Equal("GameUnknown", (await Assert.ThrowsAsync<BuilderException>(() => Run(w, new BuildRequest(w.Game, null, null)))).Key);
    }

    [Fact]
    public async Task A_failed_sd_copy_says_so_and_keeps_the_files()
    {
        var w = MakeWorld();
        var notADrive = TestFiles.Write(Path.Combine(TestFiles.TempDir(), "sd"), [0]); // a file where the SD folder should be
        var error = await Assert.ThrowsAsync<BuilderException>(() => Run(w, new BuildRequest(w.Game, null, notADrive)));
        Assert.Equal("SdCopyFailed", error.Key);
        Assert.True(File.Exists(Path.Combine(w.Paths.Output, "vcs_game.so")));
        Assert.True(File.Exists(Path.Combine(w.Paths.Output, "vcsnx.nro")));
    }

    [Theory]
    [InlineData(0, 235, 0, 60, null)]
    [InlineData(10, 235, 0, 60, 1350.0)]
    [InlineData(110, 235, 100, 60, 750.0)]
    [InlineData(235, 235, 0, 600, 0.0)]
    public void Time_left_counts_only_what_was_really_compiled(int done, int total, int cached, int elapsedSeconds, double? remainingSeconds) =>
        Assert.Equal(remainingSeconds, Eta.Remaining(done, total, cached, TimeSpan.FromSeconds(elapsedSeconds))?.TotalSeconds);
}
```

- [ ] **Step 2: Correrlos y ver que fallan**

Run: `dotnet test builder/Builder.Core.Tests`
Expected: error de compilación `The type or namespace name 'BuildPaths' could not be found` (y `SdCard`, `Pipeline`, `Eta`, `Stage`).

- [ ] **Step 3: Implementar**

`builder/Builder.Core/SdCard.cs`:
```csharp
namespace Vcsnx.Builder.Core;

public sealed record SdTarget(string Root, string Label);
public sealed record CopyReport(int Copied, int Skipped);

/// <summary>Puts the game on the Switch's SD card: sdmc:/switch/vcsnx/ with vcsnx.nro, vcs_game.so,
/// VCSNative.ini (only if the player has none, so their settings stay) and PSP_DATA.</summary>
public static class SdCard
{
    public static IReadOnlyList<SdTarget> Candidates() =>
        DriveInfo.GetDrives()
            .Where(d => d.DriveType == DriveType.Removable && d.IsReady && Directory.Exists(Path.Combine(d.RootDirectory.FullName, "switch")))
            .Select(d => new SdTarget(d.RootDirectory.FullName, string.IsNullOrEmpty(d.VolumeLabel) ? d.Name : $"{d.Name} ({d.VolumeLabel})"))
            .ToList();

    public static string GameFolder(string sdRoot) => Path.Combine(sdRoot, "switch", "vcsnx");

    public static CopyReport Install(string sdRoot, string nro, string module, string ini, string dataRoot, IProgress<string>? progress, CancellationToken ct)
    {
        var game = GameFolder(sdRoot);
        Directory.CreateDirectory(game);
        var copied = 0;
        var skipped = 0;

        void Copy(string from, string to)
        {
            ct.ThrowIfCancellationRequested();
            if (Same(from, to))
            {
                ++skipped;
                return;
            }
            Directory.CreateDirectory(Path.GetDirectoryName(to)!);
            progress?.Report(Path.GetRelativePath(sdRoot, to));
            File.Copy(from, to, overwrite: true);
            File.SetLastWriteTimeUtc(to, File.GetLastWriteTimeUtc(from));
            ++copied;
        }

        Copy(nro, Path.Combine(game, "vcsnx.nro"));
        Copy(module, Path.Combine(game, "vcs_game.so"));
        var sdIni = Path.Combine(game, "VCSNative.ini");
        if (!File.Exists(sdIni)) Copy(ini, sdIni);
        foreach (var file in Directory.EnumerateFiles(dataRoot, "*", SearchOption.AllDirectories).Order(StringComparer.Ordinal))
        {
            var name = Path.GetFileName(file);
            if (name.StartsWith("._", StringComparison.Ordinal) || name == ".DS_Store") continue;
            Copy(file, Path.Combine(game, "PSP_DATA", Path.GetRelativePath(dataRoot, file)));
        }
        return new CopyReport(copied, skipped);
    }

    /// <summary>FAT keeps write times to 2 s: same size and time is the same file; same size alone gets a hash check.</summary>
    public static bool Same(string a, string b)
    {
        if (!File.Exists(b)) return false;
        var fa = new FileInfo(a);
        var fb = new FileInfo(b);
        if (fa.Length != fb.Length) return false;
        if (Math.Abs((fa.LastWriteTimeUtc - fb.LastWriteTimeUtc).TotalSeconds) <= 2) return true;
        return HashList.Sha256Of(a) == HashList.Sha256Of(b);
    }
}
```

`builder/Builder.Core/Eta.cs`:
```csharp
namespace Vcsnx.Builder.Core;

public static class Eta
{
    /// <summary>Time left from the wall time the units really compiled so far took (cached units finish at once).</summary>
    public static TimeSpan? Remaining(int done, int total, int cached, TimeSpan elapsed)
    {
        if (done >= total) return TimeSpan.Zero;
        var compiled = done - cached;
        return compiled <= 0 ? null : elapsed / compiled * (total - done);
    }
}
```

`builder/Builder.Core/Pipeline.cs`:
```csharp
using System.Diagnostics;

namespace Vcsnx.Builder.Core;

public enum Stage { Checking, Downloading, Compiling, Linking, Packaging, CopyingToSd, Done }

public sealed record BuildProgress(Stage Stage, int Done, int Total, TimeSpan? Remaining);
public sealed record BuildRequest(string GameFolder, string? IconImage, string? SdRoot, string? CorpusZip = null);
public sealed record BuildResult(string Module, string Nro, CopyReport? Sd);

public sealed record BuildPaths(string Toolchain, string Kit, string Hashes, string Work, string Output)
{
    public static BuildPaths NextTo(string baseDir) => new(
        Path.Combine(baseDir, "toolchain"), Path.Combine(baseDir, "kit"), Path.Combine(baseDir, "hashes"),
        Path.Combine(baseDir, "trabajo"), Path.Combine(baseDir, "salida"));
}

/// <summary>Check → download the translated code → compile → link vcs_game.so → vcsnx.nro (with the player's
/// icon) → SD card (spec §4).</summary>
public sealed class Pipeline(BuildPaths paths, HttpClient http, IProcessRunner runner, int cores, long memoryBytes, Func<string, long> freeSpace)
{
    public const long RequiredSpace = 3L << 30;

    public async Task<BuildResult> RunAsync(BuildRequest request, IProgress<BuildProgress> progress, TextWriter log, CancellationToken ct)
    {
        progress.Report(new BuildProgress(Stage.Checking, 0, 0, null));
        var toolchain = new Toolchain(Path.GetFullPath(paths.Toolchain));
        if (!toolchain.PathIsUsable) throw new BuilderException("PathNotAscii", toolchain.Root);
        var known = HashList.Load(Path.Combine(paths.Hashes, "executable.sha256")).Values.ToHashSet(StringComparer.Ordinal);
        var game = GameCopy.Check(request.GameFolder, known);
        if (!game.Ok)
            throw new BuilderException(game.State switch
            {
                ExecutableState.Missing => "GameMissing",
                ExecutableState.Encrypted => "GameEncrypted",
                _ => "GameUnknown",
            });
        log.WriteLine($"game: {game.Executable} ({game.DiscVersion})");
        Directory.CreateDirectory(paths.Work);
        Directory.CreateDirectory(paths.Output);
        var free = freeSpace(paths.Work);
        if (free < RequiredSpace) throw new BuilderException("NotEnoughSpace", Math.Ceiling((RequiredSpace - free) / 1e9));
        var kit = new Kit(paths.Kit);
        log.WriteLine($"kit {kit.Info.Version}, interface {kit.Info.Abi}");

        var corpus = new Corpus(Path.Combine(paths.Work, "generated"), HashList.Load(Path.Combine(paths.Hashes, "generated.sha256")));
        if (corpus.Problems().Count > 0)
        {
            progress.Report(new BuildProgress(Stage.Downloading, 0, 0, null));
            await FetchCorpusAsync(corpus, request.CorpusZip, log, ct);
        }

        var compiler = new Compiler(toolchain, kit, corpus.GeneratedDir, Path.Combine(paths.Work, "g"), runner);
        var stems = kit.GeneratedOrder;
        var cached = stems.Count(compiler.UpToDate);
        var jobs = JobPlanner.Jobs(cores, memoryBytes);
        log.WriteLine($"compiling {stems.Count} units, {cached} already done, {jobs} at a time");
        var clock = Stopwatch.StartNew();
        progress.Report(new BuildProgress(Stage.Compiling, cached, stems.Count, null));
        var objects = await compiler.CompileAllAsync(stems, jobs,
            new Relay<int>(done => progress.Report(new BuildProgress(Stage.Compiling, done, stems.Count, Eta.Remaining(done, stems.Count, cached, clock.Elapsed)))), ct);

        progress.Report(new BuildProgress(Stage.Linking, 0, 0, null));
        var module = Path.Combine(paths.Output, "vcs_game.so");
        await new ModuleLinker(toolchain, kit, runner).LinkAsync(objects, module, ct);

        progress.Report(new BuildProgress(Stage.Packaging, 0, 0, null));
        var nro = Path.Combine(paths.Output, "vcsnx.nro");
        if (request.IconImage is { } image)
        {
            var icon = Path.Combine(paths.Work, "icon.jpg");
            IconMaker.Make(image, icon);
            NroIcon.Replace(kit.RuntimeNro, icon, nro);
        }
        else
        {
            File.Copy(kit.RuntimeNro, nro, overwrite: true);
        }

        CopyReport? sd = null;
        if (request.SdRoot is { } sdRoot)
        {
            progress.Report(new BuildProgress(Stage.CopyingToSd, 0, 0, null));
            try
            {
                sd = SdCard.Install(sdRoot, nro, module, kit.Ini, game.DataRoot!, null, ct);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                throw new BuilderException("SdCopyFailed", e.Message, paths.Output); // the files are already in salida\
            }
            log.WriteLine($"SD: {sd.Copied} copied, {sd.Skipped} already there");
        }
        progress.Report(new BuildProgress(Stage.Done, 0, 0, TimeSpan.Zero));
        return new BuildResult(module, nro, sd);
    }

    async Task FetchCorpusAsync(Corpus corpus, string? localZip, TextWriter log, CancellationToken ct)
    {
        var zip = localZip ?? Path.Combine(paths.Work, "pspr.zip");
        for (var attempt = 0; attempt < 2; ++attempt)
        {
            if (localZip is null && (attempt > 0 || !File.Exists(zip)))
            {
                try
                {
                    await Corpus.DownloadAsync(http, Corpus.ArchiveUri, zip, ct);
                }
                catch (HttpRequestException e)
                {
                    throw new BuilderException("DownloadFailed", e.Message);
                }
            }
            corpus.ExtractFrom(zip);
            var problems = corpus.Problems();
            if (problems.Count == 0) return;
            log.WriteLine($"translated code does not match: {string.Join(", ", problems.Take(5))}");
            if (localZip is not null) break;
            File.Delete(zip);
        }
        throw new BuilderException("CorpusMismatch");
    }

    sealed class Relay<T>(Action<T> report) : IProgress<T>
    {
        public void Report(T value) => report(value);
    }
}
```

- [ ] **Step 4: Correr los tests**

Run: `dotnet test builder/Builder.Core.Tests`
Expected: `Passed!  - Failed: 0, Passed: 42`

- [ ] **Step 5: Crear la consola**

Run: `cd builder && dotnet new console -n Builder.Cli -f net10.0 -o Builder.Cli && dotnet sln add Builder.Cli && dotnet add Builder.Cli reference Builder.Core && cd ..`

Reemplazar `builder/Builder.Cli/Program.cs`:
```csharp
using Vcsnx.Builder.Core;

// Console front end of Builder.Core: the Mac proof and the Windows proof under Wine (Task 11), and a fallback
// for players. Usage: Builder.Cli --game <PSP_DATA> [--icon <image>] [--sd <SD root>]
//   [--corpus-zip <PSPRecomp zip>] [--base <folder with toolchain, kit, hashes>] [--toolchain <dir>]
//   [--kit <dir>] [--hashes <dir>] [--work <dir>] [--out <dir>]
var options = new Dictionary<string, string>(StringComparer.Ordinal);
for (var i = 0; i + 1 < args.Length; i += 2)
{
    if (!args[i].StartsWith("--", StringComparison.Ordinal)) { Console.Error.WriteLine($"unexpected '{args[i]}'"); return 2; }
    options[args[i][2..]] = args[i + 1];
}
if (!options.TryGetValue("game", out var gameFolder)) { Console.Error.WriteLine("usage: Builder.Cli --game <PSP_DATA> [options]"); return 2; }
var defaults = BuildPaths.NextTo(options.GetValueOrDefault("base", AppContext.BaseDirectory));
var paths = new BuildPaths(options.GetValueOrDefault("toolchain", defaults.Toolchain), options.GetValueOrDefault("kit", defaults.Kit),
    options.GetValueOrDefault("hashes", defaults.Hashes), options.GetValueOrDefault("work", defaults.Work), options.GetValueOrDefault("out", defaults.Output));
Directory.CreateDirectory(paths.Output);
using var logFile = new StreamWriter(Path.Combine(paths.Output, "builder.log"), append: false) { AutoFlush = true };
var log = TextWriter.Synchronized(logFile);
using var cancel = new CancellationTokenSource();
Console.CancelKeyPress += (_, e) => { e.Cancel = true; cancel.Cancel(); };
var pipeline = new Pipeline(paths, new HttpClient(), new LoggingRunner(new SystemProcessRunner(), log), Environment.ProcessorCount,
    GC.GetGCMemoryInfo().TotalAvailableMemoryBytes, path => new DriveInfo(Path.GetPathRoot(Path.GetFullPath(path))!).AvailableFreeSpace);
var last = "";
var progress = new Progress<BuildProgress>(p =>
{
    var line = p.Stage == Stage.Compiling
        ? $"[{p.Stage}] {p.Done}/{p.Total}" + (p.Remaining is { } r ? $" ~{Math.Ceiling(r.TotalMinutes)} min" : "")
        : $"[{p.Stage}]";
    if (line != last) Console.WriteLine(last = line);
});
try
{
    var result = await pipeline.RunAsync(new BuildRequest(gameFolder, options.GetValueOrDefault("icon"), options.GetValueOrDefault("sd"),
        options.GetValueOrDefault("corpus-zip")), progress, log, cancel.Token);
    Console.WriteLine(Strings.Get(result.Sd is null ? "Done" : "DoneSd", Path.GetDirectoryName(result.Module)!));
    return 0;
}
catch (BuilderException e) { Console.Error.WriteLine(e.Localized); log.WriteLine(e.Message); return 1; }
catch (CompileFailedException e) { Console.Error.WriteLine(Strings.Get("CompileFailed", e.Unit)); log.WriteLine(e.Message); return 1; }
catch (ToolFailedException e) { Console.Error.WriteLine(Strings.Get("ToolFailed", e.Tool)); log.WriteLine(e.Message); return 1; }
catch (OperationCanceledException) { Console.Error.WriteLine(Strings.Get("Cancelled")); return 130; }
```

Run: `dotnet build builder/Builder.Cli`
Expected: `Build succeeded.`

- [ ] **Step 6: Commit**

```bash
git add builder/VCSNX.Builder.sln builder/Builder.Core builder/Builder.Core.Tests builder/Builder.Cli
git commit -m "feat(builder): SD install, the build pipeline and its console front end

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 11: El Builder completo en el Mac y bajo Wine (B4)

**Files:** ninguno nuevo; resultados en el ledger.

**Interfaces:**
- Consumes: `build/builder/kit` (Task 6), `build/builder/toolchain-win` (Task 1), `Builder.Cli` (Task 10), `tools/compare_elf_code.sh` (Task 1).

- [ ] **Step 1: El Builder en el Mac**

Run:
```bash
dotnet run --project builder/Builder.Cli -c Release -- --game ~/Documents/VCSNative/PSP_DATA --toolchain /opt/devkitpro \
  --kit build/builder/kit --hashes builder/hashes --work build/builder/work-mac --out build/builder/out-mac
```
Expected: `[Downloading]`, `[Compiling] 1/235` … `235/235`, `[Linking]`, `[Packaging]`, `[Done]` y el mensaje "Listo/Done". ~20 min.

- [ ] **Step 2: Mismo módulo que el de CMake**

Run: `tools/compare_elf_code.sh build/builder/out-mac/vcs_game.so build/switch/vcs_game.so`
Expected: `.text`, `.rodata`, `.data`, `.data.rel.ro` identical. Si algo difiere, comparar `compile.json`/`module_link.json` con `ninja -C build/switch -t commands vcs_game.so` antes de seguir.

- [ ] **Step 3: La caché y el ícono**

Run: el comando del Step 1 con `--icon local/cover.png`, y luego `cmp build/builder/out-mac/vcsnx.nro build/builder/kit/vcsnx.nro`.
Expected: sin `[Downloading]` ni compilación, termina en segundos; `cmp` informa que difieren (el ícono cambió).

- [ ] **Step 4: En la consola, el módulo del Builder**

Copias estables de `build/builder/out-mac/vcs_game.so` y `vcsnx.nro`, "ftp listo", subirlas a `switch/vcsnx/` y pedir al usuario que juegue unos minutos y mire el ícono en hbmenu.
Expected: igual que en la Task 5, con el ícono de su portada.

- [ ] **Step 5: La consola de Windows bajo Wine**

Run:
```bash
dotnet publish builder/Builder.Cli -c Release -r win-x64 --self-contained -p:PublishSingleFile=true -o build/builder/cli-win && \
B=build/builder/winrun && rm -rf $B && mkdir -p $B && cp build/builder/cli-win/Builder.Cli.exe $B/ && \
cp -R build/builder/toolchain-win $B/toolchain && cp -R build/builder/kit $B/kit && cp -R builder/hashes $B/hashes && \
export WINEPREFIX=$HOME/.local/share/wineprefixes/vcsnx WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" && \
time "/Applications/Wine Stable.app/Contents/Resources/wine/bin/wine" $B/Builder.Cli.exe --game "Z:$HOME/Documents/VCSNative/PSP_DATA" \
  --corpus-zip "Z:$PWD/build/builder/work-mac/pspr.zip" --base "Z:$PWD/$B" && \
tools/compare_elf_code.sh build/builder/winrun/salida/vcs_game.so build/builder/out-mac/vcs_game.so
```
Expected: las mismas etapas, ~30–60 min bajo Rosetta + Wine, y las cuatro secciones identical: el compilador de Windows da el mismo módulo.

---

### Task 12: La ventana (B5)

**Files:**
- Create: `builder/Builder.App/Builder.App.csproj`, `builder/Builder.App/Program.cs`, `builder/Builder.App/MainForm.cs`

**Interfaces:**
- Consumes: `Pipeline`, `BuildPaths.NextTo`, `BuildRequest`, `BuildResult`, `BuildProgress`, `Stage`, `GameCopy`, `GameCheck`, `HashList`, `SdCard`, `Strings`, `BuilderException`, `CompileFailedException`, `ToolFailedException`, `LoggingRunner`, `SystemProcessRunner`.

- [ ] **Step 1: Escribir el proyecto**

`builder/Builder.App/Builder.App.csproj`:
```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>WinExe</OutputType>
    <TargetFramework>net10.0-windows</TargetFramework>
    <UseWindowsForms>true</UseWindowsForms>
    <EnableWindowsTargeting>true</EnableWindowsTargeting>
    <Nullable>enable</Nullable>
    <ImplicitUsings>enable</ImplicitUsings>
    <AssemblyName>VCSNX-Builder</AssemblyName>
    <RootNamespace>Vcsnx.Builder.App</RootNamespace>
    <Version>1.0.0</Version>
    <RuntimeIdentifier>win-x64</RuntimeIdentifier>
    <SelfContained>true</SelfContained>
    <PublishSingleFile>true</PublishSingleFile>
    <IncludeNativeLibrariesForSelfExtract>true</IncludeNativeLibrariesForSelfExtract>
  </PropertyGroup>
  <ItemGroup>
    <ProjectReference Include="../Builder.Core/Builder.Core.csproj" />
  </ItemGroup>
</Project>
```

`builder/Builder.App/Program.cs`:
```csharp
namespace Vcsnx.Builder.App;

internal static class Program
{
    [STAThread]
    static void Main()
    {
        Application.SetHighDpiMode(HighDpiMode.SystemAware);
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new MainForm());
    }
}
```

`builder/Builder.App/MainForm.cs`:
```csharp
using Vcsnx.Builder.Core;

namespace Vcsnx.Builder.App;

/// <summary>The window (spec §4.2): it only gathers the three choices and shows the pipeline's progress.</summary>
internal sealed class MainForm : Form
{
    readonly BuildPaths _paths = BuildPaths.NextTo(AppContext.BaseDirectory);
    readonly TextBox _game = new() { Width = 340 };
    readonly Label _gameStatus = new() { AutoSize = true, MaximumSize = new Size(440, 0) };
    readonly PictureBox _iconPreview = new() { Size = new Size(96, 96), SizeMode = PictureBoxSizeMode.Zoom, BorderStyle = BorderStyle.FixedSingle };
    readonly ComboBox _sd = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 260 };
    readonly Button _create = new() { AutoSize = true, Text = Strings.Get("Create"), Enabled = false };
    readonly ProgressBar _bar = new() { Width = 440, Maximum = 1000 };
    readonly Label _stage = new() { AutoSize = true };
    readonly Label _remaining = new() { AutoSize = true };
    readonly Button _cancel = new() { AutoSize = true, Text = Strings.Get("Cancel"), Enabled = false };
    readonly LinkLabel _details = new() { AutoSize = true, Text = "▸ " + Strings.Get("Details") };
    readonly TextBox _log = new() { Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical, Size = new Size(440, 160), Visible = false, Font = new Font(FontFamily.GenericMonospace, 8) };
    string? _icon;
    CancellationTokenSource? _run;

    public MainForm()
    {
        Text = "bAlo";
        FormBorderStyle = FormBorderStyle.FixedSingle;
        MaximizeBox = false;
        AutoSize = true;
        AutoSizeMode = AutoSizeMode.GrowAndShrink;
        var header = new Label { Text = "bAlo", Font = new Font("Segoe UI", 26, FontStyle.Bold), ForeColor = Color.White, BackColor = Color.Black, TextAlign = ContentAlignment.MiddleCenter, Size = new Size(460, 64) };
        var chooseGame = new Button { Text = Strings.Get("Choose"), AutoSize = true };
        var chooseIcon = new Button { Text = Strings.Get("ChooseImage"), AutoSize = true };
        var removeIcon = new Button { Text = Strings.Get("Remove"), AutoSize = true };
        var layout = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, AutoSize = true, Padding = new Padding(12), WrapContents = false };
        layout.Controls.AddRange([
            header,
            new Label { Text = Strings.Get("StepGame"), AutoSize = true, Margin = new Padding(0, 12, 0, 0) }, Row(_game, chooseGame), _gameStatus,
            new Label { Text = Strings.Get("StepIcon"), AutoSize = true, Margin = new Padding(0, 12, 0, 0) }, Row(chooseIcon, removeIcon, _iconPreview),
            new Label { Text = Strings.Get("StepSd"), AutoSize = true, Margin = new Padding(0, 12, 0, 0) }, _sd,
            Row(_create), _stage, _bar, Row(_remaining, _cancel), _details, _log,
        ]);
        Controls.Add(layout);

        chooseGame.Click += (_, _) =>
        {
            using var dialog = new FolderBrowserDialog { ShowNewFolderButton = false };
            if (dialog.ShowDialog(this) == DialogResult.OK) _game.Text = dialog.SelectedPath;
        };
        _game.TextChanged += async (_, _) => await CheckGameAsync();
        chooseIcon.Click += (_, _) =>
        {
            using var dialog = new OpenFileDialog { Filter = "PNG / JPG|*.png;*.jpg;*.jpeg" };
            if (dialog.ShowDialog(this) != DialogResult.OK) return;
            _icon = dialog.FileName;
            _iconPreview.ImageLocation = _icon;
        };
        removeIcon.Click += (_, _) => { _icon = null; _iconPreview.ImageLocation = null; _iconPreview.Image = null; };
        _sd.DropDown += (_, _) => FillSdList();
        _create.Click += async (_, _) => await CreateAsync();
        _cancel.Click += (_, _) => _run?.Cancel();
        _details.LinkClicked += (_, _) => _log.Visible = !_log.Visible;
        FillSdList();
    }

    static FlowLayoutPanel Row(params Control[] controls)
    {
        var row = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = new Padding(0) };
        row.Controls.AddRange(controls);
        return row;
    }

    void FillSdList()
    {
        var current = (_sd.SelectedItem as SdChoice)?.Root;
        _sd.Items.Clear();
        _sd.Items.Add(new SdChoice(null, Strings.Get("NoSd")));
        foreach (var target in SdCard.Candidates()) _sd.Items.Add(new SdChoice(target.Root, target.Label));
        _sd.SelectedItem = _sd.Items.Cast<SdChoice>().FirstOrDefault(c => c.Root == current) ?? _sd.Items[0];
    }

    sealed record SdChoice(string? Root, string Label)
    {
        public override string ToString() => Label;
    }

    async Task CheckGameAsync()
    {
        var folder = _game.Text;
        GameCheck check;
        try
        {
            var known = HashList.Load(Path.Combine(_paths.Hashes, "executable.sha256")).Values.ToHashSet(StringComparer.Ordinal);
            check = await Task.Run(() => GameCopy.Check(folder, known));
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException)
        {
            _gameStatus.ForeColor = Color.DarkRed;
            _gameStatus.Text = e.Message;
            _create.Enabled = false;
            return;
        }
        if (folder != _game.Text) return; // the player kept typing
        _gameStatus.ForeColor = check.Ok ? Color.DarkGreen : Color.DarkRed;
        _gameStatus.Text = check.Ok ? Strings.Get("GameOk", check.DiscVersion ?? "")
            : Strings.Get(check.State switch { ExecutableState.Missing => "GameMissing", ExecutableState.Encrypted => "GameEncrypted", _ => "GameUnknown" });
        _create.Enabled = check.Ok;
    }

    async Task CreateAsync()
    {
        _run = new CancellationTokenSource();
        _create.Enabled = false;
        _cancel.Enabled = true;
        _log.Clear();
        Directory.CreateDirectory(_paths.Output);
        await using var logFile = new StreamWriter(Path.Combine(_paths.Output, "builder.log"), append: false) { AutoFlush = true };
        var log = TextWriter.Synchronized(new Tee(logFile, line => BeginInvoke(new Action(() => _log.AppendText(line + Environment.NewLine)))));
        var pipeline = new Pipeline(_paths, new HttpClient(), new LoggingRunner(new SystemProcessRunner(), log), Environment.ProcessorCount,
            GC.GetGCMemoryInfo().TotalAvailableMemoryBytes, path => new DriveInfo(Path.GetPathRoot(Path.GetFullPath(path))!).AvailableFreeSpace);
        var progress = new Progress<BuildProgress>(ShowProgress);
        try
        {
            var request = new BuildRequest(_game.Text, _icon, (_sd.SelectedItem as SdChoice)?.Root);
            var result = await RunWithCorpusFallbackAsync(pipeline, request, progress, log);
            MessageBox.Show(this, Strings.Get(result.Sd is null ? "Done" : "DoneSd", Path.GetDirectoryName(result.Module)!), "bAlo");
        }
        catch (BuilderException e) { log.WriteLine(e.Message); MessageBox.Show(this, e.Localized, "bAlo", MessageBoxButtons.OK, MessageBoxIcon.Warning); }
        catch (CompileFailedException e) { log.WriteLine(e.Message); MessageBox.Show(this, Strings.Get("CompileFailed", e.Unit), "bAlo", MessageBoxButtons.OK, MessageBoxIcon.Error); }
        catch (ToolFailedException e) { log.WriteLine(e.Message); MessageBox.Show(this, Strings.Get("ToolFailed", e.Tool), "bAlo", MessageBoxButtons.OK, MessageBoxIcon.Error); }
        catch (OperationCanceledException) { _stage.Text = Strings.Get("Cancelled"); }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException or InvalidDataException)
        {
            log.WriteLine(e.ToString());
            MessageBox.Show(this, e.Message, "bAlo", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
        finally
        {
            _cancel.Enabled = false;
            _create.Enabled = true;
            _run.Dispose();
            _run = null;
        }
    }

    // Without internet the player can point at a PSPRecomp ZIP downloaded elsewhere (spec §5).
    async Task<BuildResult> RunWithCorpusFallbackAsync(Pipeline pipeline, BuildRequest request, IProgress<BuildProgress> progress, TextWriter log)
    {
        try
        {
            return await pipeline.RunAsync(request, progress, log, _run!.Token);
        }
        catch (BuilderException e) when (e.Key == "DownloadFailed")
        {
            if (MessageBox.Show(this, e.Localized, "bAlo", MessageBoxButtons.OKCancel, MessageBoxIcon.Warning) != DialogResult.OK) throw;
            using var dialog = new OpenFileDialog { Filter = "ZIP|*.zip" };
            if (dialog.ShowDialog(this) != DialogResult.OK) throw;
            return await pipeline.RunAsync(request with { CorpusZip = dialog.FileName }, progress, log, _run!.Token);
        }
    }

    void ShowProgress(BuildProgress p)
    {
        _stage.Text = p.Stage == Stage.Compiling ? Strings.Get("StageCompiling", p.Done, p.Total) : Strings.Get("Stage" + p.Stage);
        _bar.Value = p.Stage switch
        {
            Stage.Checking => 0,
            Stage.Downloading => 20,
            Stage.Compiling => 50 + (p.Total == 0 ? 0 : 850 * p.Done / p.Total),
            Stage.Linking => 920,
            Stage.Packaging => 960,
            Stage.CopyingToSd => 980,
            _ => 1000,
        };
        _remaining.Text = p.Remaining is { } r && p.Stage == Stage.Compiling ? Strings.Get("Remaining", Math.Max(1, (int)Math.Ceiling(r.TotalMinutes))) : "";
    }

    /// <summary>Writes the log file and shows each line in "Show details".</summary>
    sealed class Tee(TextWriter file, Action<string> show) : TextWriter
    {
        public override System.Text.Encoding Encoding => file.Encoding;
        public override void Write(char value) => file.Write(value);
        public override void WriteLine(string? value)
        {
            file.WriteLine(value);
            show(value ?? "");
        }
    }
}
```

- [ ] **Step 2: Compilar para Windows**

Run: `cd builder && dotnet sln add Builder.App && cd .. && dotnet publish builder/Builder.App -c Release -o build/builder/app-win`
Expected: `build/builder/app-win/VCSNX-Builder.exe` (~70–90 MB) sin errores.

- [ ] **Step 3: Abrir la ventana bajo Wine**

Run: `cp build/builder/app-win/VCSNX-Builder.exe build/builder/winrun/ && WINEPREFIX=$HOME/.local/share/wineprefixes/vcsnx WINEDEBUG=-all "/Applications/Wine Stable.app/Contents/Resources/wine/bin/wine" build/builder/winrun/VCSNX-Builder.exe`
Expected: la ventana con "bAlo" y los tres pasos. Pedir al usuario que la mire, elegir su `PSP_DATA` (`Z:\Users\...`) y comprobar el ✓; "Crear" termina en segundos porque `trabajo\` y `salida\` ya están de la Task 11. Anotar lo que diga el usuario.

- [ ] **Step 4: Commit**

```bash
git add builder/VCSNX.Builder.sln builder/Builder.App
git commit -m "feat(builder): the bAlo window

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 13: Licencias, empaquetado y README (B6)

**Files:**
- Create: `tools/package_builder.py`, `tools/tests/test_package_builder.py`, `builder/licenses/THIRD_PARTY.md`, `builder/README.txt`, `builder/RELEASE_NOTES.md`
- Modify: `README.md`

**Interfaces:**
- Consumes: `build/builder/app-win/VCSNX-Builder.exe` (Task 12), `build/builder/toolchain-win` (Task 1), `build/builder/kit` (Task 6), `builder/hashes`, `tools/check_runtime_clean.sh` (Task 4).
- Produces: `dist/VCSNX-Builder-<version>.zip` y `dist/vcsnx.nro`; `audit(root: Path, banned: set[str]) -> list[str]`.

- [ ] **Step 1: Test que falla**

```python
"""Tests for tools/package_builder.py (stdlib unittest). Run: python3 -I tools/tests/test_package_builder.py"""
import hashlib
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from package_builder import audit  # noqa: E402


class PackageBuilderTest(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def tree(self, files: dict[str, bytes]) -> Path:
        root = Path(self._tmp.name)
        for name, data in files.items():
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        return root

    def test_a_clean_tree_with_the_runtime_nro_passes(self) -> None:
        root = self.tree({"VCSNX-Builder.exe": b"exe", "kit/obj/module_entry.o": b"o", "kit/vcsnx.nro": b"runtime",
                          "kit/module_link.json": b"generated_registry"})
        self.assertEqual(audit(root, set()), [])

    def test_game_files_the_module_and_generated_code_are_refused_by_name(self) -> None:
        root = self.tree({"kit/generated/generated_unit_0001.cpp": b"x", "salida/vcs_game.so": b"x", "PSP_DATA/PSP_GAME/x": b"x",
                          "local/cover.png": b"x", "other.nro": b"x"})
        self.assertEqual(len(audit(root, set())), 5)

    def test_a_renamed_game_file_is_refused_by_its_hash(self) -> None:
        root = self.tree({"kit/innocent.bin": b"translated code"})
        self.assertEqual(audit(root, {hashlib.sha256(b"translated code").hexdigest()}), ["kit/innocent.bin: matches a game file's hash"])


if __name__ == "__main__":
    unittest.main()
```

Run: `python3 -I tools/tests/test_package_builder.py`
Expected: `ModuleNotFoundError: No module named 'package_builder'`

- [ ] **Step 2: Implementar el empaquetado**

```python
"""Assembles the v1 release files (spec §3, §7): dist/vcsnx.nro (the runtime) and VCSNX-Builder-<version>.zip,
refusing the zip if anything of the game slipped in (names of game files, of the translated code or of the
module, or any file whose SHA-256 is in builder/hashes), and refusing the runtime unless
tools/check_runtime_clean.sh proves it carries none of the translated code.
Usage: python3 -I tools/package_builder.py --exe <VCSNX-Builder.exe> --toolchain <Windows toolchain> --kit <kit> --build <build/switch> --out dist
"""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ALLOWED = {"kit/vcsnx.nro"}  # the runtime, checked separately
FORBIDDEN = [re.compile(p, re.IGNORECASE) for p in (
    r"(^|/)generated(/|$)", r"generated_unit_\d+", r"generated_registry\.(cpp|o)$", r"vcs_game\.so", r"\.(nro|nacp)$",
    r"(^|/)PSP_DATA(/|$)", r"(^|/)PSP_GAME(/|$)", r"(^|/)local(/|$)", r"EBOOT", r"(^|/)BOOT\.BIN$", r"\.(pmf|at3|iso|cso|sfo)$",
)]


def audit(root: Path, banned: set[str]) -> list[str]:
    problems = []
    for path in sorted(p for p in root.rglob("*") if p.is_file()):
        rel = path.relative_to(root).as_posix()
        if rel not in ALLOWED and any(p.search(rel) for p in FORBIDDEN):
            problems.append(f"{rel}: name of a game file, of the translated code or of the module")
        elif hashlib.sha256(path.read_bytes()).hexdigest() in banned:
            problems.append(f"{rel}: matches a game file's hash")
    return problems


def banned_hashes(hashes: Path) -> set[str]:
    return {line.split()[0] for f in ("executable.sha256", "generated.sha256") for line in (hashes / f).read_text().splitlines() if line.strip()}


def main() -> int:
    parser = argparse.ArgumentParser()
    for name in ("exe", "toolchain", "kit", "build", "out"):
        parser.add_argument(f"--{name}", required=True, type=Path)
    args = parser.parse_args()
    subprocess.run([str(ROOT / "tools" / "check_runtime_clean.sh"), str(args.build)], check=True)
    if (args.kit / "vcsnx.nro").read_bytes() != (args.build / "vcsnx.nro").read_bytes():
        print("kit/vcsnx.nro is not the runtime that was just checked: rebuild the kit", file=sys.stderr)
        return 3
    version = json.loads((args.kit / "kit.json").read_text())["version"]
    stage = args.out / f"VCSNX-Builder-{version}"
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)
    shutil.copy2(args.exe, stage / "VCSNX-Builder.exe")
    shutil.copytree(args.toolchain, stage / "toolchain")
    shutil.copytree(args.kit, stage / "kit")
    shutil.copytree(ROOT / "builder" / "hashes", stage / "hashes")
    shutil.copytree(ROOT / "builder" / "licenses", stage / "LICENSES")
    shutil.copy2(ROOT / "LICENSE", stage / "LICENSES" / "VCSNX-LICENSE.txt")
    shutil.copy2(ROOT / "vendor/vcs-recomp/LICENSE", stage / "LICENSES" / "PSPRecomp-LICENSE.txt")
    shutil.copy2(ROOT / "vendor/vcs-recomp/profiles/vcs/third_party/ffmpeg/COPYING.LGPLv2.1", stage / "LICENSES" / "FFmpeg-COPYING.LGPLv2.1")
    shutil.copy2(ROOT / "builder" / "README.txt", stage / "README.txt")
    problems = audit(stage, banned_hashes(ROOT / "builder" / "hashes"))
    if problems:
        print("refusing to package:\n  " + "\n  ".join(problems), file=sys.stderr)
        return 3
    archive = args.out / f"VCSNX-Builder-{version}.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for path in sorted(p for p in stage.rglob("*") if p.is_file()):
            zf.write(path, path.relative_to(args.out).as_posix())
    shutil.copy2(args.build / "vcsnx.nro", args.out / "vcsnx.nro")
    print(f"{archive} {archive.stat().st_size // (1 << 20)} MiB and {args.out / 'vcsnx.nro'}: audit clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

Run: `python3 -I tools/tests/test_package_builder.py`
Expected: `Ran 3 tests ... OK`

- [ ] **Step 3: Licencias, README del `.zip`, notas y README del repo**

`builder/licenses/THIRD_PARTY.md`:
```markdown
# Third-party software in VCSNX 1.0.0

## In VCSNX-Builder-1.0.0.zip

| Component | Version | License | Source |
|---|---|---|---|
| devkitA64 GCC (toolchain/devkitA64) | 16.1.0-1 | GPL-3.0 with the GCC Runtime Library Exception | https://github.com/devkitPro/buildscripts, https://gcc.gnu.org |
| devkitA64 binutils | 2.46.0-1 | GPL-3.0 | https://github.com/devkitPro/buildscripts, https://www.gnu.org/software/binutils |
| newlib (devkita64-newlib) | 4.6.0.20260123-4 | BSD-style (newlib's COPYING.NEWLIB) | https://github.com/devkitPro/newlib |
| PSPRecomp headers (kit/include/psprecomp) | f6e7d41 | MIT (PSPRecomp-LICENSE.txt) | https://github.com/jessicanataliagta/PSPRecomp |
| vice-city-stories-recomp headers (kit/include/host) | b5a861d | MIT | https://github.com/OverkillLabs3/vice-city-stories-recomp |
| SkiaSharp (inside VCSNX-Builder.exe) | from NuGet | MIT | https://github.com/mono/SkiaSharp |
| .NET runtime (inside VCSNX-Builder.exe) | 10 | MIT | https://github.com/dotnet/runtime |

## In vcsnx.nro

| Component | Version | License | Source |
|---|---|---|---|
| Game-module loader (platform/switch/elf_module, game_module) | adapted from gtalcs_nx so_util.c | MIT, (C) 2021 Andy Nguyen, fgsfds | https://github.com/NaGaa95/gtalcs_nx |
| PSPRecomp runtime and the vice-city-stories-recomp profile | f6e7d41 / b5a861d | MIT | see above |
| mesa-switch (Vulkan) | 606441c | MIT | https://github.com/danfromtico/mesa-switch |
| switch-mesa EGL, renamed copy | 20.1 | MIT | https://github.com/devkitPro/pacman-packages |
| libnx | 4.12.0 | ISC | https://github.com/switchbrew/libnx |
| SDL2 | 2.28.5 | zlib | https://github.com/libsdl-org/SDL |
| FFmpeg | 7.1 | LGPL-2.1 (FFmpeg-COPYING.LGPLv2.1) | https://ffmpeg.org |
| dav1d, bzip2, zlib, expat, zstd | devkitPro portlibs | BSD-2 / bzip2 / zlib / MIT / BSD-3 | https://github.com/devkitPro/pacman-packages |
| stb_image | 2.x | MIT / public domain | https://github.com/nothings/stb |

The toolchain packages are devkitPro's own Windows builds, unmodified. VCSNX's code is MIT
(VCSNX-LICENSE.txt); the "studios bAlo" logo and the splash sound are not covered by it.
No part of Grand Theft Auto: Vice City Stories is included: vcs_game.so is made on each player's PC from
their own copy.
```

`builder/README.txt`:
```
VCSNX Builder 1.0.0 — GTA: Vice City Stories para Nintendo Switch / for Nintendo Switch

ESPAÑOL
VCSNX son dos archivos en sdmc:/switch/vcsnx/: vcsnx.nro (del release, ya está en kit\) y vcs_game.so,
el código del juego, que este programa crea en tu PC con TU copia del juego (ULUS-10160, EE. UU.).
1. Descomprime esta carpeta en una ruta sin acentos, por ejemplo C:\VCSNX-Builder.
2. Abre VCSNX-Builder.exe. Si Windows dice "Windows protegió su PC": "Más información" → "Ejecutar de todas formas".
3. Elige la carpeta PSP_DATA de tu copia. Si el ejecutable está cifrado, el programa te dice cómo
   desencriptarlo con PPSSPP.
4. Opcional: tu imagen para el ícono y la SD de la Switch.
5. Pulsa "Crear vcs_game.so". La primera vez baja el código traducido (necesita internet) y compila ~35–60 min.
6. En la Switch, abre VCSNX desde hbmenu manteniendo R al abrir un juego.
Actualizaciones: baja el vcsnx.nro nuevo del release y reemplázalo en la SD; vcs_game.so sigue sirviendo salvo
que la consola diga que hay que crearlo de nuevo.
Si tu antivirus bloquea el compilador (toolchain\devkitA64\...), permite la carpeta del programa y vuelve a
pulsar "Crear": lo ya compilado se conserva.

ENGLISH
VCSNX is two files in sdmc:/switch/vcsnx/: vcsnx.nro (from the release, also in kit\) and vcs_game.so, the
game's code, which this program makes on your PC from YOUR copy of the game (ULUS-10160, US).
1. Unzip this folder to a path without accents, for example C:\VCSNX-Builder.
2. Open VCSNX-Builder.exe. If Windows says "Windows protected your PC": "More info" → "Run anyway".
3. Pick the PSP_DATA folder of your copy. If the executable is encrypted, the program tells you how to
   decrypt it with PPSSPP.
4. Optional: your image for the icon, and the Switch's SD card.
5. Press "Create vcs_game.so". The first time it downloads the translated code (needs internet) and compiles for ~35–60 min.
6. On the Switch, open VCSNX from hbmenu while holding R as you start a game.
Updates: download the new vcsnx.nro from the release and replace it on the SD card; vcs_game.so keeps working
unless the console says it must be made again.
If your antivirus blocks the compiler (toolchain\devkitA64\...), allow the program's folder and press "Create"
again: what was already compiled is kept.

Licenses: LICENSES\THIRD_PARTY.md. Problems: salida\builder.log.
```

`builder/RELEASE_NOTES.md`:
```markdown
# VCSNX 1.0.0

GTA: Vice City Stories (PSP) running natively on the Nintendo Switch, with Switch controls and prompts, a
smaller HUD and the studios bAlo splash.

- **`vcsnx.nro`**: the runtime. It contains nothing of the game.
- **`VCSNX-Builder-1.0.0.zip`** (Windows 10/11, 64-bit): makes **`vcs_game.so`**, the game's code, on your PC
  from **your own copy** of the game (ULUS-10160, US), and can copy everything to your SD card. See
  `README.txt` inside the zip. No game files are included.

---

GTA: Vice City Stories (PSP) nativo en Nintendo Switch, con controles y textos de Switch, HUD más pequeño y el
splash de studios bAlo. **`vcsnx.nro`** es el runtime, sin nada del juego; **`VCSNX-Builder-1.0.0.zip`**
(Windows 10/11 de 64 bits) crea **`vcs_game.so`**, el código del juego, en tu PC con **tu propia copia**
(ULUS-10160, EE. UU.) y puede copiar todo a tu SD. No incluye archivos del juego.
```

En `README.md`, justo después del aviso "No game files are included...":
```markdown
## Easy way: vcsnx.nro + VCSNX Builder for Windows

From [Releases](https://github.com/fsalamancar/VCSNX/releases) download `vcsnx.nro` (the runtime, nothing of
the game) and `VCSNX-Builder-<version>.zip`. Unzip the Builder to a folder without accents (e.g.
`C:\VCSNX-Builder`), open `VCSNX-Builder.exe`, pick your `PSP_DATA` folder and press **Create vcs_game.so**:
it makes the game's code on your PC from your own copy (the first time takes ~35–60 min and needs internet)
and can copy everything to your SD card (`sdmc:/switch/vcsnx/`). The steps below are for building by hand.
```

- [ ] **Step 4: Armar los archivos del release**

Run: `python3 -I tools/package_builder.py --exe build/builder/app-win/VCSNX-Builder.exe --toolchain build/builder/toolchain-win --kit build/builder/kit --build build/switch --out dist && ls -la dist`
Expected: `runtime is clean: ...`, `dist/VCSNX-Builder-1.0.0.zip ~150 MiB and dist/vcsnx.nro: audit clean`.

- [ ] **Step 5: Commit**

```bash
git add tools/package_builder.py tools/tests/test_package_builder.py builder/licenses builder/README.txt builder/RELEASE_NOTES.md README.md
git commit -m "feat(builder): package vcsnx.nro and the Builder zip, with licenses and content audits

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 14: Prueba del usuario en Windows y release v1.0.0 (B7)

**Files:** ninguno nuevo.

- [ ] **Step 1: El usuario lo prueba en su PC con Windows**

Darle `dist/VCSNX-Builder-1.0.0.zip` (pendrive o nube): descomprimir en `C:\VCSNX-Builder`, abrir `VCSNX-Builder.exe`, elegir su `PSP_DATA`, su SD y opcionalmente una imagen, "Crear". Expected: "Listo…", y `vcsnx.nro` + `vcs_game.so` de la SD se juegan en la Switch. Anotar tiempos y problemas; un problema vuelve a systematic-debugging.

- [ ] **Step 2: Juntar en `main`**

Pedir permiso al usuario (finishing-a-development-branch). Con su sí:
```bash
git checkout main && git merge --ff-only feat/builder && (cmake --build build/mac >/dev/null && cd build/mac && ctest 2>&1 | tail -3) && \
dotnet test builder/Builder.Core.Tests && for t in tools/tests/test_*.py; do python3 -I "$t" || exit 1; done
```
Expected: `100% tests passed out of 30`, `Passed!  - Failed: 0, Passed: 42` y los tests de Python `OK`.

- [ ] **Step 3: Publicar el release**

Confirmar con el usuario antes de publicar (público e irreversible). Con su sí:
```bash
git grep -n -I "$(whoami)\|192\.168\." -- . ':!vendor' | wc -l        # Expected: 0
C=$(git commit-tree 'main^{tree}' -p public -m "feat: VCSNX 1.0.0: publishable runtime and the VCSNX Builder for Windows

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>") && git branch -f public "$C" && \
git push origin public:main && git tag -a v1.0.0 "$C" -m "VCSNX 1.0.0" && git push origin v1.0.0 && \
gh release create v1.0.0 dist/vcsnx.nro dist/VCSNX-Builder-1.0.0.zip -R fsalamancar/VCSNX --title "VCSNX 1.0.0" --notes-file builder/RELEASE_NOTES.md
```
Expected: la URL del release; `gh release view v1.0.0 -R fsalamancar/VCSNX --json assets --jq '.assets[].name'` → `vcsnx.nro` y `VCSNX-Builder-1.0.0.zip`.
