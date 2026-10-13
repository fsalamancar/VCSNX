"""Tests for tools/make_builder_kit.py (stdlib unittest). Run: python3 -I tools/tests/test_make_builder_kit.py"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from make_builder_kit import compile_template, module_link_template  # noqa: E402

REPO = "/Users/x/VCSNX"
GXX = "/opt/devkitpro/devkitA64/bin/aarch64-none-elf-g++"
AOT_COMPILE = [
    GXX, f"-I{REPO}/generated", f"-I{REPO}/vendor/vcs-recomp/profiles/vcs/host", f"-I{REPO}/vendor/vcs-recomp/include",
    "-isystem", "/opt/devkitpro/libnx/include", "-march=armv8-a+crc+crypto", "-std=c++20", "-fPIC", "-O2", "-g0", "-w",
    "-ffp-contract=off", "-MD", "-MT", "CMakeFiles/vcsnx_aot.dir/generated/generated_unit_0000.o",
    "-MF", "CMakeFiles/vcsnx_aot.dir/generated/generated_unit_0000.o.d",
    "-o", "CMakeFiles/vcsnx_aot.dir/generated/generated_unit_0000.o", "-c", f"{REPO}/generated/generated_unit_0000.cpp",
]
BUILD = "/Users/x/VCSNX/build/switch"
# As ninja -t commands prints it (CMake writes the object paths with "/./"); no -lgcc: the module imports
# _Unwind_Resume from the runtime, and module_eh_end.o comes last.
MODULE_LINK = [
    "cd", BUILD, "&&", GXX, "-march=armv8-a+crc+crypto", "-mtune=cortex-a57", "-fPIC", "-shared", "-nostdlib",
    "-Wl,-Bsymbolic", "-Wl,--build-id=sha1",
    f"{BUILD}/CMakeFiles/vcsnx_aot.dir/./generated/generated_registry.o",
    f"{BUILD}/CMakeFiles/vcsnx_aot.dir/./generated/generated_unit_0000.o",
    f"{BUILD}/CMakeFiles/vcsnx_module_entry.dir/./platform/switch/module_entry.o",
    f"{BUILD}/CMakeFiles/vcsnx_module_eh_end.dir/./platform/switch/module_eh_end.o",
    "-o", f"{BUILD}/vcs_game.so",
]


class MakeBuilderKitTest(unittest.TestCase):
    def test_compile_template_keeps_the_flags_and_names_the_paths(self) -> None:
        self.assertEqual(compile_template(AOT_COMPILE, REPO), [
            "-I{generated}", "-I{kit}/include/host", "-I{kit}/include/psprecomp", "-isystem", "{toolchain}/libnx/include",
            "-march=armv8-a+crc+crypto", "-std=c++20", "-fPIC", "-O2", "-g0", "-w", "-ffp-contract=off",
            "-o", "{output}", "-c", "{source}",
        ])

    def test_the_module_link_puts_the_translated_code_in_one_placeholder_and_our_objects_in_the_kit(self) -> None:
        args, objects, order = module_link_template(MODULE_LINK)
        self.assertEqual(order, ["generated_registry", "generated_unit_0000"])
        self.assertEqual(args, ["-march=armv8-a+crc+crypto", "-mtune=cortex-a57", "-fPIC", "-shared", "-nostdlib",
                                "-Wl,-Bsymbolic", "-Wl,--build-id=sha1", "{generated_objects}",
                                "{kit}/obj/module_entry.o", "{kit}/obj/module_eh_end.o", "-o", "{output}"])
        self.assertEqual(objects, [("obj/module_entry.o", f"{BUILD}/CMakeFiles/vcsnx_module_entry.dir/./platform/switch/module_entry.o"),
                                   ("obj/module_eh_end.o", f"{BUILD}/CMakeFiles/vcsnx_module_eh_end.dir/./platform/switch/module_eh_end.o")])

    def test_an_unknown_path_in_the_link_is_refused(self) -> None:
        with self.assertRaises(SystemExit):
            module_link_template(MODULE_LINK[:-2] + ["/Users/x/elsewhere/libfoo.a", "-o", f"{BUILD}/vcs_game.so"])

    def test_the_end_marker_must_be_the_last_object(self) -> None:
        # A .eh_frame entry after the marker is lost to the unwinder (and ElfModule refuses the module).
        entry, eh_end = MODULE_LINK[-4], MODULE_LINK[-3]
        with self.assertRaises(SystemExit):
            module_link_template(MODULE_LINK[:-4] + [eh_end, entry] + MODULE_LINK[-2:])


if __name__ == "__main__":
    unittest.main()
