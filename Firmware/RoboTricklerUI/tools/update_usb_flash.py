#!/usr/bin/env python3
"""Update the USB-Flash release folder from compiled firmware artifacts."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

from partition_layout import require_partition, selected_partitions_csv


SCRIPT_DIR = Path(__file__).resolve().parent
SKETCH_DIR = SCRIPT_DIR.parent
DEFAULT_BUILD_DIR = SKETCH_DIR.parent / "build"
DEFAULT_USB_FLASH_DIR = SKETCH_DIR.parent / "USB-Flash"
SD_FILES_GZ_DIR = SKETCH_DIR / "SD-Files-Gz"
COMPILE_UPLOAD = SCRIPT_DIR / "compile_upload.py"
GZIP_SD_FILES = SCRIPT_DIR / "gzip_sd_files.py"
CREATE_FLASH_DATA = SCRIPT_DIR / "create_flash_data.py"
CREATE_FLASH_LITTLEFS_IMAGE = SCRIPT_DIR / "create_flash_littlefs_image.py"

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
            "Regenerate LittleFS, copy compiled firmware files into USB-Flash, "
            "copy the selected partition CSV, and validate the merged image."
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


def regenerate_littlefs(build_dir: Path) -> None:
    if SD_FILES_GZ_DIR.exists():
        if not SD_FILES_GZ_DIR.is_dir():
            raise RuntimeError(
                f"Refusing to replace non-directory output: {SD_FILES_GZ_DIR}"
            )
        shutil.rmtree(SD_FILES_GZ_DIR)
        print(f"Removed {SD_FILES_GZ_DIR}", flush=True)

    steps = (
        (GZIP_SD_FILES, []),
        (CREATE_FLASH_DATA, []),
        (
            CREATE_FLASH_LITTLEFS_IMAGE,
            ["--output", str(build_dir / "littlefs.bin")],
        ),
    )
    for script, arguments in steps:
        require_file(script, f"generation script {script.name}")
        print(f"Running {script.name}...", flush=True)
        subprocess.run(
            [sys.executable, str(script), *arguments],
            cwd=SKETCH_DIR,
            check=True,
        )


def compile_firmware(build_dir: Path) -> None:
    require_file(COMPILE_UPLOAD, "compile script")
    print("Running compile_upload.py --cli...", flush=True)
    subprocess.run(
        [
            sys.executable,
            str(COMPILE_UPLOAD),
            "--cli",
            "--error",
            "--compile-only",
            "--build-dir",
            str(build_dir),
        ],
        cwd=SKETCH_DIR,
        check=True,
    )


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
        regenerate_littlefs(build_dir)
        compile_firmware(build_dir)
        copy_package_files(build_dir, output_dir)
        validate_merged_image(output_dir)
        print("USB-Flash package updated successfully")
        return 0
    except (
        FileNotFoundError,
        OSError,
        RuntimeError,
        subprocess.CalledProcessError,
        ValueError,
    ) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
