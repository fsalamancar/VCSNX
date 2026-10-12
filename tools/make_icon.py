"""Cuts the .nro icon (256x256 JPEG) out of a cover image. hbmenu and Sphaira show it, and Sphaira's
HOME menu forwarder copies it.

The cover is the user's own copy and stays out of git: keep it and the result in local/ (ignored).
cmake/vcsnx_switch.cmake embeds local/icon.jpg when it exists.

Usage: python3 -I tools/make_icon.py local/cover.png local/icon.jpg [centre_x centre_y side]
The crop is a square given as fractions of the cover's width; the defaults frame the GTA: Vice City
Stories cover around its logo, with the panels around it (helicopter, biker, Vic, the woman).
"""
import sys
from pathlib import Path

from PIL import Image

ICON_SIZE = 256
DEFAULT_CROP = (0.4945, 0.5191, 0.8364)  # centre x, centre y, side; all as fractions of the width


def main() -> int:
    if len(sys.argv) not in (3, 6):
        print("usage: make_icon.py <cover> <icon.jpg> [centre_x centre_y side]", file=sys.stderr)
        return 2
    cover = Image.open(sys.argv[1]).convert("RGB")
    cx, cy, side = (float(v) for v in sys.argv[3:6]) if len(sys.argv) == 6 else DEFAULT_CROP
    width = cover.size[0]
    half = side * width / 2
    box = (round(cx * width - half), round(cy * width - half), round(cx * width + half), round(cy * width + half))
    if box[0] < 0 or box[1] < 0 or box[2] > cover.size[0] or box[3] > cover.size[1]:
        print(f"crop {box} falls outside the {cover.size[0]}x{cover.size[1]} cover", file=sys.stderr)
        return 1
    icon = cover.crop(box).resize((ICON_SIZE, ICON_SIZE), Image.LANCZOS)
    out = Path(sys.argv[2])
    out.parent.mkdir(parents=True, exist_ok=True)
    # Baseline JPEG without metadata: what elf2nro and the HOME menu expect.
    icon.save(out, "JPEG", quality=92, optimize=True, progressive=False)
    print(f"{out}: {ICON_SIZE}x{ICON_SIZE}, {out.stat().st_size} bytes, crop {box}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
