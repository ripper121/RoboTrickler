#!/usr/bin/env python3
"""Flash RoboTricklerUI firmware and LittleFS over a serial connection."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
SKETCH_DIR = SCRIPT_DIR.parent
BUILD_DIR = SKETCH_DIR.parent / "build"
ARDUINO_JSON = SKETCH_DIR / ".vscode" / "arduino.json"
GZIP_FILES_SCRIPT = SCRIPT_DIR / "gzip_sd_files.py"
CREATE_FLASH_DATA_SCRIPT = SCRIPT_DIR / "create_flash_data.py"
CREATE_LITTLEFS_IMAGE_SCRIPT = SCRIPT_DIR / "create_flash_littlefs_image.py"
DEFAULT_ESPTOOL = (
    Path(os.environ.get("LOCALAPPDATA", ""))
    / "Arduino15"
    / "packages"
    / "esp32"
    / "tools"
    / "esptool_py"
    / "5.3.0"
    / "esptool.exe"
)


def configured_port() -> str:
    if ARDUINO_JSON.is_file():
        with ARDUINO_JSON.open("r", encoding="utf-8") as config_file:
            value = json.load(config_file).get("port")
            if value:
                return str(value)
    return "COM10"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default=configured_port(), help="Serial port.")
    parser.add_argument("--baud", type=int, default=115200, help="Upload baud rate.")
    parser.add_argument("--build-dir", type=Path, default=BUILD_DIR, help="Build artifact directory.")
    parser.add_argument("--esptool", type=Path, default=DEFAULT_ESPTOOL, help="Path to esptool.exe.")
    parser.add_argument(
        "--full",
        action="store_true",
        help="Erase flash and install bootloader, partition table, firmware, and LittleFS.",
    )
    return parser.parse_args()


def require_file(path: Path) -> Path:
    path = path.resolve()
    if not path.is_file():
        raise FileNotFoundError(f"Required file not found: {path}")
    return path


def run(command: list[str]) -> None:
    print(" ".join(f'"{item}"' if " " in item else item for item in command), flush=True)
    subprocess.run(command, check=True)


def prepare_littlefs(build_dir: Path) -> Path:
    require_file(GZIP_FILES_SCRIPT)
    require_file(CREATE_FLASH_DATA_SCRIPT)
    require_file(CREATE_LITTLEFS_IMAGE_SCRIPT)

    build_dir.mkdir(parents=True, exist_ok=True)
    littlefs = build_dir / "littlefs.bin"

    print("Regenerating SD-Files-Gz...", flush=True)
    run([sys.executable, str(GZIP_FILES_SCRIPT)])
    print("Staging LittleFS data...", flush=True)
    run([sys.executable, str(CREATE_FLASH_DATA_SCRIPT)])
    print("Building LittleFS image...", flush=True)
    run(
        [
            sys.executable,
            str(CREATE_LITTLEFS_IMAGE_SCRIPT),
            "--output",
            str(littlefs),
        ]
    )
    return require_file(littlefs)


def main() -> int:
    args = parse_args()
    try:
        esptool = require_file(args.esptool)
        build_dir = args.build_dir.resolve()
        littlefs = prepare_littlefs(build_dir)
        firmware = require_file(build_dir / "RoboTricklerUI.ino.bin")

        common = [
            str(esptool),
            "--chip",
            "esp32",
            "--port",
            args.port,
            "--baud",
            str(args.baud),
        ]

        if args.full:
            bootloader = require_file(build_dir / "RoboTricklerUI.ino.bootloader.bin")
            partitions = require_file(build_dir / "RoboTricklerUI.ino.partitions.bin")
            boot_app = require_file(build_dir / "boot_app0.bin")
            run(common + ["erase-flash"])
            files = [
                "0x1000",
                str(bootloader),
                "0x8000",
                str(partitions),
                "0xE000",
                str(boot_app),
                "0x10000",
                str(firmware),
                "0x330000",
                str(littlefs),
            ]
        else:
            files = [
                "0x10000",
                str(firmware),
                "0x330000",
                str(littlefs),
            ]

        run(
            common
            + [
                "--before",
                "default-reset",
                "--after",
                "hard-reset",
                "write-flash",
                "--flash-mode",
                "dio",
                "--flash-freq",
                "80m",
                "--flash-size",
                "4MB",
            ]
            + files
        )
        print("Flash completed successfully.")
        return 0
    except (FileNotFoundError, json.JSONDecodeError, subprocess.CalledProcessError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
