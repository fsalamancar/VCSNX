#!/usr/bin/env bash
# Fetches upstream PSPRecomp at the pinned commit into .cache/upstream (ignored by git).
# Used as the diff base for the fork review and as the reference generated/ corpus.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=tools/lib/pinned.sh
. "$ROOT/tools/lib/pinned.sh"
DEST="$ROOT/.cache/upstream"
if [ -d "$DEST/.git" ] && [ "$(git -C "$DEST" rev-parse HEAD)" = "$UPSTREAM_COMMIT" ]; then
    echo "up to date: $DEST"
    exit 0
fi
mkdir -p "$ROOT/.cache"
fetch_pinned "$UPSTREAM_URL" "$UPSTREAM_COMMIT" "$DEST"
echo "fetched $UPSTREAM_COMMIT into $DEST ($(find "$DEST/profiles/vcs/generated" -type f | wc -l | tr -d ' ') generated files)"
