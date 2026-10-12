#!/usr/bin/env bash
# Installs upstream's published AOT corpus (pinned f6e7d41) as generated/, after checking that the
# user owns the matching ULUS-10160 ELF (spec §13.2 fallback; docs/regen-report.md).
# Usage: tools/restore_generated.sh <decrypted ELF>
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ELF="${1:?usage: tools/restore_generated.sh <decrypted ELF>}"
KNOWN_SHA256=" 85c5e172fa0207086b72a37649e9a10d47845f0866b0dee179c48d930e43d17f fee2e86c7fe457ab6da463d9fdc16f156a0900adcdaa2fd260206c11907c5d65 "
actual="$(shasum -a 256 "$ELF" | cut -d' ' -f1)"
case "$KNOWN_SHA256" in *" $actual "*) ;; *) echo "unknown ELF (sha256 $actual); only ULUS-10160 is supported" >&2; exit 4;; esac
"$ROOT/tools/fetch_upstream.sh"
STAGING="$(mktemp -d "$ROOT/.cache/restore.XXXXXX")"
cp -R "$ROOT/.cache/upstream/profiles/vcs/generated/." "$STAGING/"
python3 -I "$ROOT/tools/compare_trees.py" "$ROOT/.cache/upstream/profiles/vcs/generated" "$STAGING" >/dev/null
rm -rf "$ROOT/generated"
mv "$STAGING" "$ROOT/generated"
echo "generated/ restored from upstream f6e7d41 ($(find "$ROOT/generated" -name '*.cpp' | wc -l | tr -d ' ') .cpp files)"
