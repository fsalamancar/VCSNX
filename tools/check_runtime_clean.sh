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
