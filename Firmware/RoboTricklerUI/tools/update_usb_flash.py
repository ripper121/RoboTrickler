#!/usr/bin/env python3
"""Update the USB-Flash release folder from compiled firmware artifacts."""

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

from partition_layout import require_partition, selected_partitions_csv


SCRIPT_DIR = Path(__file__).resolve().parent
SKETCH_DIR = SCRIPT_DIR.parent
DEFAULT_BUILD_DIR = SKETCH_DIR.parent / "build"
DEFAULT_USB_FLASH_DIR = SKETCH_DIR.parent / "USB-Flash"

BUILD_ARTIFACTS = (
    "RoboTricklerUI.ino.bin",
    "RoboTricklerUI.ino.bootloader.bin",
    "RoboTricklerUI.ino.partitions.bin",
    "RoboTricklerUI.ino.merged.bin",
    "boot_app0.bin",
    "littlefs.bin",
    "sdkconfig",
)
STATIC_FILES = (
    "esptool.exe",
    "flash.bat",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Copy compiled firmware files into USB-Flash, copy the selected "
            "partition CSV, and validate the merged image."
        )
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=DEFAULT_BUILD_DIR,
        help=f"compiled artifact directory (default: {DEFAULT_BUILD_DIR})",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DEFAULT_USB_FLASH_DIR,
        help=f"USB flash package directory (default: {DEFAULT_USB_FLASH_DIR})",
    )
    return parser.parse_args()


def require_file(path: Path, description: str) -> Path:
    if not path.is_file():
        raise FileNotFoundError(f"{description} not found: {path}")
    return path


def copy_package_files(build_dir: Path, output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    obsolete_zip = output_dir / "USB-Flash.zip"
    if obsolete_zip.exists():
        obsolete_zip.unlink()
        print(f"Removed obsolete {obsolete_zip.name}")

    for name in STATIC_FILES:
        require_file(output_dir / name, f"static package file {name}")

    for name in BUILD_ARTIFACTS:
        source = require_file(build_dir / name, f"build artifact {name}")
        destination = output_dir / name
        shutil.copy2(source, destination)
        print(f"Copied {source.name} ({destination.stat().st_size} bytes)")

    partition_source = selected_partitions_csv()
    shutil.copy2(partition_source, output_dir / "partitions.csv")
    print(f"Copied partition layout: {partition_source}")


def validate_merged_image(output_dir: Path) -> None:
    merged_path = output_dir / "RoboTricklerUI.ino.merged.bin"
    flash_size = merged_path.stat().st_size
    if flash_size != 8 * 1024 * 1024:
        raise ValueError(
            f"Merged image is {flash_size} bytes; expected an 8 MiB flash image"
        )

    checks = (
        (0x1000, "RoboTricklerUI.ino.bootloader.bin"),
        (0x8000, "RoboTricklerUI.ino.partitions.bin"),
        (0xE000, "boot_app0.bin"),
        (0x10000, "RoboTricklerUI.ino.bin"),
        (require_partition("spiffs").offset, "littlefs.bin"),
    )
    with merged_path.open("rb") as merged_file:
        for offset, name in checks:
            expected = (output_dir / name).read_bytes()
            merged_file.seek(offset)
            if merged_file.read(len(expected)) != expected:
                raise ValueError(
                    f"Merged image does not contain {name} at {offset:#x}"
                )
    print("Validated 8 MiB merged image, including LittleFS")


def main() -> int:
    args = parse_args()
    build_dir = args.build_dir.resolve()
    output_dir = args.output_dir.resolve()

    try:
        copy_package_files(build_dir, output_dir)
        validate_merged_image(output_dir)
        print("USB-Flash package updated successfully")
        return 0
    except (FileNotFoundError, OSError, ValueError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
