#!/usr/bin/env bash
# Runs the Mac test bench build against the user's own PSP_DATA (spec H2).
# Usage: tools/run_mac.sh <path to PSP_DATA>   (extra PSPRECOMP_* variables pass through)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DATA="${1:?usage: tools/run_mac.sh <path to PSP_DATA>}"
BIN="$ROOT/build/mac/bin"
[ -x "$BIN/vcsnx" ] || { echo "build first: cmake --build --preset mac" >&2; exit 3; }
if [ ! -f "$DATA/EBOOT.BIN" ] && [ ! -f "$DATA/PSP_GAME/SYSDIR/BOOT.BIN" ]; then
    echo "no decrypted ELF under $DATA" >&2; exit 4
fi
ln -sfn "$DATA" "$BIN/PSP_DATA"
cp "$ROOT/config/VCSNative.ini" "$BIN/VCSNative.ini"
cd "$BIN"
PSPRECOMP_FRAME_TIME_DIAG=1 exec "$BIN/vcsnx"
