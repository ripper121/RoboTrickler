#!/usr/bin/env python3
"""Copy SD-Files-LittleFS into the complete LittleFS staging folder."""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path


SKETCH_DIR = Path(__file__).resolve().parent.parent
DEFAULT_SOURCE = SKETCH_DIR / "SD-Files-LittleFS"
DEFAULT_OUTPUT = SKETCH_DIR / "data"


def recreate_output(output: Path) -> None:
    if output.exists():
        if not output.is_dir():
            raise RuntimeError(f"Refusing to replace non-directory output: {output}")
        shutil.rmtree(output)
    output.mkdir(parents=True)


def copy_filesystem(source: Path, output: Path) -> int:
    copied = 0
    for path in source.rglob("*"):
        if not path.is_file():
            continue
        relative_path = path.relative_to(source)
        target = output / relative_path
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        copied += 1

    return copied


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE, help=f"Default: {DEFAULT_SOURCE}")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help=f"Default: {DEFAULT_OUTPUT}")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    source = args.source.resolve()
    output = args.output.resolve()
    if not source.is_dir():
        raise FileNotFoundError(f"Source folder not found: {source}")

    recreate_output(output)
    copied = copy_filesystem(source, output)
    total_bytes = sum(path.stat().st_size for path in output.rglob("*") if path.is_file())
    print(f"Copied {copied} files to {output}")
    print(f"Total flash data size: {total_bytes} bytes")


if __name__ == "__main__":
    main()
