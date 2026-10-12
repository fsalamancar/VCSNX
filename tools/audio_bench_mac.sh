#!/usr/bin/env bash
# Audio starvation bench on the Mac build (run tools/run_mac.sh once first: it links PSP_DATA).
# VCSNX_DEBUG_VBLANK_SLEEP_US makes every vblank slow, like the Switch, so the limiter's catch-up
# path runs. Prints the share of output blocks that were silence plus the last audio summary.
# Usage: tools/audio_bench_mac.sh <label> <sleep_us per vblank, 0 = full speed> <seconds>
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LABEL="${1:?usage: tools/audio_bench_mac.sh <label> <sleep_us> <seconds>}"
SLEEP_US="${2:?sleep_us}"
SECONDS_TO_RUN="${3:?seconds}"
BIN="$ROOT/build/mac/bin"
OUT="$ROOT/build/mac/audio-bench"
[ -x "$BIN/vcsnx" ] || { echo "build first: cmake --build --preset mac" >&2; exit 3; }
[ -e "$BIN/PSP_DATA" ] || { echo "run tools/run_mac.sh <PSP_DATA> once first" >&2; exit 4; }
mkdir -p "$OUT"
rm -f "$OUT/$LABEL.raw"
cp "$ROOT/config/VCSNative.ini" "$BIN/VCSNative.ini"
status=0
(cd "$BIN" && SDL_AUDIODRIVER=dummy PSPRECOMP_AUDIO_SUMMARY=1 PSPRECOMP_REALTIME_SPEED_DIAG=1 \
    PSPRECOMP_AUDIO_RAWDUMP="$OUT/$LABEL.raw" VCSNX_DEBUG_VBLANK_SLEEP_US="$SLEEP_US" \
    perl -e "alarm $SECONDS_TO_RUN; exec @ARGV" ./vcsnx > "$OUT/$LABEL.out" 2>&1) || status=$?
# 142 = stopped by the alarm, which is how every bench run ends.
[ "$status" -eq 0 ] || [ "$status" -eq 142 ] || { echo "vcsnx exited with $status" >&2; exit "$status"; }
python3 -I "$ROOT/tools/audio_silence.py" "$OUT/$LABEL.raw"
grep "audio-summary" "$OUT/$LABEL.out" | tail -1
