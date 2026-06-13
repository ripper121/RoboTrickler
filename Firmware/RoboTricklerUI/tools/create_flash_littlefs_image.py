#!/usr/bin/env python3
"""Build a LittleFS image from the generated filesystem staging folder."""

from __future__ import annotations

import argparse
import os
import subprocess
from pathlib import Path


SKETCH_DIR = Path(__file__).resolve().parent.parent
DEFAULT_DATA_DIR = SKETCH_DIR / "data"
DEFAULT_OUTPUT = SKETCH_DIR.parent / "build" / "littlefs.bin"
DEFAULT_MKLITTLEFS = (
    Path(os.environ.get("LOCALAPPDATA", ""))
    / "Arduino15"
    / "packages"
    / "esp32"
    / "tools"
    / "mklittlefs"
    / "4.0.2-db0513a"
    / "mklittlefs.exe"
)

LITTLEFS_SIZE = 0xC0000


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-dir", type=Path, default=DEFAULT_DATA_DIR, help=f"Default: {DEFAULT_DATA_DIR}")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help=f"Default: {DEFAULT_OUTPUT}")
    parser.add_argument("--mklittlefs", type=Path, default=DEFAULT_MKLITTLEFS, help=f"Default: {DEFAULT_MKLITTLEFS}")
    parser.add_argument("--size", default=hex(LITTLEFS_SIZE), help=f"LittleFS partition size. Default: {hex(LITTLEFS_SIZE)}")
    return parser.parse_args()


def parse_size(value: str) -> int:
    return int(value, 0)


def main() -> None:
    args = parse_args()
    data_dir = args.data_dir.resolve()
    output = args.output.resolve()
    mklittlefs = args.mklittlefs.resolve()
    size = parse_size(args.size)

    if not data_dir.is_dir():
        raise FileNotFoundError(f"Data folder not found: {data_dir}")
    if not mklittlefs.is_file():
        raise FileNotFoundError(f"mklittlefs not found: {mklittlefs}")

    output.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        str(mklittlefs),
        "-c",
        str(data_dir),
        "-b",
        "4096",
        "-p",
        "256",
        "-s",
        str(size),
        str(output),
    ]
    subprocess.run(cmd, check=True)
    print(f"Wrote {output}")
    print(f"Image size: {output.stat().st_size} bytes")


if __name__ == "__main__":
    main()
