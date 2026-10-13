"""Tests for tools/package_builder.py (stdlib unittest). Run: python3 -I tools/tests/test_package_builder.py"""
import hashlib
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from package_builder import REQUIRED_LICENSES, audit, local_path_leaks, missing_licenses, runtime_icon_problem  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]


def nro(icon: bytes, nacp: bytes = b"nacp") -> bytes:
    """A minimal NRO: 0x100 bytes of program ("NRO0" at 0x10, its size at 0x18), then the ASET with the icon."""
    program = bytearray(0x100)
    program[0x10:0x14] = b"NRO0"
    struct.pack_into("<I", program, 0x18, len(program))
    header = bytearray(0x38)
    header[0:4] = b"ASET"
    struct.pack_into("<QQQQ", header, 0x08, 0x38, len(icon), 0x38 + len(icon), len(nacp))
    return bytes(program) + bytes(header) + icon + nacp


class PackageBuilderTest(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def tree(self, files: dict[str, bytes]) -> Path:
        root = Path(self._tmp.name)
        for name, data in files.items():
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        return root

    def test_a_clean_tree_with_the_runtime_nro_passes(self) -> None:
        root = self.tree({"VCSNX-Builder.exe": b"exe", "kit/obj/module_entry.o": b"o", "kit/vcsnx.nro": b"runtime",
                          "kit/module_link.json": b"generated_registry"})
        self.assertEqual(audit(root, set()), [])

    def test_game_files_the_module_and_generated_code_are_refused_by_name(self) -> None:
        root = self.tree({"kit/generated/generated_unit_0001.cpp": b"x", "salida/vcs_game.so": b"x", "PSP_DATA/PSP_GAME/x": b"x",
                          "local/cover.png": b"x", "other.nro": b"x"})
        self.assertEqual(len(audit(root, set())), 5)

    def test_a_renamed_game_file_is_refused_by_its_hash(self) -> None:
        root = self.tree({"kit/innocent.bin": b"translated code"})
        self.assertEqual(audit(root, {hashlib.sha256(b"translated code").hexdigest()}), ["kit/innocent.bin: matches a game file's hash"])

    def test_the_published_runtime_carries_only_an_allowed_icon(self) -> None:
        # A dev build embeds local/icon.jpg, cut from the game's cover: that must never reach the release.
        default = hashlib.sha256(b"libnx default").hexdigest()
        self.assertIsNone(runtime_icon_problem(nro(b"libnx default"), {default}))
        self.assertIn("icon", runtime_icon_problem(nro(b"cover art"), {default}) or "")
        self.assertIn("not an NRO", runtime_icon_problem(b"not an nro at all", {default}) or "")


    def test_a_release_without_every_license_text_is_refused(self) -> None:
        self.assertEqual([n for n in REQUIRED_LICENSES if n != "THIRD_PARTY.md"], missing_licenses(self.tree({"THIRD_PARTY.md": b"list"})))
        self.assertIn("GPL-3.0.txt", REQUIRED_LICENSES)
        self.assertIn("so_util-LICENSE.txt", REQUIRED_LICENSES)

    def test_the_repository_carries_every_license_text(self) -> None:
        self.assertEqual([], missing_licenses(ROOT / "builder" / "licenses"))

    def test_files_that_carry_the_build_machines_home_folder_are_refused(self) -> None:
        root = self.tree({"kit/obj/module_entry.o": b"\x7fELF..DW_AT_comp_dir./Users/someone/VCSNX/build..",
                          "VCSNX-Builder.exe": b"MZ..C:/_/builder/Builder.App/Program.cs..", "README.txt": b"/Users/x/VCSNX"})
        self.assertEqual(["kit/obj/module_entry.o: carries /Users/someone"], local_path_leaks(root, [b"/Users/someone"]))


if __name__ == "__main__":
    unittest.main()
