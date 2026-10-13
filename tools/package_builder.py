"""Assembles the release files (spec §3, §7): dist/vcsnx.nro (the runtime) and VCSNX-Builder-<version>.zip,
refusing the zip if anything of the game slipped in (names of game files, of the translated code or of the
module, or any file whose SHA-256 is in builder/hashes), and refusing the runtime unless
tools/check_runtime_clean.sh proves it carries none of the translated code and its icon is libnx's default
(a dev build embeds local/icon.jpg, cut from the game's cover: build the release with -DVCSNX_LOCAL_ICON=OFF).
It also refuses a release without every license text in builder/licenses (the toolchain is GPL: its texts
and where its source is must travel with it), or with a file that carries this machine's home folder.
Usage: python3 -I tools/package_builder.py --exe <VCSNX-Builder.exe> --toolchain <Windows toolchain> --kit <kit> --build <build/switch> --out dist
"""
import argparse
import hashlib
import json
import re
import shutil
import struct
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ICON = Path("/opt/devkitpro/libnx/default_icon.jpg")  # what nx_create_nro embeds without ICON
ALLOWED = {"kit/vcsnx.nro"}  # the runtime, checked separately
FORBIDDEN = [re.compile(p, re.IGNORECASE) for p in (
    r"(^|/)generated(/|$)", r"generated_unit_\d+", r"generated_registry\.(cpp|o)$", r"vcs_game\.so", r"\.(nro|nacp)$",
    r"(^|/)PSP_DATA(/|$)", r"(^|/)PSP_GAME(/|$)", r"(^|/)local(/|$)", r"EBOOT", r"(^|/)BOOT\.BIN$", r"\.(pmf|at3|iso|cso|sfo)$",
)]


# builder/licenses → LICENSES/ in the zip; THIRD_PARTY.md says which component each one covers.
REQUIRED_LICENSES = (
    "THIRD_PARTY.md", "SOURCES.md",
    "GPL-3.0.txt", "GPL-2.0.txt", "LGPL-3.0.txt", "GCC-RUNTIME-LIBRARY-EXCEPTION-3.1.txt",
    "newlib-COPYING.NEWLIB.txt", "newlib-COPYING.LIBGLOSS.txt",
    "so_util-LICENSE.txt", "libnx-LICENSE.md", "SDL2-LICENSE.txt", "mesa-license.rst", "mesa-MIT.txt",
    "dav1d-COPYING.txt", "expat-COPYING.txt", "bzip2-LICENSE.txt", "zlib-LICENSE.txt", "zstd-LICENSE.txt",
)


def missing_licenses(folder: Path) -> list[str]:
    """The license files of REQUIRED_LICENSES that `folder` lacks."""
    return [name for name in REQUIRED_LICENSES if not (folder / name).is_file()]


def local_path_leaks(root: Path, markers: list[bytes]) -> list[str]:
    """Files under `root` (a folder or one file) that carry one of `markers` (this machine's home folder,
    e.g. in debug info)."""
    paths = [root] if root.is_file() else sorted(p for p in root.rglob("*") if p.is_file())
    leaks = []
    for path in paths:
        data = path.read_bytes()
        name = path.name if path == root else path.relative_to(root).as_posix()
        leaks += [f"{name}: carries {m.decode()}" for m in markers if m in data]
    return leaks


def audit(root: Path, banned: set[str]) -> list[str]:
    problems = []
    for path in sorted(p for p in root.rglob("*") if p.is_file()):
        rel = path.relative_to(root).as_posix()
        if rel not in ALLOWED and any(p.search(rel) for p in FORBIDDEN):
            problems.append(f"{rel}: name of a game file, of the translated code or of the module")
        elif hashlib.sha256(path.read_bytes()).hexdigest() in banned:
            problems.append(f"{rel}: matches a game file's hash")
    return problems


def runtime_icon_problem(nro: bytes, allowed: set[str]) -> str | None:
    """Why the runtime's HOME-menu icon must not be published, or None. The icon sits in the NRO's asset
    section: "ASET" at the program's size (the word at 0x18), then (offset, size) of the icon."""
    if len(nro) < 0x80 or nro[0x10:0x14] != b"NRO0":
        return "the runtime is not an NRO"
    size = struct.unpack_from("<I", nro, 0x18)[0]
    if nro[size:size + 4] != b"ASET":
        return "the runtime has no asset section"
    offset, length = struct.unpack_from("<QQ", nro, size + 8)
    icon = nro[size + offset:size + offset + length]
    if hashlib.sha256(icon).hexdigest() not in allowed:
        return "the runtime's icon is not libnx's default (local art?): reconfigure with -DVCSNX_LOCAL_ICON=OFF and rebuild"
    return None


def banned_hashes(hashes: Path) -> set[str]:
    return {line.split()[0] for f in ("executable.sha256", "generated.sha256") for line in (hashes / f).read_text().splitlines() if line.strip()}


def main() -> int:
    parser = argparse.ArgumentParser()
    for name in ("exe", "toolchain", "kit", "build", "out"):
        parser.add_argument(f"--{name}", required=True, type=Path)
    args = parser.parse_args()
    subprocess.run([str(ROOT / "tools" / "check_runtime_clean.sh"), str(args.build)], check=True)
    icon_problem = runtime_icon_problem((args.build / "vcsnx.nro").read_bytes(), {hashlib.sha256(DEFAULT_ICON.read_bytes()).hexdigest()})
    if icon_problem:
        print(icon_problem, file=sys.stderr)
        return 3
    if (args.kit / "vcsnx.nro").read_bytes() != (args.build / "vcsnx.nro").read_bytes():
        print("kit/vcsnx.nro is not the runtime that was just checked: rebuild the kit", file=sys.stderr)
        return 3
    missing = missing_licenses(ROOT / "builder" / "licenses")
    if missing:
        print("builder/licenses lacks: " + ", ".join(missing), file=sys.stderr)
        return 3
    version = json.loads((args.kit / "kit.json").read_text())["version"]
    stage = args.out / f"VCSNX-Builder-{version}"
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)
    shutil.copy2(args.exe, stage / "VCSNX-Builder.exe")
    shutil.copytree(args.toolchain, stage / "toolchain")
    shutil.copytree(args.kit, stage / "kit")
    shutil.copytree(ROOT / "builder" / "hashes", stage / "hashes")
    shutil.copytree(ROOT / "builder" / "licenses", stage / "LICENSES")
    shutil.copy2(ROOT / "LICENSE", stage / "LICENSES" / "VCSNX-LICENSE.txt")
    shutil.copy2(ROOT / "vendor/vcs-recomp/LICENSE", stage / "LICENSES" / "PSPRecomp-LICENSE.txt")
    shutil.copy2(ROOT / "vendor/vcs-recomp/profiles/vcs/third_party/ffmpeg/COPYING.LGPLv2.1", stage / "LICENSES" / "FFmpeg-COPYING.LGPLv2.1")
    shutil.copy2(ROOT / "builder" / "README.txt", stage / "README.txt")
    problems = audit(stage, banned_hashes(ROOT / "builder" / "hashes"))
    home = [str(Path.home()).encode()]
    problems += local_path_leaks(stage, home) + local_path_leaks(args.build / "vcsnx.nro", home)
    if problems:
        print("refusing to package:\n  " + "\n  ".join(problems), file=sys.stderr)
        return 3
    archive = args.out / f"VCSNX-Builder-{version}.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for path in sorted(p for p in stage.rglob("*") if p.is_file()):
            zf.write(path, path.relative_to(args.out).as_posix())
    shutil.copy2(args.build / "vcsnx.nro", args.out / "vcsnx.nro")
    print(f"{archive} {archive.stat().st_size // (1 << 20)} MiB and {args.out / 'vcsnx.nro'}: audit clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
