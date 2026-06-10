#!/usr/bin/env python3
"""Create the SD-Files.tar package consumed by the firmware updater."""

from __future__ import annotations

import argparse
import sys
import tarfile
from pathlib import Path


DEFAULT_SOURCE = Path(__file__).resolve().parent / "SD-Files"
DEFAULT_OUTPUT = Path(__file__).resolve().parent / "SD-Files.tar"


def is_relative_to(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def add_tree(tar: tarfile.TarFile, source: Path, output: Path) -> int:
    source = source.resolve()
    output = output.resolve()
    added = 0

    for path in sorted(source.rglob("*")):
        if path.resolve() == output:
            continue
        if path.is_dir():
            continue

        archive_name = path.relative_to(source).as_posix()
        tar.add(path, arcname=archive_name, recursive=False)
        added += 1

    return added


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Create an uncompressed TAR package from the SD-Files folder."
    )
    parser.add_argument(
        "--source",
        type=Path,
        default=DEFAULT_SOURCE,
        help=f"SD files folder. Default: {DEFAULT_SOURCE}",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help=f"Output TAR path. Default: {DEFAULT_OUTPUT}",
    )
    args = parser.parse_args()

    source = args.source.resolve()
    output = args.output.resolve()

    if not source.is_dir():
        print(f"Source folder does not exist: {source}", file=sys.stderr)
        return 1

    output.parent.mkdir(parents=True, exist_ok=True)
    if is_relative_to(output, source):
        print(
            "Refusing to write the TAR inside SD-Files; choose an output outside the source folder.",
            file=sys.stderr,
        )
        return 1

    with tarfile.open(output, "w", format=tarfile.USTAR_FORMAT) as tar:
        file_count = add_tree(tar, source, output)

    print(f"Created {output}")
    print(f"Packed {file_count} files from {source}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
