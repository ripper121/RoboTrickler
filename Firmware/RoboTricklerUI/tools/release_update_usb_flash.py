#!/usr/bin/env python3
"""Update the USB-Flash release folder from compiled firmware artifacts."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

from partition_layout import require_partition, selected_partitions_csv

LEGACY_PARTITION_SCHEME = "min_spiffs"
LEGACY_FLASH_BYTES = 4 * 1024 * 1024
DEFAULT_FLASH_BYTES = 8 * 1024 * 1024
PARTITION_SCHEME_ENV = "RTUI_PARTITION_SCHEME"
# Defines forced to 0 for a production package build.
PROD_DEFINES = {"DEBUG": 0, "ENABLE_SCREENSHOT": 0}


def _define_re(name: str) -> re.Pattern[str]:
    return re.compile(rf"^(\s*#\s*define\s+{name}\s+)([01])(\b.*)$", re.MULTILINE)


SCRIPT_DIR = Path(__file__).resolve().parent
SKETCH_DIR = SCRIPT_DIR.parent
DEFAULT_BUILD_DIR = SKETCH_DIR.parent / "build"
DEFAULT_USB_FLASH_DIR = SKETCH_DIR.parent / "USB-Flash"
SD_FILES_GZ_DIR = SKETCH_DIR / "SD-Files-Gz"
SD_FILES_LEGACY_DIR = SKETCH_DIR / "SD-Files-Legacy"
COMPILE_OPTIONS_FILE = SKETCH_DIR / "compile_options.h"
FIRMWARE_BUILD_UPLOAD_SCRIPT = SCRIPT_DIR / "firmware_build_upload.py"
GENERATE_SD_TREES_SCRIPT = SCRIPT_DIR / "filesystem_generate_sd_trees.py"
STAGE_LITTLEFS_DATA_SCRIPT = SCRIPT_DIR / "filesystem_stage_littlefs_data.py"
BUILD_LITTLEFS_IMAGE_SCRIPT = SCRIPT_DIR / "filesystem_build_littlefs_image.py"
MANUAL_GENERATE_PDF_SCRIPT = SCRIPT_DIR / "manual_generate_pdf.py"

BUILD_ARTIFACTS = (
    "RoboTricklerUI.ino.bin",
    "RoboTricklerUI.ino.bootloader.bin",
    "RoboTricklerUI.ino.partitions.bin",
    "RoboTricklerUI.ino.merged.bin",
    "boot_app0.bin",
    "littlefs.bin",
    "sdkconfig",
)
# In legacy (4 MB / min_spiffs) mode the LittleFS image is not built or flashed.
LEGACY_SKIP_ARTIFACTS = ("littlefs.bin",)
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
    parser.add_argument(
        "-legacy",
        "--legacy",
        dest="legacy",
        action="store_true",
        help=(
            "Build the 4 MB / Minimal SPIFFS (min_spiffs) package without a "
            "LittleFS image instead of the default 8 MB layout."
        ),
    )
    parser.add_argument(
        "-prod",
        "--prod",
        dest="prod",
        action="store_true",
        help="Production build: force DEBUG 0 and ENABLE_SCREENSHOT 0 for the build.",
    )
    parser.add_argument(
        "-pdf",
        "--pdf",
        dest="pdf",
        action="store_true",
        help="Render manual.md into SD-Files-Gz/Manual.pdf (uncompressed).",
    )
    parser.add_argument(
        "-flash",
        "--flash",
        dest="flash",
        action="store_true",
        help="After building the package, run flash.bat to flash a connected device.",
    )
    return parser.parse_args()


def require_file(path: Path, description: str) -> Path:
    if not path.is_file():
        raise FileNotFoundError(f"{description} not found: {path}")
    return path


def regenerate_littlefs(build_dir: Path, legacy: bool = False) -> None:
    if SD_FILES_GZ_DIR.exists():
        if not SD_FILES_GZ_DIR.is_dir():
            raise RuntimeError(
                f"Refusing to replace non-directory output: {SD_FILES_GZ_DIR}"
            )
        shutil.rmtree(SD_FILES_GZ_DIR)
        print(f"Removed {SD_FILES_GZ_DIR}", flush=True)

    steps = [
        (GENERATE_SD_TREES_SCRIPT, []),
        (STAGE_LITTLEFS_DATA_SCRIPT, []),
    ]
    # The 4 MB / min_spiffs build has no LittleFS, so skip the image build but
    # still regenerate the gzipped SD-Files-Gz output.
    if not legacy:
        steps.append(
            (
                BUILD_LITTLEFS_IMAGE_SCRIPT,
                ["--output", str(build_dir / "littlefs.bin")],
            )
        )
    for script, arguments in steps:
        require_file(script, f"generation script {script.name}")
        print(f"Running {script.name}...", flush=True)
        subprocess.run(
            [sys.executable, str(script), *arguments],
            cwd=SKETCH_DIR,
            check=True,
        )


def set_compile_defines(values: dict[str, int]) -> str | None:
    """Force the given #define names to their values, returning the original text
    (or None if nothing changed) so it can be restored afterward."""
    original = COMPILE_OPTIONS_FILE.read_text(encoding="utf-8")
    patched = original
    for name, value in values.items():
        pattern = _define_re(name)
        if not pattern.search(patched):
            raise RuntimeError(f"{name} define not found in {COMPILE_OPTIONS_FILE}")
        patched = pattern.sub(rf"\g<1>{value}\g<3>", patched, count=1)
        print(f"Set {name} {value} in {COMPILE_OPTIONS_FILE.name}", flush=True)

    if patched == original:
        return None

    COMPILE_OPTIONS_FILE.write_text(patched, encoding="utf-8")
    return original


def restore_compile_options(original: str | None) -> None:
    if original is None:
        return
    COMPILE_OPTIONS_FILE.write_text(original, encoding="utf-8")
    print(f"Restored {COMPILE_OPTIONS_FILE.name}", flush=True)


def compile_firmware(build_dir: Path, legacy: bool) -> None:
    require_file(FIRMWARE_BUILD_UPLOAD_SCRIPT, "compile script")
    print("Running firmware_build_upload.py --cli...", flush=True)
    command = [
        sys.executable,
        str(FIRMWARE_BUILD_UPLOAD_SCRIPT),
        "--cli",
        "--error",
        "--compile-only",
        "--build-dir",
        str(build_dir),
    ]
    if legacy:
        command.append("--legacy-partition")
    subprocess.run(
        command,
        cwd=SKETCH_DIR,
        check=True,
    )


def copy_package_files(build_dir: Path, output_dir: Path, legacy: bool) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    obsolete_zip = output_dir / "USB-Flash.zip"
    if obsolete_zip.exists():
        obsolete_zip.unlink()
        print(f"Removed obsolete {obsolete_zip.name}")

    for name in STATIC_FILES:
        require_file(output_dir / name, f"static package file {name}")

    artifacts = BUILD_ARTIFACTS
    if legacy:
        artifacts = tuple(name for name in artifacts if name not in LEGACY_SKIP_ARTIFACTS)
        # Drop a stale 8 MB LittleFS image so the legacy package stays consistent.
        for name in LEGACY_SKIP_ARTIFACTS:
            stale = output_dir / name
            if stale.exists():
                stale.unlink()
                print(f"Removed {stale.name} (not used in legacy mode)")

    for name in artifacts:
        source = require_file(build_dir / name, f"build artifact {name}")
        destination = output_dir / name
        shutil.copy2(source, destination)
        print(f"Copied {source.name} ({destination.stat().st_size} bytes)")

    if legacy:
        # Provide the firmware image under the OTA update filename as well.
        update_source = require_file(
            build_dir / "RoboTricklerUI.ino.bin", "build artifact RoboTricklerUI.ino.bin"
        )
        update_dest = output_dir / "update.bin"
        shutil.copy2(update_source, update_dest)
        print(f"Copied {update_source.name} as {update_dest.name} ({update_dest.stat().st_size} bytes)")

    partition_source = selected_partitions_csv()
    shutil.copy2(partition_source, output_dir / "partitions.csv")
    print(f"Copied partition layout: {partition_source}")


def export_prod_images(build_dir: Path) -> None:
    """Copy the compiled firmware (and LittleFS, when built) into SD-Files-Gz for release."""
    SD_FILES_GZ_DIR.mkdir(parents=True, exist_ok=True)

    firmware_source = require_file(
        build_dir / "RoboTricklerUI.ino.bin", "firmware binary"
    )
    firmware_dest = SD_FILES_GZ_DIR / "firmware.bin"
    shutil.copy2(firmware_source, firmware_dest)
    print(f"Exported {firmware_dest.name} ({firmware_dest.stat().st_size} bytes)")

    littlefs_source = build_dir / "littlefs.bin"
    if littlefs_source.is_file():
        littlefs_dest = SD_FILES_GZ_DIR / "littlefs.bin"
        shutil.copy2(littlefs_source, littlefs_dest)
        print(f"Exported {littlefs_dest.name} ({littlefs_dest.stat().st_size} bytes)")


def export_legacy_update(build_dir: Path) -> None:
    """Export the legacy OTA firmware and SD-card files into SD-Files-Gz."""
    SD_FILES_GZ_DIR.mkdir(parents=True, exist_ok=True)
    firmware_source = require_file(
        build_dir / "RoboTricklerUI.ino.bin", "firmware binary"
    )
    update_dest = SD_FILES_GZ_DIR / "update.bin"
    shutil.copy2(firmware_source, update_dest)
    print(f"Exported {update_dest.name} ({update_dest.stat().st_size} bytes)")

    # Bundle the legacy SD-card files (e.g. profile/calibration files) so the
    # release folder has everything the 4 MB build needs.
    if not SD_FILES_LEGACY_DIR.is_dir():
        raise FileNotFoundError(
            f"legacy SD files directory not found: {SD_FILES_LEGACY_DIR}"
        )
    for source in sorted(SD_FILES_LEGACY_DIR.glob("*")):
        if not source.is_file():
            continue
        destination = SD_FILES_GZ_DIR / source.name
        shutil.copy2(source, destination)
        print(f"Exported {source.name} ({destination.stat().st_size} bytes)")


def generate_manual_pdf() -> None:
    """Render manual.md into SD-Files-Gz/Manual.pdf (uncompressed) for release.

    Runs after regenerate_littlefs (which rebuilds SD-Files-Gz) and after the
    LittleFS image is built, so the large PDF ships in the SD-card file tree
    without being gzipped or packed into the 1.5 MB LittleFS image."""
    require_file(MANUAL_GENERATE_PDF_SCRIPT, "manual PDF script")
    output_path = SD_FILES_GZ_DIR / "Manual.pdf"
    print(f"Running {MANUAL_GENERATE_PDF_SCRIPT.name}...", flush=True)
    subprocess.run(
        [
            sys.executable,
            str(MANUAL_GENERATE_PDF_SCRIPT),
            "--output",
            str(output_path),
        ],
        cwd=SKETCH_DIR,
        check=True,
    )


def run_flash_bat(output_dir: Path) -> None:
    flash_bat = require_file(output_dir / "flash.bat", "flash script flash.bat")
    print(f"Running {flash_bat.name}...", flush=True)
    subprocess.run(
        ["cmd.exe", "/c", str(flash_bat)],
        cwd=output_dir,
        check=True,
    )


def validate_merged_image(output_dir: Path, legacy: bool) -> None:
    merged_path = output_dir / "RoboTricklerUI.ino.merged.bin"
    flash_size = merged_path.stat().st_size
    expected_size = LEGACY_FLASH_BYTES if legacy else DEFAULT_FLASH_BYTES
    if flash_size != expected_size:
        raise ValueError(
            f"Merged image is {flash_size} bytes; expected a "
            f"{expected_size // (1024 * 1024)} MiB flash image"
        )

    checks = [
        (0x1000, "RoboTricklerUI.ino.bootloader.bin"),
        (0x8000, "RoboTricklerUI.ino.partitions.bin"),
        (0xE000, "boot_app0.bin"),
        (0x10000, "RoboTricklerUI.ino.bin"),
    ]
    if not legacy:
        checks.append((require_partition("spiffs").offset, "littlefs.bin"))

    with merged_path.open("rb") as merged_file:
        for offset, name in checks:
            expected = (output_dir / name).read_bytes()
            merged_file.seek(offset)
            if merged_file.read(len(expected)) != expected:
                raise ValueError(
                    f"Merged image does not contain {name} at {offset:#x}"
                )
    if legacy:
        print("Validated 4 MiB merged image (Minimal SPIFFS, no LittleFS)")
    else:
        print("Validated 8 MiB merged image, including LittleFS")


def main() -> int:
    args = parse_args()
    build_dir = args.build_dir.resolve()
    output_dir = args.output_dir.resolve()

    # partition_layout reads this env var to pick the matching partitions.csv.
    if args.legacy:
        os.environ[PARTITION_SCHEME_ENV] = LEGACY_PARTITION_SCHEME
    else:
        os.environ.pop(PARTITION_SCHEME_ENV, None)

    # Collect every compile_options.h define override into one patch so a single
    # restore in the finally block reverts them all.
    define_overrides: dict[str, int] = {}
    if args.legacy:
        define_overrides["ENABLE_LITTLEFS"] = 0
    if args.prod:
        define_overrides.update(PROD_DEFINES)

    original_compile_options: str | None = None
    try:
        if define_overrides:
            # Restored in the finally block.
            original_compile_options = set_compile_defines(define_overrides)
        if args.legacy:
            print("Legacy mode: 4 MB / Minimal SPIFFS, skipping LittleFS image.", flush=True)
        regenerate_littlefs(build_dir, args.legacy)
        compile_firmware(build_dir, args.legacy)
        copy_package_files(build_dir, output_dir, args.legacy)
        validate_merged_image(output_dir, args.legacy)
        if args.legacy:
            export_legacy_update(build_dir)
        if args.prod:
            export_prod_images(build_dir)
        if args.pdf:
            generate_manual_pdf()
        print("USB-Flash package updated successfully")
        if args.flash:
            run_flash_bat(output_dir)
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
    finally:
        restore_compile_options(original_compile_options)


if __name__ == "__main__":
    raise SystemExit(main())
