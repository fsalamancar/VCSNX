#!/usr/bin/env bash
# Builds a small shared module, the way the Builder builds vcs_game.so, with devkitPro's Windows toolchain
# under Wine, and checks it carries the same code as the Mac toolchain's. Proves the Windows compiler,
# its C/C++ headers and its linker run on their own (no MSYS2).
# Usage: tools/smoke_windows_toolchain.sh <toolchain from fetch_windows_toolchain.py>
# Wine: $WINE, else WineHQ's macOS build in ~/Applications or /Applications ("Wine Stable.app").
set -euo pipefail
TC="$(cd "${1:?usage: smoke_windows_toolchain.sh <toolchain folder>}" && pwd)"
if [ -z "${WINE:-}" ]; then
    for app in "$HOME/Applications/Wine Stable.app" "/Applications/Wine Stable.app"; do
        [ -x "$app/Contents/Resources/wine/bin/wine" ] && { WINE="$app/Contents/Resources/wine/bin/wine"; break; }
    done
fi
[ -n "${WINE:-}" ] || { echo "no Wine: install WineHQ's macOS build or set WINE" >&2; exit 3; }
export WINEPREFIX="${WINEPREFIX:-$HOME/.local/share/wineprefixes/vcsnx}" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml="
mkdir -p "$WINEPREFIX"   # Wine does not create the prefix folder itself
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cat > "$WORK/smoke.cpp" <<'CPP'
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
int host_add(int a, int b);
extern "C" int smoke_entry(int x) { return host_add(x, std::numeric_limits<std::uint8_t>::max()); }
extern "C" float smoke_math(float v) { return std::sqrt(v) + std::bit_cast<float>(std::uint32_t{0x3f800000u}); }
CPP
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
