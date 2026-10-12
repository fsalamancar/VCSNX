#!/usr/bin/env bash
# Regenerates the VCS AOT corpus from the user's own decrypted ELF and compares it with
# upstream's published generated/ (spec H1). Usage: tools/regen_generated.sh <decrypted ELF> [out dir]
# The new corpus is built in a temporary folder and only replaces <out dir> when it is identical to
# upstream's, so a stalled or partial generation never breaks a working build. (Both known
# generators currently stall: docs/regen-report.md. tools/restore_generated.sh is the working path.)
# VCSNX_RECOMP selects the generator binary (default: the vendor build, build/mac/bin/vcs_recomp;
# tools/build_upstream_generator.sh builds upstream's, see docs/fork-review.md).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ELF="${1:?usage: tools/regen_generated.sh <decrypted ELF> [output dir]}"
OUT="${2:-$ROOT/generated}"
OUT="${OUT%/}"
RECOMP="${VCSNX_RECOMP:-$ROOT/build/mac/bin/vcs_recomp}"
REFERENCE="$ROOT/.cache/upstream/profiles/vcs/generated"
KNOWN_SHA256=" 85c5e172fa0207086b72a37649e9a10d47845f0866b0dee179c48d930e43d17f fee2e86c7fe457ab6da463d9fdc16f156a0900adcdaa2fd260206c11907c5d65 "

case "$OUT" in ""|"/"|"$ROOT"|"$HOME"|"${ROOT%/*}") echo "refusing to use $OUT as output directory" >&2; exit 5;; esac
[ -x "$RECOMP" ] || { echo "missing $RECOMP: run 'cmake --build --preset mac' first" >&2; exit 3; }
[ -d "$REFERENCE" ] || { echo "missing $REFERENCE: run tools/fetch_upstream.sh first" >&2; exit 3; }
actual="$(shasum -a 256 "$ELF" | cut -d' ' -f1)"
case "$KNOWN_SHA256" in *" $actual "*) ;; *) echo "unknown ELF (sha256 $actual); only ULUS-10160 is supported" >&2; exit 4;; esac

mkdir -p "$ROOT/.cache"
STAGING="$(mktemp -d "$ROOT/.cache/regen.XXXXXX")"
status=0
"$RECOMP" "$ELF" --auto "$STAGING" 0x08804000 16384 || status=$?
if [ "$status" -eq 0 ] && python3 -I "$ROOT/tools/compare_trees.py" "$REFERENCE" "$STAGING"; then
    rm -rf "$OUT"
    mv "$STAGING" "$OUT"
    echo "regenerated corpus is identical to upstream; installed in $OUT"
else
    echo "regeneration failed or differs from upstream (generator exit $status); $OUT left untouched." >&2
    echo "partial output kept for inspection: $STAGING" >&2
    exit 1
fi
