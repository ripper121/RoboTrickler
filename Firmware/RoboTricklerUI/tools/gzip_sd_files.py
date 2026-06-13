#!/usr/bin/env python3
"""Create the compressed LittleFS source tree in SD-Files-Gz."""

from __future__ import annotations

import argparse
import gzip
import io
import os
import sys
import tempfile
from pathlib import Path


DEFAULT_SOURCE = Path(__file__).resolve().parent.parent / "SD-Files"
DEFAULT_OUTPUT = Path(__file__).resolve().parent.parent / "SD-Files-Gz"
EXCLUDED_TOP_LEVEL_DIRECTORIES = {"profiles"}
EXCLUDED_FILES = {"avg.txt", "calibrate.txt"}
UNCOMPRESSED_FILES = {"system/logo.bmp"}
MAX_GZIP_COMPRESSION = 9


def is_relative_to(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def should_compress(relative_path: Path) -> bool:
    return (
        len(relative_path.parts) > 1
        and relative_path.parts[0] not in EXCLUDED_TOP_LEVEL_DIRECTORIES
        and relative_path.as_posix() not in UNCOMPRESSED_FILES
        and relative_path.suffix.lower() != ".gz"
    )


def gzip_bytes(data: bytes) -> bytes:
    output = io.BytesIO()
    with gzip.GzipFile(
        filename="",
        mode="wb",
        fileobj=output,
        compresslevel=MAX_GZIP_COMPRESSION,
        mtime=0,
    ) as gzip_file:
        gzip_file.write(data)
    return output.getvalue()


def write_if_changed(output: Path, data: bytes) -> bool:
    if output.exists() and output.read_bytes() == data:
        return False

    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            dir=output.parent,
            prefix=f".{output.name}.",
            suffix=".tmp",
            delete=False,
        ) as temporary_file:
            temporary_file.write(data)
            temporary_path = Path(temporary_file.name)

        os.replace(temporary_path, output)
        return True
    finally:
        if temporary_path is not None and temporary_path.exists():
            temporary_path.unlink()


def create_output_tree(
    source: Path, output: Path, dry_run: bool
) -> tuple[int, int, int]:
    processed = 0
    changed = 0
    removed = 0
    expected_files: set[Path] = set()

    for path in sorted(source.rglob("*")):
        if not path.is_file():
            continue

        relative_path = path.relative_to(source)
        if relative_path.as_posix() in EXCLUDED_FILES:
            continue
        compress = should_compress(relative_path)
        output_relative_path = relative_path
        data = path.read_bytes()

        if compress:
            output_relative_path = relative_path.with_name(relative_path.name + ".gz")
            data = gzip_bytes(data)

        output_path = output / output_relative_path
        expected_files.add(output_path)
        processed += 1

        if dry_run:
            status = "would update"
            if output_path.exists() and output_path.read_bytes() == data:
                status = "unchanged"
            else:
                changed += 1
        else:
            output_path.parent.mkdir(parents=True, exist_ok=True)
            if write_if_changed(output_path, data):
                status = "compressed" if compress else "copied"
                changed += 1
            else:
                status = "unchanged"

        if dry_run and compress:
            status += " compressed"
        elif dry_run:
            status += " copied"

        print(f"{status}: {output_relative_path}")

    if output.exists():
        for output_path in sorted(output.rglob("*"), reverse=True):
            if output_path.is_file() and output_path not in expected_files:
                relative_path = output_path.relative_to(output)
                if dry_run:
                    print(f"would remove: {relative_path}")
                else:
                    output_path.unlink()
                    print(f"removed: {relative_path}")
                removed += 1
            elif not dry_run and output_path.is_dir():
                try:
                    output_path.rmdir()
                except OSError:
                    pass

    return processed, changed, removed


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Create the SD-Files-Gz LittleFS source tree. Files parsed directly "
            "by firmware remain uncompressed; web assets are stored as .gz files."
        )
    )
    parser.add_argument(
        "--source",
        type=Path,
        default=DEFAULT_SOURCE,
        help=f"Source files folder. Default: {DEFAULT_SOURCE}",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help=f"Generated compressed LittleFS tree. Default: {DEFAULT_OUTPUT}",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show which output files would change without writing them.",
    )
    args = parser.parse_args()

    source = args.source.resolve()
    output = args.output.resolve()

    if not source.is_dir():
        print(f"Source folder does not exist: {source}", file=sys.stderr)
        return 1

    if is_relative_to(output, source) or is_relative_to(source, output):
        print(
            "Source and output folders must not contain each other.",
            file=sys.stderr,
        )
        return 1

    if not args.dry_run:
        output.mkdir(parents=True, exist_ok=True)

    processed, changed, removed = create_output_tree(source, output, args.dry_run)
    action = "Would update" if args.dry_run else "Updated"
    remove_action = "would remove" if args.dry_run else "removed"
    print(
        f"{action} {changed} of {processed} files and {remove_action} "
        f"{removed} stale files in {output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
