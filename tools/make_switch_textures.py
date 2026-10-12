"""Draws VCSNX's Switch replacements for the PSP-specific menu textures, with Pillow and no game data.

The fork swaps any game texture for Textures/<hash>.png, where <hash> names the decoded original and
the PNG is exactly 2x, 4x or 8x its size (vcs_texture_replace.cpp). This writes, at 4x:
- cc29539db154b0c7.png: the PSP drawn in Settings > Controls (256x128) becomes a Pro Controller-style
  pad. The game draws the label lines at fixed places for the PSP's buttons, so the d-pad stays upper
  left, the stick lower left and -/+ at the bottom right of centre, where those lines end. Each face
  button carries the letter that does that action (pad_mapping.cpp: cross is A, circle is B, square
  is Y, triangle is X), so the line for "Run" ends on A even though A sits on the right of a real pad.
- 78dfb7e45c4c32b5.png / 27b80d3a3bb72734.png: the menu legend's cross and circle (16x16) become the
  A (confirm) and B (back) buttons.

Usage: python3 -I tools/make_switch_textures.py <output folder>
Then copy the PNGs into the game's Textures folder (Textures/ beside the game, sdmc:/switch/vcsnx/Textures
on the Switch); [Textures] Replacement=true in VCSNative.ini loads them.
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

CONTROLS_PSP = "cc29539db154b0c7"  # 256x128
LEGEND_CROSS = "78dfb7e45c4c32b5"  # 16x16
LEGEND_CIRCLE = "27b80d3a3bb72734"  # 16x16

SUPERSAMPLE = 4
# The menu draws the 256x128 texture 1.94x wide and 1.73x tall on a 1280x720 frame; vertical radii are
# stretched by the ratio so buttons look round on screen.
ROUND_Y = 1.12

FONT_PATHS = (
    "/System/Library/Fonts/Supplemental/Arial Rounded Bold.ttf",
    "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
)

BODY = (58, 58, 64, 255)
BODY_EDGE = (24, 24, 28, 255)
BODY_SHINE = (74, 74, 82, 255)
RECESS = (40, 40, 46, 255)
BUTTON = (30, 30, 34, 255)
BUTTON_RING = (112, 112, 120, 255)
SHOULDER = (88, 88, 96, 255)
GLYPH = (230, 230, 230, 255)
LED = (90, 220, 120, 255)


def font(size: int) -> ImageFont.FreeTypeFont:
    for path in FONT_PATHS:
        if Path(path).exists():
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


class Canvas:
    """Draws in target-pixel coordinates on a supersampled image."""

    def __init__(self, width: int, height: int) -> None:
        self.size = (width, height)
        self.image = Image.new("RGBA", (width * SUPERSAMPLE, height * SUPERSAMPLE), (0, 0, 0, 0))
        self.draw = ImageDraw.Draw(self.image)

    def _box(self, x0: float, y0: float, x1: float, y1: float) -> list:
        return [x0 * SUPERSAMPLE, y0 * SUPERSAMPLE, x1 * SUPERSAMPLE, y1 * SUPERSAMPLE]

    def ellipse(self, cx: float, cy: float, rx: float, ry: float, fill, outline=None, width: float = 0) -> None:
        self.draw.ellipse(self._box(cx - rx, cy - ry, cx + rx, cy + ry), fill=fill, outline=outline,
                          width=round(width * SUPERSAMPLE))

    def button(self, cx: float, cy: float, r: float, fill, outline=None, width: float = 0) -> None:
        self.ellipse(cx, cy, r, r * ROUND_Y, fill, outline, width)

    def rounded(self, x0: float, y0: float, x1: float, y1: float, radius: float, fill, outline=None,
                width: float = 0) -> None:
        self.draw.rounded_rectangle(self._box(x0, y0, x1, y1), radius=radius * SUPERSAMPLE, fill=fill,
                                    outline=outline, width=round(width * SUPERSAMPLE))

    def polygon(self, points, fill) -> None:
        self.draw.polygon([(x * SUPERSAMPLE, y * SUPERSAMPLE) for x, y in points], fill=fill)

    def text(self, cx: float, cy: float, label: str, size: float, fill) -> None:
        face = font(round(size * SUPERSAMPLE))
        self.draw.text((cx * SUPERSAMPLE, cy * SUPERSAMPLE), label, font=face, fill=fill, anchor="mm")

    def result(self) -> Image.Image:
        return self.image.resize(self.size, Image.LANCZOS)


def body_shape(c: Canvas, fill, grow: float = 0.0) -> None:
    """Main body plus two grips; `grow` widens every part (used for the outline pass)."""
    c.rounded(40 - grow, 64 - grow, 984 + grow, 330 + grow, 110 + grow, fill)
    c.ellipse(224, 340, 190 + grow, 150 + grow, fill)
    c.ellipse(800, 340, 190 + grow, 150 + grow, fill)


def pro_controller() -> Image.Image:
    c = Canvas(1024, 512)
    # Shoulder buttons first, so the body overlaps their lower edge.
    c.rounded(70, 34, 270, 84, 22, SHOULDER, BODY_EDGE, 5)
    c.rounded(754, 34, 954, 84, 22, SHOULDER, BODY_EDGE, 5)
    body_shape(c, BODY_EDGE, grow=6)
    body_shape(c, BODY)
    c.rounded(120, 74, 904, 120, 40, BODY_SHINE)  # light catching the top edge
    # Player LEDs, top centre.
    for i in range(4):
        c.rounded(470 + i * 24, 90, 484 + i * 24, 98, 3, LED)

    # D-pad (where the PSP's was: the label lines end there).
    cx, cy = 118, 238
    c.ellipse(cx, cy, 86, 86 * ROUND_Y, RECESS)
    arm, length = 24, 68
    c.rounded(cx - length, cy - arm * ROUND_Y, cx + length, cy + arm * ROUND_Y, 8, BUTTON)
    c.rounded(cx - arm, cy - length * ROUND_Y, cx + arm, cy + length * ROUND_Y, 8, BUTTON)
    tip, half = 14, 9
    for dx, dy in ((0, -1), (0, 1), (-1, 0), (1, 0)):
        ax, ay = cx + dx * (length - 22), cy + dy * (length - 22) * ROUND_Y
        # Each arrow points outwards, along its arm.
        if dx == 0:
            c.polygon([(ax, ay + dy * tip * 0.5), (ax - half, ay - dy * tip * 0.5), (ax + half, ay - dy * tip * 0.5)],
                      GLYPH)
        else:
            c.polygon([(ax + dx * tip * 0.5, ay), (ax - dx * tip * 0.5, ay - half), (ax - dx * tip * 0.5, ay + half)],
                      GLYPH)

    # Sticks: the left one where the PSP's nub was, the right one inside the right grip.
    for sx, sy in ((122, 372), (676, 352)):
        c.button(sx, sy, 50, BUTTON, BODY_EDGE, 4)
        c.button(sx, sy, 38, RECESS, BUTTON_RING, 3)
        c.button(sx, sy, 24, BUTTON)

    # Face buttons in the PSP's places, each with the letter that does its action: triangle X (top),
    # circle B (right), cross A (bottom), square Y (left).
    for label, bx, by in (("X", 878, 182), ("B", 933, 238), ("A", 878, 293), ("Y", 822, 238)):
        c.button(bx, by, 31, BUTTON, BUTTON_RING, 4)
        c.text(bx, by + 1, label, 38, GLYPH)

    # Minus and plus where Select and Start were, then Capture and Home in the middle.
    c.button(710, 446, 19, BUTTON, BUTTON_RING, 3)
    c.rounded(701, 443, 719, 449, 2, GLYPH)
    c.button(773, 446, 19, BUTTON, BUTTON_RING, 3)
    c.rounded(764, 443, 782, 449, 2, GLYPH)
    c.rounded(770, 436, 776, 456, 2, GLYPH)
    c.rounded(436, 236, 466, 270, 6, BUTTON, BUTTON_RING, 3)
    c.button(451, 253, 8, RECESS)
    c.button(574, 253, 18, BUTTON, BUTTON_RING, 3)
    c.polygon([(574, 241), (563, 252), (585, 252)], GLYPH)
    c.rounded(566, 252, 582, 264, 1, GLYPH)
    return c.result()


def legend_button(label: str) -> Image.Image:
    c = Canvas(64, 64)
    c.ellipse(32, 32, 28, 28, (36, 36, 40, 255), (150, 150, 156, 255), 4)
    c.text(32, 33, label, 34, GLYPH)
    return c.result()


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__.strip().splitlines()[-4], file=sys.stderr)
        return 2
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    images = {
        CONTROLS_PSP: pro_controller(),
        LEGEND_CROSS: legend_button("A"),
        LEGEND_CIRCLE: legend_button("B"),
    }
    for name, image in images.items():
        image.save(out / f"{name}.png")
        print(f"{name}.png {image.size[0]}x{image.size[1]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
