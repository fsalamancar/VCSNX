"""Builds the VCSNX Builder's kit from the split Switch build (spec §3): the exact compile line of the
translated code and link line of vcs_game.so (with placeholders), the module's own entry objects, the
headers the translated code includes, the interface fingerprint, the runtime vcsnx.nro the Builder puts
the player's icon into, and VCSNX's own menu pictures (Textures/). Nothing in the kit comes from the game.
Usage: python3 -I tools/make_builder_kit.py <build/switch> <repo> <output kit folder>
"""
import json
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from module_abi import abi_of, generated_dependencies  # noqa: E402
from make_switch_textures import write_textures  # noqa: E402

DKP = "/opt/devkitpro"
GXX = f"{DKP}/devkitA64/bin/aarch64-none-elf-g++"
GCC_VERSION = "16.1.0"
# Our own objects in vcs_game.so; the end of .eh_frame must stay the last one (module_eh_end.S).
KIT_OBJECT_DIRS = ("vcsnx_module_entry.dir/", "vcsnx_module_eh_end.dir/")
EH_END = "module_eh_end.o"


def compile_template(args: list[str], repo: str) -> list[str]:
    out: list[str] = []
    it = iter(args[1:])  # the compiler itself is the Builder's toolchain
    for arg in it:
        if arg == "-MD":
            continue
        if arg in ("-MT", "-MF"):
            next(it)
            continue
        if arg == "-o":
            next(it)
            out += ["-o", "{output}"]
        elif arg == "-c":
            next(it)
            out += ["-c", "{source}"]
        elif arg == f"-I{repo}/generated":
            out.append("-I{generated}")
        elif arg == f"-I{repo}/vendor/vcs-recomp/profiles/vcs/host":
            out.append("-I{kit}/include/host")
        elif arg == f"-I{repo}/vendor/vcs-recomp/include":
            out.append("-I{kit}/include/psprecomp")
        else:
            out.append(arg.replace(DKP, "{toolchain}"))
    return out


def module_link_template(args: list[str]) -> tuple[list[str], list[tuple[str, str]], list[str]]:
    start = next(i for i, a in enumerate(args) if a.endswith("aarch64-none-elf-g++"))
    out: list[str] = []
    objects: list[tuple[str, str]] = []
    order: list[str] = []
    it = iter(args[start + 1:])
    for arg in it:
        if "vcsnx_aot.dir/" in arg:
            if not order:
                out.append("{generated_objects}")
            order.append(Path(arg).stem)
        elif any(d in arg for d in KIT_OBJECT_DIRS):
            dest = f"obj/{Path(arg).name}"
            objects.append((dest, arg))
            out.append("{kit}/" + dest)
        elif arg == "-o":
            next(it)
            out += ["-o", "{output}"]
        else:
            out.append(arg)
    leftovers = [a for a in out if a.startswith("/") or DKP in a]
    if leftovers:
        raise SystemExit(f"module link line has paths the kit does not know: {leftovers}")
    if out[-3:] != ["{kit}/obj/" + EH_END, "-o", "{output}"]:
        raise SystemExit(f"{EH_END} must be the last object of the module link: {out[-4:]}")
    return out, objects, order


def add_switch_textures(kit: Path) -> list[str]:
    """VCSNX's own pictures for the game's menus (Settings > Controls, the menu legend's A and B), drawn by
    tools/make_switch_textures.py from assets/controls; the Builder copies kit/Textures to switch/vcsnx/Textures."""
    return sorted(f"{name}.png" for name in write_textures(kit / "Textures"))


def main() -> int:
    if len(sys.argv) != 4:
        print("usage: make_builder_kit.py <build/switch> <repo> <output kit folder>", file=sys.stderr)
        return 2
    build, repo, kit = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve(), Path(sys.argv[3]).resolve()
    gcc = subprocess.run([GXX, "-dumpfullversion"], capture_output=True, text=True, check=True).stdout.strip()
    if gcc != GCC_VERSION:
        raise SystemExit(f"the kit must be built with GCC {GCC_VERSION} (the Windows toolchain's), found {gcc}")
    commands = subprocess.run(["ninja", "-C", str(build), "-t", "commands", "vcs_game.so"],
                              capture_output=True, text=True, check=True).stdout.splitlines()
    aot = next(shlex.split(c) for c in commands if "vcsnx_aot.dir/generated/generated_unit_0000.o" in c and " -c " in c)
    link = shlex.split(commands[-1])
    headers = generated_dependencies(GXX, repo)
    abi = abi_of(headers, repo)
    baked = re.search(r"^VCSNX_MODULE_ABI:INTERNAL=([0-9a-f]{64})$", (build / "CMakeCache.txt").read_text(), re.M)
    if not baked or baked.group(1) != abi:
        raise SystemExit("build/switch was configured with another interface fingerprint: reconfigure and rebuild it first")
    if kit.exists():
        shutil.rmtree(kit)
    (kit / "obj").mkdir(parents=True)
    link_args, objects, order = module_link_template(link)
    for dest, source in objects:
        shutil.copy2(source if Path(source).is_absolute() else build / source, kit / dest)
    roots = {f"{repo}/vendor/vcs-recomp/profiles/vcs/host/": "include/host/", f"{repo}/vendor/vcs-recomp/include/": "include/psprecomp/"}
    for header in headers:
        prefix = next((p for p in roots if str(header).startswith(p)), None)
        if prefix is None:
            raise SystemExit(f"the translated code includes a header outside the kit's include roots: {header}")
        target = kit / roots[prefix] / str(header)[len(prefix):]
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(header, target)
    (kit / "compile.json").write_text(json.dumps(compile_template(aot, str(repo)), indent=1))
    (kit / "module_link.json").write_text(json.dumps({"args": link_args, "generated_order": order}, indent=1))
    version = re.search(r"project\(VCSNX VERSION (\d+\.\d+\.\d+)", (repo / "CMakeLists.txt").read_text()).group(1)
    (kit / "kit.json").write_text(json.dumps({"version": version, "gcc": gcc, "abi": abi}, indent=1))
    shutil.copy2(build / "vcsnx.nro", kit / "vcsnx.nro")
    shutil.copy2(repo / "config" / "VCSNative.ini", kit / "VCSNative.ini")
    textures = add_switch_textures(kit)
    size = sum(p.stat().st_size for p in kit.rglob("*") if p.is_file()) // (1 << 20)
    print(f"kit: {len(order)} translated units to build, {len(objects)} module objects, {len(headers)} headers, "
          f"{len(textures)} textures, abi {abi[:12]}, {size} MiB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
