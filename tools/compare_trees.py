"""Compare two directory trees file by file by SHA-256 (stdlib only).

Usage: python3 -I tools/compare_trees.py <reference_dir> <candidate_dir>
Exit status: 0 when both trees hold exactly the same files with identical bytes,
1 when they differ, 2 when an argument is not a directory.
"""
from __future__ import annotations

import argparse
import hashlib
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class TreeDiff:
    identical: tuple[str, ...]
    different: tuple[str, ...]
    missing: tuple[str, ...]  # in reference, absent from candidate
    extra: tuple[str, ...]  # in candidate, absent from reference

    @property
    def matches(self) -> bool:
        return not (self.different or self.missing or self.extra)


def _digest(path: Path) -> str:
    sha = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            sha.update(chunk)
    return sha.hexdigest()


def _files(root: Path) -> dict[str, Path]:
    return {path.relative_to(root).as_posix(): path for path in sorted(root.rglob("*")) if path.is_file()}


def compare_trees(reference: Path, candidate: Path) -> TreeDiff:
    """Classify every file of both trees as identical, different, missing or extra."""
    ref, cand = _files(reference), _files(candidate)
    identical: list[str] = []
    different: list[str] = []
    for name in sorted(ref.keys() & cand.keys()):
        (identical if _digest(ref[name]) == _digest(cand[name]) else different).append(name)
    return TreeDiff(
        identical=tuple(identical),
        different=tuple(different),
        missing=tuple(sorted(ref.keys() - cand.keys())),
        extra=tuple(sorted(cand.keys() - ref.keys())),
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Compare two directory trees by SHA-256.")
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    args = parser.parse_args(argv)
    for directory in (args.reference, args.candidate):
        if not directory.is_dir():
            print(f"not a directory: {directory}", file=sys.stderr)
            return 2
    result = compare_trees(args.reference, args.candidate)
    print(f"identical={len(result.identical)} different={len(result.different)} "
          f"missing={len(result.missing)} extra={len(result.extra)}")
    for label, names in (("different", result.different), ("missing", result.missing), ("extra", result.extra)):
        for name in names:
            print(f"{label}: {name}")
    return 0 if result.matches else 1


if __name__ == "__main__":
    sys.exit(main())
