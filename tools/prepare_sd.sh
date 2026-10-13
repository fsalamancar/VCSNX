#!/usr/bin/env bash
# Copies the user's PSP_DATA, the VCSNX config and the .nro onto a mounted Switch SD card.
# Usage: tools/prepare_sd.sh <path to PSP_DATA> <SD card root, e.g. /Volumes/SWITCH>
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DATA="${1:?usage: tools/prepare_sd.sh <path to PSP_DATA> <SD card root>}"
SD="${2:?usage: tools/prepare_sd.sh <path to PSP_DATA> <SD card root>}"
NRO="$(find "$ROOT/build/switch" -name vcsnx.nro -print -quit)"
[ -d "$SD/switch" ] || { echo "$SD does not look like a Switch SD card (no switch/ folder)" >&2; exit 3; }
[ -n "$NRO" ] || { echo "build first: cmake --build --preset switch" >&2; exit 3; }
# The same places the game looks for its executable (vendor vcs_bootstrap_paths.cpp), in its order.
found=""
for relative in PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF EBOOT_DECRYPTED.ELF PSP_GAME/SYSDIR/EBOOT_DECRYPTED.BIN \
                EBOOT_DECRYPTED.BIN PSP_GAME/SYSDIR/BOOT.BIN BOOT.BIN PSP_GAME/SYSDIR/EBOOT.BIN EBOOT.BIN; do
    if [ -f "$DATA/$relative" ]; then found="$relative"; break; fi
done
if [ -z "$found" ]; then
    echo "no game executable under $DATA (expected PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF)" >&2; exit 4
fi
if [ "$(head -c 4 "$DATA/$found" | od -An -tx1 | tr -d ' \n')" != "7f454c46" ]; then
    echo "$DATA/$found is not a decrypted ELF; see README step 2" >&2; exit 4
fi
DEST="$SD/switch/vcsnx"
mkdir -p "$DEST"
# --delete only inside our own PSP_DATA copy; macOS metadata files are not copied.
rsync -a --delete --exclude='._*' --exclude='.DS_Store' "$DATA/" "$DEST/PSP_DATA/"
cp "$ROOT/config/VCSNative.ini" "$DEST/VCSNative.ini"
cp "$NRO" "$DEST/vcsnx.nro"
MODULE="$(find "$ROOT/build/switch" -maxdepth 1 -name vcs_game.so -print -quit)"
[ -z "$MODULE" ] || cp "$MODULE" "$DEST/vcs_game.so"   # the split build (VCSNX_GAME_MODULE=ON)
dot_clean -m "$DEST" 2>/dev/null || true
echo "SD ready: $DEST ($(du -sh "$DEST" | cut -f1))"
