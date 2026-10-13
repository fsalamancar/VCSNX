"""The game module's interface fingerprint (spec §2.3): SHA-256 of the vendor headers the translated code
is compiled against. CMake bakes it into vcsnx.nro and module_entry.o; tools/make_builder_kit.py writes the
same value to the kit. A runtime update that keeps these headers keeps the fingerprint, so players' existing
vcs_game.so still loads.
Usage: python3 -I tools/module_abi.py <aarch64-none-elf-g++> <repo> [--list]
"""
import hashlib
import subprocess
import sys
from pathlib import Path


def parse_depfile(text: str) -> list[str]:
    deps: set[str] = set()
    for rule in text.replace("\\\n", " ").splitlines():
        if ":" in rule:
            deps.update(rule.split(":", 1)[1].split())
    return sorted(deps)


def generated_dependencies(gxx: str, repo: Path) -> list[Path]:
    sources = sorted(str(p) for p in (repo / "generated").glob("*.cpp"))
    if not sources:
        raise SystemExit(f"no translated code in {repo / 'generated'}: run tools/restore_generated.sh first")
    flags = ["-std=c++20", "-D__SWITCH__", f"-I{repo}/generated", f"-I{repo}/vendor/vcs-recomp/profiles/vcs/host",
             f"-I{repo}/vendor/vcs-recomp/include"]
    text = subprocess.run([gxx, *flags, "-MM", *sources], capture_output=True, text=True, check=True).stdout
    vendor = f"{repo}/vendor/"
    return [Path(dep) for dep in parse_depfile(text) if dep.startswith(vendor)]


def abi_of(headers: list[Path], repo: Path) -> str:
    digest = hashlib.sha256()
    for header in sorted(headers, key=lambda p: p.relative_to(repo).as_posix()):
        digest.update(f"{header.relative_to(repo).as_posix()}\0{hashlib.sha256(header.read_bytes()).hexdigest()}\n".encode())
    return digest.hexdigest()


def main() -> int:
    if len(sys.argv) not in (3, 4):
        print("usage: module_abi.py <g++> <repo> [--list]", file=sys.stderr)
        return 2
    repo = Path(sys.argv[2]).resolve()
    headers = generated_dependencies(sys.argv[1], repo)
    print(";".join(str(h) for h in headers) if sys.argv[3:] == ["--list"] else abi_of(headers, repo))
    return 0


if __name__ == "__main__":
    sys.exit(main())
