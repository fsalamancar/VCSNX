"""Draws the README banner (assets/readme/banner.png): a neon-80s sunset with "GTA: Vice City Stories",
"Nintendo Switch" and "bAlo productions". Everything is drawn here; no game art is used.

Fonts (SIL Open Font License 1.1, from https://github.com/google/fonts): Neonderthaw, Audiowide and Pacifico.

Usage: python3 -I tools/make_readme_banner.py <Neonderthaw-Regular.ttf> <Audiowide-Regular.ttf> <Pacifico-Regular.ttf> <out.png>
"""

import logging
import math
import random
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

logger = logging.getLogger(__name__)

WIDTH, HEIGHT = 1280, 400
HORIZON = 268
SKY = [(10, 2, 30), (38, 8, 70), (120, 20, 110), (255, 60, 130)]
PINK = (255, 46, 136)
CYAN = (40, 230, 255)
GRID = (255, 40, 160)
NIGHT = (8, 0, 18)


def lerp(a: tuple[int, ...], b: tuple[int, ...], t: float) -> tuple[int, ...]:
    """Mixes two colours, t in 0..1."""
    return tuple(round(x + (y - x) * t) for x, y in zip(a, b))


def gradient(stops: list[tuple[int, int, int]], t: float) -> tuple[int, ...]:
    """The colour at t (0..1) along evenly spaced stops."""
    t = min(max(t, 0.0), 1.0) * (len(stops) - 1)
    i = min(int(t), len(stops) - 2)
    return lerp(stops[i], stops[i + 1], t - i)


def sky() -> Image.Image:
    """Night sky fading to a pink horizon, with stars, and a dark floor below the horizon."""
    image = Image.new("RGB", (WIDTH, HEIGHT), NIGHT)
    draw = ImageDraw.Draw(image)
    for y in range(HORIZON):
        draw.line([(0, y), (WIDTH, y)], fill=gradient(SKY, (y / HORIZON) ** 1.6))
    stars = random.Random(1986)
    for _ in range(140):
        x, y = stars.randrange(WIDTH), stars.randrange(int(HORIZON * 0.6))
        level = stars.randrange(120, 256)
        draw.point((x, y), fill=(level, level, level))
    for y in range(HORIZON, HEIGHT):
        draw.line([(0, y), (WIDTH, y)], fill=lerp((60, 6, 60), NIGHT, ((y - HORIZON) / (HEIGHT - HORIZON)) ** 0.5))
    return image


def sun(image: Image.Image) -> None:
    """The striped retro sun sitting on the horizon."""
    cx, cy, radius = WIDTH // 2, HORIZON - 40, 150
    layer = Image.new("RGB", image.size)
    draw = ImageDraw.Draw(layer)
    for y in range(cy - radius, HORIZON):
        t = (y - (cy - radius)) / (2 * radius)
        draw.line([(0, y), (WIDTH, y)], fill=gradient([(255, 236, 90), (255, 150, 40), (255, 50, 120)], t))
    mask = Image.new("L", image.size, 0)
    shape = ImageDraw.Draw(mask)
    shape.ellipse([cx - radius, cy - radius, cx + radius, cy + radius], fill=255)
    shape.rectangle([0, HORIZON, WIDTH, HEIGHT], fill=0)
    y, gap = cy - 10, 3
    while y < HORIZON:
        shape.rectangle([0, y, WIDTH, y + gap], fill=0)
        y += 18
        gap += 2
    glow = mask.filter(ImageFilter.GaussianBlur(28))
    image.paste(Image.new("RGB", image.size, (255, 80, 140)), (0, 0), glow.point(lambda v: v * 0.55))
    image.paste(layer, (0, 0), mask)


def grid(image: Image.Image) -> None:
    """A neon floor grid running to the vanishing point at the sun."""
    layer = Image.new("RGB", image.size, (0, 0, 0))
    draw = ImageDraw.Draw(layer)
    depth = HEIGHT - HORIZON
    for i in range(1, 14):
        y = HORIZON + depth * (i / 13) ** 2.2
        draw.line([(0, y), (WIDTH, y)], fill=GRID, width=2)
    vx = WIDTH / 2
    for i in range(-24, 25):
        x = vx + i * 120
        draw.line([(vx + i * 6, HORIZON), (x, HEIGHT)], fill=GRID, width=2)
    fade = Image.new("L", image.size, 0)
    fd = ImageDraw.Draw(fade)
    for y in range(HORIZON, HEIGHT):
        fd.line([(0, y), (WIDTH, y)], fill=round(255 * ((y - HORIZON) / depth) ** 0.6))
    layer = ImageChops.multiply(layer, Image.merge("RGB", [fade] * 3))
    image.paste(ImageChops.screen(image, layer.filter(ImageFilter.GaussianBlur(4))), (0, 0))
    image.paste(ImageChops.screen(image, layer), (0, 0))


def palm(draw: ImageDraw.ImageDraw, base_x: float, height: float, lean: float, flip: int) -> None:
    """A palm silhouette: a curved tapering trunk and drooping fronds."""
    points = []
    for i in range(41):
        t = i / 40
        points.append((base_x + flip * lean * t * t, HEIGHT - height * t))
    for i in range(40):
        (x0, y0), (x1, y1) = points[i], points[i + 1]
        width = 18 - 11 * i / 40
        draw.line([(x0, y0), (x1, y1)], fill=NIGHT, width=round(width))
    crown = points[-1]
    for angle in (-170, -140, -110, -70, -40, -10, 20, 200, 160):
        a = math.radians(angle)
        length = 120 + 30 * math.cos(a * 3)
        frond = []
        for i in range(25):
            t = i / 24
            x = crown[0] + math.cos(a) * length * t
            y = crown[1] + math.sin(a) * length * t + 70 * t * t
            frond.append((x, y))
        for i, (x, y) in enumerate(frond[:-1]):
            draw.line([(x, y), frond[i + 1]], fill=NIGHT, width=max(2, round(9 * (1 - i / 24))))
            if i % 2 == 0 and i > 2:
                nx, ny = -math.sin(a), math.cos(a)
                leaf = 26 * (1 - i / 26)
                draw.line([(x, y), (x + nx * leaf * 0.6 + 6, y + abs(ny) * leaf + 10)], fill=NIGHT, width=3)
                draw.line([(x, y), (x - nx * leaf * 0.6 + 6, y + abs(ny) * leaf + 10)], fill=NIGHT, width=3)


def neon_text(image: Image.Image, xy: tuple[float, float], text: str, font: ImageFont.FreeTypeFont, colour: tuple[int, int, int],
              spacing: float = 0.0) -> None:
    """Text like a neon tube on a dark halo: wide soft glow, tight glow, coloured tube and a white-hot core, centred
    on xy. The glow scales with the font size."""
    letters = list(text)
    widths = [font.getlength(c) for c in letters]
    total = sum(widths) + spacing * (len(letters) - 1)
    x0, y = xy[0] - total / 2, xy[1]

    def write(draw: ImageDraw.ImageDraw, fill: tuple[int, ...], stroke: int) -> None:
        x = x0
        for c, w in zip(letters, widths):
            draw.text((x, y), c, font=font, fill=fill, anchor="ls", stroke_width=stroke, stroke_fill=fill)
            x += w + spacing

    size = font.size
    halo = Image.new("L", image.size, 0)
    write(ImageDraw.Draw(halo), (255,), max(2, round(size * 0.06)))
    image.paste(Image.new("RGB", image.size, NIGHT), (0, 0), halo.filter(ImageFilter.GaussianBlur(size * 0.12)).point(lambda v: round(v * 0.7)))
    tube = Image.new("RGB", image.size, (0, 0, 0))
    write(ImageDraw.Draw(tube), colour, max(1, round(size * 0.015)))
    soft = 0.8 if size >= 40 else 0.45
    for radius, strength in ((size * 0.18, soft), (size * 0.07, soft + 0.1), (size * 0.02, 1.0)):
        glow = tube.filter(ImageFilter.GaussianBlur(radius))
        glow = glow.point(lambda v, s=strength: min(255, round(v * s * 1.6)))
        image.paste(ImageChops.screen(image, glow), (0, 0))
    image.paste(ImageChops.screen(image, tube), (0, 0))
    hot = Image.new("RGB", image.size, (0, 0, 0))
    write(ImageDraw.Draw(hot), lerp(colour, (255, 255, 255), 0.75), 0)
    mask = Image.new("L", image.size, 0)
    write(ImageDraw.Draw(mask), (255,), 0)
    image.paste(hot, (0, 0), mask.filter(ImageFilter.MinFilter(3)) if size >= 60 else mask)  # thin strokes keep their core


def banner(script: Path, caps: Path, signature: Path) -> Image.Image:
    """The whole banner."""
    image = sky()
    sun(image)
    grid(image)
    draw = ImageDraw.Draw(image)
    palm(draw, 95, 330, 60, 1)
    palm(draw, 20, 250, 45, 1)
    palm(draw, WIDTH - 70, 315, 70, -1)
    neon_text(image, (WIDTH / 2, 96), "GTA", ImageFont.truetype(str(caps), 60), CYAN, spacing=30)
    neon_text(image, (WIDTH / 2, 214), "Vice City Stories", ImageFont.truetype(str(script), 132), PINK)
    neon_text(image, (WIDTH / 2, 328), "NINTENDO SWITCH", ImageFont.truetype(str(caps), 40), CYAN, spacing=14)
    neon_text(image, (WIDTH - 160, 382), "bAlo productions", ImageFont.truetype(str(signature), 30), (255, 214, 110))
    return image


def main(argv: list[str]) -> int:
    """Writes the banner to argv[4]."""
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    if len(argv) != 5:
        logger.error(__doc__)
        return 2
    out = Path(argv[4])
    out.parent.mkdir(parents=True, exist_ok=True)
    banner(Path(argv[1]), Path(argv[2]), Path(argv[3])).save(out, optimize=True)
    logger.info("%s: %d bytes", out, out.stat().st_size)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
