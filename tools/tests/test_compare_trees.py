"""Tests for tools/compare_trees.py (stdlib unittest). Run: python3 -I tools/tests/test_compare_trees.py"""
import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from compare_trees import compare_trees, main  # noqa: E402


def write(root: Path, name: str, data: bytes) -> None:
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


class CompareTreesTest(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        base = Path(self._tmp.name)
        self.ref, self.cand = base / "ref", base / "cand"
        self.ref.mkdir()
        self.cand.mkdir()

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def test_identical_trees_match(self) -> None:
        for root in (self.ref, self.cand):
            write(root, "a.cpp", b"int a;")
            write(root, "sub/b.hpp", b"#pragma once")
        result = compare_trees(self.ref, self.cand)
        self.assertTrue(result.matches)
        self.assertEqual(result.identical, ("a.cpp", "sub/b.hpp"))

    def test_one_changed_byte_is_reported(self) -> None:
        write(self.ref, "a.cpp", b"int a;")
        write(self.cand, "a.cpp", b"int b;")
        result = compare_trees(self.ref, self.cand)
        self.assertFalse(result.matches)
        self.assertEqual(result.different, ("a.cpp",))

    def test_missing_and_extra_files_are_reported(self) -> None:
        write(self.ref, "only_ref.cpp", b"x")
        write(self.cand, "only_cand.cpp", b"y")
        result = compare_trees(self.ref, self.cand)
        self.assertEqual(result.missing, ("only_ref.cpp",))
        self.assertEqual(result.extra, ("only_cand.cpp",))
        self.assertFalse(result.matches)

    def test_cli_exit_codes(self) -> None:
        write(self.ref, "a.cpp", b"same")
        write(self.cand, "a.cpp", b"same")
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(main([str(self.ref), str(self.cand)]), 0)
        write(self.cand, "a.cpp", b"diff")
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(main([str(self.ref), str(self.cand)]), 1)
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(main([str(self.ref), str(self.cand / "nope")]), 2)


if __name__ == "__main__":
    unittest.main()
