#!/usr/bin/env bash
# Checks every tool VCSNX Plans 1-2 need on the Mac (host build + devkitPro Switch build).
# Prints one line per check. Exits 0 only when nothing required is missing; WARN lines
# (video decoders, needed from H5 on) do not fail the check.
set -u
DKP="${DEVKITPRO:-/opt/devkitpro}"
PORTLIBS="$DKP/portlibs/switch"
export PKG_CONFIG_PATH="/opt/homebrew/opt/ffmpeg@7/lib/pkgconfig:/opt/homebrew/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
missing=0

check() { # description, command...
    local description="$1"; shift
    if "$@" >/dev/null 2>&1; then echo "ok      $description"; else echo "MISSING $description"; missing=1; fi
}
warn_check() { # description, command...
    local description="$1"; shift
    if "$@" >/dev/null 2>&1; then echo "ok      $description"; else echo "WARN    $description"; fi
}

echo "== Mac host"
for tool in cmake ninja pkg-config python3 git rsync; do check "$tool" command -v "$tool"; done
echo "info    $(cmake --version 2>/dev/null | head -1)"
check "sdl2 (pkg-config)" pkg-config --exists sdl2
check "ffmpeg libs (pkg-config)" pkg-config --exists libavformat libavcodec libswscale libswresample libavutil
echo "info    host libavcodec $(pkg-config --modversion libavcodec 2>/dev/null || echo none)"

echo "== devkitPro"
check "aarch64-none-elf-g++" test -x "$DKP/devkitA64/bin/aarch64-none-elf-g++"
check "Switch.cmake" test -f "$DKP/cmake/Switch.cmake"
check "elf2nro" test -x "$DKP/tools/bin/elf2nro"
check "nxlink" test -x "$DKP/tools/bin/nxlink"
check "libnx" test -f "$DKP/libnx/lib/libnx.a"
check "switch pkg-config" test -x "$PORTLIBS/bin/aarch64-none-elf-pkg-config"
check "switch-sdl2" test -f "$PORTLIBS/lib/libSDL2.a"
check "switch-mesa (libEGL)" test -f "$PORTLIBS/lib/libEGL.a"
check "switch-ffmpeg (libavcodec)" test -f "$PORTLIBS/lib/libavcodec.a"
check "cmake helper nx_create_nro" grep -rqs "nx_create_nro" "$DKP/cmake"
check "cmake helper nx_generate_nacp" grep -rqs "nx_generate_nacp" "$DKP/cmake"
check "cmake system name NintendoSwitch" grep -rqs "NintendoSwitch" "$DKP/cmake"
echo "info    $("$DKP/devkitA64/bin/aarch64-none-elf-gcc" --version 2>/dev/null | head -1)"
major="$(sed -n 's/^#define LIBAVCODEC_VERSION_MAJOR *\([0-9]*\).*/\1/p' "$PORTLIBS/include/libavcodec/version_major.h" 2>/dev/null)"
echo "info    switch libavcodec major ${major:-unknown} (vendor code targets 61 = FFmpeg 7)"

echo "== switch-ffmpeg pieces used by vcs_media_decoder.cpp (needed from H5)"
NM="$DKP/devkitA64/bin/aarch64-none-elf-nm"
for sym in ff_h264_decoder ff_atrac3p_decoder ff_mpegps_demuxer; do
    warn_check "$sym" sh -c "\"$NM\" -g \"$PORTLIBS/lib/libavcodec.a\" \"$PORTLIBS/lib/libavformat.a\" 2>/dev/null | grep -Eq ' [DR] $sym\$'"
done
echo "== Vulkan (Plan 2: fork GE backend, MoltenVK on Mac, mesa-switch on Switch)"
check "glslc (brew shaderc)" command -v glslc
check "docker daemon (builds mesa-switch)" docker info
check "Vulkan loader (brew vulkan-loader)" test -f /opt/homebrew/lib/libvulkan.dylib
check "MoltenVK ICD (brew molten-vk)" test -f /opt/homebrew/lib/libMoltenVK.dylib
check "Vulkan headers (brew vulkan-headers)" test -f /opt/homebrew/include/vulkan/vulkan.h
check "switch-libexpat" test -f "$PORTLIBS/lib/libexpat.a"
check "switch-libzstd" test -f "$PORTLIBS/lib/libzstd.a"
warn_check "mesa-switch SDK staged (tools/build_mesa_switch.sh)" test -f "$(cd "$(dirname "$0")/.." && pwd)/.cache/mesa-switch-sdk/lib/libvulkan.a"

exit "$missing"
