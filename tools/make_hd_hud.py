"""Makes 4x replacements of the HUD fonts from the user's own texture dump, so HUD text is sharp at 720p.

The PSP draws its 2D in 480x272, so on the Switch every glyph is stretched 2.67x and looks soft or
blocky. The fork's automatic upscaler skips 2D art on purpose (vcs_texture_replace.cpp), but it loads
Textures/<hash>.png replacements that are exactly 2x/4x/8x the original. This upscales each font atlas
4x and rebuilds the glyph edges: the alpha is resampled, smoothed and pushed through a steep curve
around the edge, so outlines come out crisp and the PSP's stair-steps come out round. Colours are
spread into the transparent texels first, so no dark fringe creeps in. The atlas layout (where each
glyph sits) is unchanged, which is what the game's text code relies on.

The fonts are the game's: the PNGs this writes are derived from the user's copy and stay out of git.
Usage: python3 -I tools/make_hd_hud.py <Textures/originals from a DumpOriginals run> <output folder>
Then copy the PNGs into the game's Textures folder (sdmc:/switch/vcsnx/Textures on the Switch).
"""
import sys
from pathlib import Path

from PIL import Image, ImageFilter

SCALE = 4
EDGE_SMOOTH = 0.55  # Gaussian radius in source texels: rounds stair-steps without losing detail
EDGE_WIDTH = 0.10   # half-width of the alpha ramp around the edge: smaller is crisper

HUD_FONTS = {
    "0bbb937679e08a99": "help boxes and subtitles (256x256)",
    "b452c921f0ad4b7c": "script font: zone and vehicle names (256x256)",
    "52c208aead627131": "menu titles (256x256)",
    "c80445fc6052f327": "money and clock digits (128x64)",
}


def bleed(image: Image.Image) -> Image.Image:
    """Spreads glyph colours into transparent texels, so resampling never mixes in their black RGB."""
    rgb = image.convert("RGB")
    solid = image.getchannel("A").point(lambda v: 255 if v > 0 else 0)
    for _ in range(4):
        grown = rgb.filter(ImageFilter.MaxFilter(3))
        rgb = Image.composite(rgb, grown, solid)
        solid = solid.filter(ImageFilter.MaxFilter(3))
    return rgb


def smoothstep_curve(width: float):
    lo, hi = 255 * (0.5 - width), 255 * (0.5 + width)

    def curve(value: int) -> int:
        t = min(1.0, max(0.0, (value - lo) / (hi - lo)))
        return round(255 * t * t * (3 - 2 * t))

    return curve


def upscale_font(image: Image.Image) -> Image.Image:
    image = image.convert("RGBA")
    size = (image.size[0] * SCALE, image.size[1] * SCALE)
    alpha = image.getchannel("A").resize(size, Image.BICUBIC)
    alpha = alpha.filter(ImageFilter.GaussianBlur(SCALE * EDGE_SMOOTH)).point(smoothstep_curve(EDGE_WIDTH))
    rgb = bleed(image).resize(size, Image.BICUBIC).filter(ImageFilter.GaussianBlur(SCALE * 0.25))
    out = rgb.convert("RGBA")
    out.putalpha(alpha)
    return out


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: make_hd_hud.py <Textures/originals> <output folder>", file=sys.stderr)
        return 2
    originals, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    missing = 0
    for name, what in HUD_FONTS.items():
        source = originals / f"{name}.png"
        if not source.exists():
            print(f"missing {source.name} ({what}): play until it appears with DumpOriginals=true", file=sys.stderr)
            missing += 1
            continue
        result = upscale_font(Image.open(source))
        result.save(out / source.name)
        print(f"{source.name} {result.size[0]}x{result.size[1]}: {what}")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
