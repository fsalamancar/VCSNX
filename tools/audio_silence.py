"""Share of an audio dump that is device underrun.

Reads the S16LE stereo file written by PSPRECOMP_AUDIO_RAWDUMP and counts 512-frame blocks that
are exactly zero (what the SDL callback emits when every channel queue is empty), after skipping
the boot. Usage: python3 -I tools/audio_silence.py <dump.raw> [skip_seconds=20]
"""
import sys
from array import array

BLOCK_SAMPLES = 512 * 2  # 512 stereo frames
RATE = 44100


def main() -> int:
    path = sys.argv[1]
    skip = float(sys.argv[2]) if len(sys.argv) > 2 else 20.0
    data = array("h")
    with open(path, "rb") as dump:
        data.frombytes(dump.read())
    start = int(skip * RATE) * 2
    start -= start % BLOCK_SAMPLES
    blocks = silent = 0
    for index in range(start, len(data) - BLOCK_SAMPLES + 1, BLOCK_SAMPLES):
        blocks += 1
        if not any(data[index:index + BLOCK_SAMPLES]):
            silent += 1
    share = 100.0 * silent / max(blocks, 1)
    print(f"{blocks} blocks after {skip:.0f} s, silent {silent} ({share:.1f}%)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
