"""Writes the hash lists the VCSNX Builder checks against: the two known ULUS-10160 executables and the
237 files of PSPRecomp's published generated/ (commit f6e7d41). They are only numbers: nothing of the game.
Usage: python3 -I tools/make_builder_hashes.py <PSPRecomp checkout>/profiles/vcs/generated builder/hashes
"""
import hashlib
import sys
from pathlib import Path

KNOWN_EXECUTABLES = {
    "85c5e172fa0207086b72a37649e9a10d47845f0866b0dee179c48d930e43d17f": "EBOOT.BIN",
    "fee2e86c7fe457ab6da463d9fdc16f156a0900adcdaa2fd260206c11907c5d65": "BOOT.BIN",
}


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: make_builder_hashes.py <generated folder> <output folder>", file=sys.stderr)
        return 2
    generated, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    files = sorted(p for p in generated.iterdir() if p.is_file())
    if len(files) != 237:
        print(f"expected 237 files in {generated}, found {len(files)}", file=sys.stderr)
        return 3
    lines = [f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}" for p in files]
    (out / "generated.sha256").write_text("\n".join(lines) + "\n")
    (out / "executable.sha256").write_text("".join(f"{sha}  {name}\n" for sha, name in KNOWN_EXECUTABLES.items()))
    print(f"{len(lines)} generated hashes, {len(KNOWN_EXECUTABLES)} executable hashes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
