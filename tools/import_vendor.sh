#!/usr/bin/env bash
# Imports the pinned PSPRecomp fork into vendor/vcs-recomp without generated code or prebuilt
# binaries, plus upstream's optimize_generated_*.py passes. Re-running replaces vendor/vcs-recomp.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=tools/lib/pinned.sh
. "$ROOT/tools/lib/pinned.sh"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

fetch_pinned "$FORK_URL" "$FORK_COMMIT" "$WORK/fork"
fetch_pinned "$UPSTREAM_URL" "$UPSTREAM_COMMIT" "$WORK/upstream"

DEST="$ROOT/vendor/vcs-recomp"
rm -rf "$DEST"
mkdir -p "$DEST"
rsync -a \
    --exclude='.git/' \
    --exclude='profiles/vcs/generated/' \
    --exclude='profiles/vcs/third_party/ffmpeg/bin/' \
    --exclude='profiles/vcs/third_party/ffmpeg/lib/' \
    --exclude='*.dll' --exclude='*.lib' --exclude='*.exe' \
    "$WORK/fork/" "$DEST/"
for pass in cross_unit_hot_regs fpr_blocks gpr_blocks; do
    cp "$WORK/upstream/profiles/vcs/tools/optimize_generated_${pass}.py" "$DEST/profiles/vcs/tools/"
done

# Guards: nothing derived from the game and no Windows binaries may land in vendor/.
if [ -e "$DEST/profiles/vcs/generated" ]; then echo "generated/ was imported" >&2; exit 1; fi
if find "$DEST" -type f \( -iname '*.dll' -o -iname '*.lib' -o -iname '*.exe' \) | grep -q .; then
    echo "a prebuilt binary was imported" >&2; exit 1
fi
echo "imported fork $FORK_COMMIT (+ optimizers from $UPSTREAM_COMMIT): $(find "$DEST" -type f | wc -l | tr -d ' ') files, $(du -sh "$DEST" | cut -f1)"
