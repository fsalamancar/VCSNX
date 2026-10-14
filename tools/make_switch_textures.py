"""Draws VCSNX's Switch replacements for the PSP-specific menu textures, with Pillow and no game data.

The fork swaps any game texture for Textures/<hash>.png, where <hash> names the decoded original and
the PNG is exactly 2x, 4x or 8x its size (vcs_texture_replace.cpp). This writes:
- cc29539db154b0c7.png, at 8x: the PSP drawn in Settings > Controls (256x128) becomes the author's Switch
  (assets/controls/switch_handheld.png, the author's own picture under a free licence) with VCSNX's labels:
  each control with what it does on foot and, in cyan, in a vehicle (pad_mapping.cpp). VCSNX hides the
  game's own labels and label lines on that page and stretches this picture over the band they took
  (platform/switch/controls_screen.hpp), so it is drawn for that band's shape and squeezed to 2:1.
- 78dfb7e45c4c32b5.png / 27b80d3a3bb72734.png, at 4x: the menu legend's cross and circle (16x16) become the
  A (confirm) and B (back) buttons.

Usage: python3 -I tools/make_switch_textures.py <output folder>
Then copy the PNGs into the game's Textures folder (Textures/ beside the game, sdmc:/switch/vcsnx/Textures
on the Switch); [Textures] Replacement=true in VCSNative.ini loads them.
"""
import sys
from dataclasses import dataclass
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


HANDHELD = Path(__file__).resolve().parents[1] / "assets" / "controls" / "switch_handheld.png"

# The band the picture fills (controls_screen.hpp: 460 x 118 of the menu's 480 x 272, drawn 2.67 x 2.65 times
# that on a 1280 x 720 screen), drawn at this size and then squeezed to the texture's 2:1.
BAND = (2456, 624)
PICTURE = (2048, 1024)
FOOT = (240, 240, 240, 255)
CAR = (0, 215, 220, 255)  # the menu's cyan
PINK = (255, 105, 180, 255)
BADGE = (40, 40, 46, 255)


@dataclass(frozen=True)
class Control:
    """One control: its badge, what it does on foot and in a vehicle ("" when the same), and where it is on the
    author's picture (its own 708 x 305 pixels)."""

    badge: str
    on_foot: str
    in_car: str
    point: tuple


# pad_mapping.cpp: A is cross, B circle, Y square, X triangle; on foot ZL/ZR are L/R; in a vehicle ZR accelerates,
# ZL brakes, L and R look left and right (both, or the right stick pressed, look back). The actions are the game's
# own for SETUP 1 (Settings > Controls, on foot and in car, 2026-10-14).
CONTROLS_LEFT = (
    Control("L", "Look / Fine aim", "Look left", (32, 14)),
    Control("ZL", "Look / Fine aim", "Brake / Reverse", (14, 34)),
    Control("−", "Camera modes", "", (84, 36)),
    Control("stick", "Move", "Steer", (54, 81)),
    Control("up", "Special mission", "", (55, 142)),
    Control("leftright", "Cycle weapons / targets", "Radio station", (55, 164)),
    Control("down", "Free aim", "Horn", (55, 186)),
)
CONTROLS_RIGHT = (
    Control("R", "Target", "Look right", (676, 14)),
    Control("ZR", "Target", "Accelerate", (694, 34)),
    Control("+", "Pause", "", (625, 36)),
    # In the order their lines reach the buttons without crossing.
    Control("X", "Enter vehicle", "Exit vehicle", (655, 60)),
    Control("Y", "Jump", "Brake / Reverse", (631, 82)),
    Control("A", "Run", "Accelerate", (678, 82)),
    Control("B", "Attack / Fire weapon", "", (655, 104)),
    Control("rstick", "Camera", "Press to look back", (655, 164)),
)


def draw_badge(d: ImageDraw.ImageDraw, cx: float, cy: float, kind: str, scale: float) -> None:
    r = 30 * scale
    if kind in ("L", "ZL", "R", "ZR"):
        d.rounded_rectangle([cx - r * 1.35, cy - r * 0.85, cx + r * 1.35, cy + r * 0.85], radius=r * 0.5, fill=BADGE,
                            outline=PINK, width=round(4 * scale))
    else:
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=BADGE, outline=PINK, width=round(4 * scale))
    if kind in ("stick", "rstick"):
        d.ellipse([cx - r * 0.62, cy - r * 0.62, cx + r * 0.62, cy + r * 0.62], outline=FOOT, width=round(4 * scale))
        d.ellipse([cx - r * 0.3, cy - r * 0.3, cx + r * 0.3, cy + r * 0.3], fill=FOOT)
        return
    arrows = {"up": [(0, -1)], "down": [(0, 1)], "leftright": [(-1, 0), (1, 0)]}
    if kind in arrows:
        for dx, dy in arrows[kind]:
            ax, ay, t = cx + dx * r * 0.32, cy + dy * r * 0.32, r * 0.42
            if dx == 0:
                pts = [(ax, ay + dy * t), (ax - t, ay - dy * t * 0.6), (ax + t, ay - dy * t * 0.6)]
            else:
                pts = [(ax + dx * t, ay), (ax - dx * t * 0.6, ay - t), (ax - dx * t * 0.6, ay + t)]
            d.polygon(pts, fill=FOOT)
        return
    if kind in ("−", "+"):  # bars: the fonts have no minus sign
        t, l = 4 * scale, r * 0.5
        d.rectangle([cx - l, cy - t, cx + l, cy + t], fill=FOOT)
        if kind == "+":
            d.rectangle([cx - t, cy - l, cx + t, cy + l], fill=FOOT)
        return
    d.text((cx, cy + 2 * scale), kind, font=font(round(34 * scale)), fill=FOOT, anchor="mm")


def handheld_controls(handheld: Path = HANDHELD) -> Image.Image:
    """The author's Switch in the middle, VCSNX's labels for the left Joy-Con's controls on the left and the right
    one's on the right, each with a line to its control."""
    ss = 2  # drawn at twice the size, then reduced
    band_w, band_h = BAND[0] * ss, BAND[1] * ss
    image = Image.new("RGBA", (band_w, band_h), (0, 0, 0, 0))
    switch = Image.open(handheld).convert("RGBA")
    k = (BAND[1] - 64) * ss / switch.height
    sw, sh = round(switch.width * k), round(switch.height * k)
    ox, oy = (band_w - sw) // 2, (band_h - sh) // 2
    image.alpha_composite(switch.resize((sw, sh), Image.LANCZOS), (ox, oy))
    d = ImageDraw.Draw(image)
    foot_font, car_font = font(36 * ss), font(29 * ss)

    def column(controls, left: bool) -> None:
        rows = len(controls)
        step = band_h / rows
        badge_x = (ox - 64 * ss) if left else (ox + sw + 64 * ss)
        text_x = badge_x - 58 * ss if left else badge_x + 58 * ss
        anchor = "r" if left else "l"
        for i, control in enumerate(controls):
            cy = step * (i + 0.5)
            bx, by = ox + control.point[0] * k, oy + control.point[1] * k
            d.line([(badge_x + (40 if left else -40) * ss, cy), (bx, by)], fill=PINK, width=4 * ss)
            d.ellipse([bx - 9 * ss, by - 9 * ss, bx + 9 * ss, by + 9 * ss], fill=PINK)
            draw_badge(d, badge_x, cy, control.badge, ss)
            if control.in_car:
                d.text((text_x, cy - 18 * ss), control.on_foot, font=foot_font, fill=FOOT, anchor=anchor + "m")
                d.text((text_x, cy + 20 * ss), "Car: " + control.in_car, font=car_font, fill=CAR, anchor=anchor + "m")
            else:
                d.text((text_x, cy), control.on_foot, font=foot_font, fill=FOOT, anchor=anchor + "m")

    column(CONTROLS_LEFT, True)
    column(CONTROLS_RIGHT, False)
    return image.resize(PICTURE, Image.LANCZOS)


def legend_button(label: str) -> Image.Image:
    c = Canvas(64, 64)
    c.ellipse(32, 32, 28, 28, (36, 36, 40, 255), (150, 150, 156, 255), 4)
    c.text(32, 33, label, 34, GLYPH)
    return c.result()


def write_textures(out: Path) -> dict[str, tuple[int, int]]:
    """Writes the three pictures into `out` as <key>.png; returns each key's size."""
    out.mkdir(parents=True, exist_ok=True)
    images = {
        CONTROLS_PSP: handheld_controls(),
        LEGEND_CROSS: legend_button("A"),
        LEGEND_CIRCLE: legend_button("B"),
    }
    for name, image in images.items():
        image.save(out / f"{name}.png")
    return {name: image.size for name, image in images.items()}


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__.strip().splitlines()[-4], file=sys.stderr)
        return 2
    for name, (width, height) in write_textures(Path(sys.argv[1])).items():
        print(f"{name}.png {width}x{height}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
