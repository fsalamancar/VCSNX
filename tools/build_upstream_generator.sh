#!/usr/bin/env bash
# Builds upstream PSPRecomp's (pinned f6e7d41) profile generator into .cache/bin/vcs_recomp_upstream.
# H1 uses it to reproduce upstream's published generated/ byte for byte (docs/fork-review.md).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/.cache/upstream"
[ -d "$SRC/src" ] || { echo "missing $SRC: run tools/fetch_upstream.sh first" >&2; exit 3; }
mkdir -p "$ROOT/.cache/bin"
c++ -std=c++20 -O2 -I"$SRC/include" \
    "$SRC"/src/{decoder,deflate,elf32,guest_memory,nid_registry,program_analysis,runtime,sha256}.cpp \
    "$SRC/profiles/vcs/tools/vcs_codegen_main.cpp" \
    -o "$ROOT/.cache/bin/vcs_recomp_upstream"
echo "built $ROOT/.cache/bin/vcs_recomp_upstream"
