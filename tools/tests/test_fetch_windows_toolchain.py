"""Tests for tools/fetch_windows_toolchain.py (stdlib unittest). Run: python3 -I tools/tests/test_fetch_windows_toolchain.py"""
import hashlib
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from fetch_windows_toolchain import PACKAGES, verify  # noqa: E402


class FetchWindowsToolchainTest(unittest.TestCase):
    def test_verify_accepts_the_right_hash(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "pkg"
            path.write_bytes(b"package")
            self.assertTrue(verify(path, hashlib.sha256(b"package").hexdigest()))

    def test_verify_rejects_a_wrong_hash(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "pkg"
            path.write_bytes(b"tampered")
            self.assertFalse(verify(path, hashlib.sha256(b"package").hexdigest()))

    def test_only_what_building_the_module_needs_is_pinned(self) -> None:
        names = [name for _, name, _ in PACKAGES]
        self.assertEqual(names, ["devkita64-gcc-16.1.0-1-x86_64.pkg.tar.zst",
                                 "devkita64-binutils-2.46.0-1-x86_64.pkg.tar.zst",
                                 "devkita64-newlib-4.6.0.20260123-4-any.pkg.tar.zst"])
        for _, _, sha in PACKAGES:
            self.assertRegex(sha, r"^[0-9a-f]{64}$")


if __name__ == "__main__":
    unittest.main()
