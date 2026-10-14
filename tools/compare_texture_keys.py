# tools/compare_texture_keys.py
"""Compares the Builder's texture list (Builder.Cli --hd-list) with the names of the textures the console dumped
([Textures] DumpOriginals=true writes sdmc:/switch/vcsnx/Textures/originals/<key>.png). Prints how many dumped keys
the Builder also produces, per source, and the dumped keys it does not. Nothing of the game is written.
Usage: python3 -I tools/compare_texture_keys.py <list.txt> <dumped-names.txt>
"""
import sys
from collections import Counter
from pathlib import Path


def main(listing: Path, dumped: Path) -> int:
    known = {}
    for line in listing.read_text().splitlines():
        parts = line.split(maxsplit=4)
        if len(parts) >= 4:
            known[parts[0]] = parts[3]
    keys = sorted({name.strip()[:16] for name in dumped.read_text().splitlines() if name.strip().endswith(".png")})
    per_source = Counter(known[key] for key in keys if key in known)
    matched = sum(per_source.values())
    detail = ", ".join(f"{source} {count}" for source, count in sorted(per_source.items()))
    print(f"dumped {len(keys)}")
    print(f"matched {matched} ({detail})")
    print("unmatched: " + " ".join(key for key in keys if key not in known))
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    sys.exit(main(Path(sys.argv[1]), Path(sys.argv[2])))
