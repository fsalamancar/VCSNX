"""Tests for tools/make_module_imports.py (stdlib unittest). Run: python3 -I tools/tests/test_make_module_imports.py"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from make_module_imports import assembly, own_unwinder, undefined_symbols  # noqa: E402

NM = """
                 U _ZN9psprecomp11GuestMemory15aot_store8_slowEjh
                 U memcpy
                 w fixture_weak_missing
0000000000001000 T vcsnx_register_generated_functions
                 U memcpy
"""

# nm --defined-only of a module linked with -lgcc: libgcc.a's unwinder came along as local symbols.
NM_WITH_LIBGCC = """
0000000000002000 t _Unwind_Find_FDE
0000000000002400 t _Unwind_Resume
0000000000001000 T vcsnx_register_generated_functions
"""


class MakeModuleImportsTest(unittest.TestCase):
    def test_only_strong_undefined_symbols_once_each(self) -> None:
        self.assertEqual(undefined_symbols(NM), ["_ZN9psprecomp11GuestMemory15aot_store8_slowEjh", "memcpy"])

    def test_a_module_carrying_its_own_unwinder_is_caught(self) -> None:
        # Its cleanups would call that private _Unwind_Resume, which knows none of the registered frames.
        self.assertEqual(own_unwinder(NM_WITH_LIBGCC), ["_Unwind_Find_FDE", "_Unwind_Resume"])
        self.assertEqual(own_unwinder(NM), [])

    def test_the_table_pairs_names_and_addresses_and_ends_with_zeros(self) -> None:
        text = assembly(["memcpy", "sinf"], "vcs_game.so")
        self.assertIn(".global vcsnx_module_imports", text)
        self.assertIn(".quad .Lvcsnx_import_0, memcpy", text)
        self.assertIn(".quad .Lvcsnx_import_1, sinf", text)
        self.assertIn('.Lvcsnx_import_1: .asciz "sinf"', text)
        self.assertLess(text.index(".quad .Lvcsnx_import_1, sinf"), text.index(".quad 0, 0"))


if __name__ == "__main__":
    unittest.main()
