"""Downloads devkitPro's official Windows toolchain for the VCSNX Builder, pinned and verified: what
compiling and linking vcs_game.so needs (GCC, binutils, and newlib for the C headers <cmath> includes),
at the versions VCSNX is built with on the Mac (GCC 16.1.0). Every package is checked against the
SHA-256 of devkitPro's package database before it is unpacked, unmodified.

Usage: python3 -I tools/fetch_windows_toolchain.py <output folder>
The output folder gets the packages' opt/devkitpro tree (devkitA64/).
"""
import hashlib
import shutil
import subprocess
import sys
import tempfile
import urllib.request
from pathlib import Path

WINDOWS = "https://pkg.devkitpro.org/packages/windows/x86_64/"
LIBS = "https://pkg.devkitpro.org/packages/"
PACKAGES = [
    (WINDOWS, "devkita64-gcc-16.1.0-1-x86_64.pkg.tar.zst", "4ccf7ee6f3d85248f3b58708e44aa3e9f777479ff90cd4409a95862752e85400"),
    (WINDOWS, "devkita64-binutils-2.46.0-1-x86_64.pkg.tar.zst", "c516a70a31521b61e18759669c6f5c6718796a6cbb0b3c1c023a2ac418f79988"),
    (LIBS, "devkita64-newlib-4.6.0.20260123-4-any.pkg.tar.zst", "fc97ba1009a94b68f8714c3cc548ef697ca82687b72fe0c05a7ca3df81beb53e"),
]
USER_AGENT = "pacman/6.0"  # devkitPro's server answers package downloads made the way pacman makes them


def verify(path: Path, sha256: str) -> bool:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest() == sha256


def download(url: str, destination: Path) -> None:
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request) as response, destination.open("wb") as out:
        shutil.copyfileobj(response, out)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: fetch_windows_toolchain.py <output folder>", file=sys.stderr)
        return 2
    out = Path(sys.argv[1]).resolve()
    out.mkdir(parents=True, exist_ok=True)
    cache = out.parent / "packages"
    cache.mkdir(exist_ok=True)
    for base, name, sha in PACKAGES:
        package = cache / name
        if not package.exists() or not verify(package, sha):
            download(base + name, package)
        if not verify(package, sha):
            print(f"{name}: SHA-256 does not match devkitPro's database", file=sys.stderr)
            return 3
        with tempfile.TemporaryDirectory() as staging:
            subprocess.run(["tar", "--zstd", "-xf", str(package), "-C", staging], check=True)
            shutil.copytree(Path(staging) / "opt" / "devkitpro", out, dirs_exist_ok=True)
        print(f"{name}: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
