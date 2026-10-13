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
