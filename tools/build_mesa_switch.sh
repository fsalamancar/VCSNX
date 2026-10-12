#!/usr/bin/env bash
# Builds the pinned mesa-switch Vulkan (NVK) static libraries in Docker and stages an SDK in
# .cache/mesa-switch-sdk/{lib,include} (ignored by git). Usage: tools/build_mesa_switch.sh
# MESA_SWITCH_SRC=<checkout> reuses an existing checkout of the pinned commit (and its build).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=tools/lib/pinned.sh
. "$ROOT/tools/lib/pinned.sh"
SRC="${MESA_SWITCH_SRC:-$ROOT/.cache/mesa-switch}"
SDK="$ROOT/.cache/mesa-switch-sdk"

if [ ! -d "$SRC/.git" ] || [ "$(git -C "$SRC" rev-parse HEAD)" != "$MESA_SWITCH_COMMIT" ]; then
    [ -z "${MESA_SWITCH_SRC:-}" ] || { echo "$SRC is not mesa-switch at $MESA_SWITCH_COMMIT" >&2; exit 3; }
    mkdir -p "$ROOT/.cache"
    fetch_pinned "$MESA_SWITCH_URL" "$MESA_SWITCH_COMMIT" "$SRC"
fi
docker info >/dev/null 2>&1 || { echo "Docker is not running (open Docker Desktop)" >&2; exit 3; }
if [ ! -f "$SRC/builddir-switch/src/nouveau/vulkan/libvulkan.a" ]; then
    (cd "$SRC" && ./build-switch.sh)
fi

# The archives build-switch.sh asks ninja for, in its order.
ARCHIVES="src/nouveau/vulkan/libnvk.a src/nouveau/vulkan/libvulkan.a src/vulkan/util/libvulkan_util.a
src/compiler/nir/libnir.a src/compiler/libcompiler.a src/compiler/spirv/libvtn.a src/util/libxmlconfig.a
src/nouveau/compiler/libnak.a src/nouveau/compiler/libnak_rs.a src/nouveau/nil/libnil.a
src/nouveau/nil/liblibnil_format_table.a src/nouveau/mme/libnouveau_mme.a src/nouveau/winsys/libnouveau_ws.a
src/nouveau/headers/libnvidia_headers_c.a src/compiler/rust/libcompiler_c_helpers.a src/util/libmesa_util.a
src/util/libmesa_util_simd.a src/util/blake3/libblake3.a src/c11/impl/libmesa_util_c11.a"
rm -rf "$SDK"
mkdir -p "$SDK/lib" "$SDK/include"
count=0
for archive in $ARCHIVES; do
    cp "$SRC/builddir-switch/$archive" "$SDK/lib/"
    count=$((count + 1))
done
cp -R "$SRC/include/vulkan" "$SRC/include/vk_video" "$SDK/include/"
[ -f "$SDK/lib/libvulkan.a" ] && [ -f "$SDK/include/vulkan/vulkan.h" ] || { echo "SDK incomplete" >&2; exit 4; }

# switch-sdl2 presents through switch-mesa's EGL (Mesa 20.1), and that libEGL.a carries its own
# copy of Mesa's internals (util_format_*, ralloc, hash_table, NIR, ...) under the same names
# mesa-switch (Mesa 26) defines. Linking both gave 333 duplicate symbols, and letting one copy win
# would hand one Mesa the other's tables. A private copy of the legacy libEGL.a gets a prefix on
# every symbol both define, so each Mesa keeps its own code. References the legacy copy makes but
# does not satisfy itself, and that mesa-switch would satisfy, are renamed too, so they can never
# bind across versions: the C11 thread functions (newlib codes, thrd_success = 4, against Mesa 26's
# 0) to vcsnx_legacy_c11_* in platform/switch/legacy_c11_shim.c, and NIR/GLSL internals to names
# nothing defines, so a link that ever pulls them fails loudly instead.
DKP="${DEVKITPRO:-/opt/devkitpro}"
NM="$DKP/devkitA64/bin/aarch64-none-elf-nm"
OBJCOPY="$DKP/devkitA64/bin/aarch64-none-elf-objcopy"
LEGACY_EGL="$DKP/portlibs/switch/lib/libEGL.a"
defined_symbols() { "$NM" -g --defined-only "$@" 2>/dev/null | awk 'NF == 3 && $2 ~ /[A-Z]/ {print $3}' | sort -u; }
mkdir -p "$SDK/legacy"
defined_symbols "$SDK"/lib/*.a > "$SDK/legacy/mesa-switch-symbols.txt"
undefined_symbols() { "$NM" -u "$@" 2>/dev/null | awk 'NF >= 2 {print $NF}' | grep -v ':$' | sort -u; }
defined_symbols "$LEGACY_EGL" > "$SDK/legacy/legacy-defined.txt"
comm -12 "$SDK/legacy/legacy-defined.txt" "$SDK/legacy/mesa-switch-symbols.txt" \
    | awk '{print $1, "vcsnx_mesa20_" $1}' > "$SDK/legacy/renamed-symbols.txt"
undefined_symbols "$LEGACY_EGL" | comm -12 - "$SDK/legacy/mesa-switch-symbols.txt" \
    | comm -23 - "$SDK/legacy/legacy-defined.txt" > "$SDK/legacy/cross-references.txt"
awk '/^(call_once|cnd_|mtx_|thrd_|tss_)/ {print $1, "vcsnx_legacy_c11_" $1; next}
     {print $1, "vcsnx_mesa20_unresolved_" $1}' "$SDK/legacy/cross-references.txt" >> "$SDK/legacy/renamed-symbols.txt"
"$OBJCOPY" --redefine-syms="$SDK/legacy/renamed-symbols.txt" "$LEGACY_EGL" "$SDK/legacy/libEGL.a"
left="$(defined_symbols "$SDK/legacy/libEGL.a" | comm -12 - "$SDK/legacy/mesa-switch-symbols.txt" | wc -l | tr -d ' ')"
[ "$left" = 0 ] || { echo "legacy libEGL.a still shares $left symbols with mesa-switch" >&2; exit 4; }
crossing="$(undefined_symbols "$SDK/legacy/libEGL.a" | comm -12 - "$SDK/legacy/mesa-switch-symbols.txt" | wc -l | tr -d ' ')"
[ "$crossing" = 0 ] || { echo "legacy libEGL.a still references $crossing mesa-switch symbols" >&2; exit 4; }
echo "legacy EGL isolated: $(wc -l < "$SDK/legacy/renamed-symbols.txt" | tr -d ' ') symbols renamed in $SDK/legacy/libEGL.a" \
     "($(wc -l < "$SDK/legacy/cross-references.txt" | tr -d ' ') were references into mesa-switch)"
echo "mesa-switch SDK ready: $SDK ($count archives, $(du -sh "$SDK" | cut -f1))"
