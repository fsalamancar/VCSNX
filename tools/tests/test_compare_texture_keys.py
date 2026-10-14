# tests/tools/test_compare_texture_keys.py
"""compare_texture_keys: which of the textures the console dumped the Builder's list also has."""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOL = Path(__file__).resolve().parents[1] / "compare_texture_keys.py"


class CompareTest(unittest.TestCase):
    def test_counts_matches_per_source_and_lists_unmatched_sizes(self):
        with tempfile.TemporaryDirectory() as tmp:
            listing = Path(tmp, "list.txt")
            listing.write_text("00000000000000aa 64 64 GTA3PSP.IMG car\n00000000000000bb 32 32 LOADSC0.XTX s\n")
            dumped = Path(tmp, "dumped.txt")
            dumped.write_text("00000000000000aa.png\n00000000000000cc.png\n00000000000000aa.png.usage\n")
            out = subprocess.run([sys.executable, "-I", str(TOOL), str(listing), str(dumped)],
                                 capture_output=True, text=True, check=True).stdout
            self.assertIn("dumped 2", out)
            self.assertIn("matched 1 (GTA3PSP.IMG 1)", out)
            self.assertIn("unmatched: 00000000000000cc", out)


if __name__ == "__main__":
    unittest.main()
