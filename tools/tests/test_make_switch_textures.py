"""The Settings > Controls picture: the author's Switch with VCSNX's labels, drawn for the band it fills."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import make_switch_textures as tex  # noqa: E402

HANDHELD = Path(__file__).resolve().parents[2] / "assets" / "controls" / "switch_handheld.png"


def test_the_picture_is_8x_the_games_and_transparent_around() -> None:
    image = tex.handheld_controls(HANDHELD)
    assert image.size == (1024 * 2, 512 * 2)  # 8x the game's 256 x 128, a power-of-two multiple
    assert image.mode == "RGBA"
    assert image.getpixel((0, 0))[3] == 0 and image.getpixel((image.width - 1, image.height - 1))[3] == 0


def test_the_switch_is_in_the_middle_and_labelled_on_both_sides() -> None:
    image = tex.handheld_controls(HANDHELD)
    w, h = image.size
    alpha = image.getchannel("A")
    middle = alpha.crop((w * 2 // 5, h // 3, w * 3 // 5, h * 2 // 3))
    assert middle.getextrema()[1] == 255, "the console in the middle"
    for x0, x1 in ((0, w // 5), (w * 4 // 5, w)):
        column = alpha.crop((x0, 0, x1, h))
        assert sum(1 for a in column.getdata() if a > 128) > column.width * column.height // 50, "labels"


def test_every_control_has_a_label_for_on_foot_and_in_car() -> None:
    rows = tex.CONTROLS_LEFT + tex.CONTROLS_RIGHT
    assert len(rows) == 15
    for row in rows:
        assert row.badge and row.on_foot, row
    in_car = {row.badge: row.in_car for row in rows}
    assert in_car["ZR"] == "Accelerate" and in_car["ZL"] == "Brake / Reverse"
    assert in_car["R"] == "Look right" and in_car["L"] == "Look left"


def test_the_three_pictures_are_written_under_their_keys(tmp_path: Path) -> None:
    sizes = tex.write_textures(tmp_path)
    assert sizes == {
        tex.CONTROLS_PSP: (2048, 1024), tex.LEGEND_CROSS: (64, 64), tex.LEGEND_CIRCLE: (64, 64),
    }
    assert sorted(p.name for p in tmp_path.iterdir()) == sorted(f"{key}.png" for key in sizes)
