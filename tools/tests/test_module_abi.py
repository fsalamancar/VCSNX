"""Tests for tools/module_abi.py (stdlib unittest). Run: python3 -I tools/tests/test_module_abi.py"""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from module_abi import abi_of, parse_depfile  # noqa: E402


class ModuleAbiTest(unittest.TestCase):
    def test_parse_depfile_lists_every_dependency_once(self) -> None:
        text = "a.o: /r/generated/a.cpp /r/vendor/x.hpp \\\n /r/vendor/y.hpp\nb.o: /r/generated/b.cpp \\\n /r/vendor/x.hpp\n"
        self.assertEqual(parse_depfile(text), ["/r/generated/a.cpp", "/r/generated/b.cpp", "/r/vendor/x.hpp", "/r/vendor/y.hpp"])

    def test_the_fingerprint_follows_header_contents_not_their_order(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp)
            (repo / "vendor").mkdir()
            x, y = repo / "vendor" / "x.hpp", repo / "vendor" / "y.hpp"
            x.write_text("struct X { int a; };")
            y.write_text("struct Y {};")
            first = abi_of([x, y], repo)
            self.assertEqual(first, abi_of([y, x], repo))
            self.assertRegex(first, r"^[0-9a-f]{64}$")
            x.write_text("struct X { int a; int b; };")
            self.assertNotEqual(first, abi_of([x, y], repo))


if __name__ == "__main__":
    unittest.main()
