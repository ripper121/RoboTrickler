#!/usr/bin/env python3
"""Flash RoboTricklerUI firmware and LittleFS over a serial connection."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

from partition_layout import require_partition


SCRIPT_DIR = Path(__file__).resolve().parent
SKETCH_DIR = SCRIPT_DIR.parent
BUILD_DIR = SKETCH_DIR.parent / "build"
ARDUINO_JSON = SKETCH_DIR / ".vscode" / "arduino.json"
COMPILE_UPLOAD_SCRIPT = SCRIPT_DIR / "compile_upload.py"
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
        "--compile-timeout",
        type=int,
        default=600,
        help="CLI compile timeout in seconds.",
    )
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


def compile_artifacts(build_dir: Path, timeout: int) -> None:
    require_file(COMPILE_UPLOAD_SCRIPT)
    build_dir.mkdir(parents=True, exist_ok=True)
    print("Compiling firmware and LittleFS with arduino-cli...", flush=True)
    run(
        [
            sys.executable,
            str(COMPILE_UPLOAD_SCRIPT),
            "--cli",
            "--error",
            "--compile-only",
            "--build-dir",
            str(build_dir),
            "--compile-timeout",
            str(timeout),
        ]
    )


def main() -> int:
    args = parse_args()
    try:
        esptool = require_file(args.esptool)
        build_dir = args.build_dir.resolve()
        compile_artifacts(build_dir, args.compile_timeout)
        littlefs = require_file(build_dir / "littlefs.bin")
        firmware = require_file(build_dir / "RoboTricklerUI.ino.bin")
        app_partition = require_partition("app0")
        otadata_partition = require_partition("otadata")
        littlefs_partition = require_partition("spiffs")
        if firmware.stat().st_size > app_partition.size:
            raise ValueError(
                f"Firmware is {firmware.stat().st_size} bytes but app0 is only "
                f"{app_partition.size} bytes"
            )

        common = [
            str(esptool),
            "--chip",
            "esp32",
            "--port",
            args.port,
            "--baud",
            str(args.baud),
        ]

        partitions = require_file(build_dir / "RoboTricklerUI.ino.partitions.bin")
        boot_app = require_file(build_dir / "boot_app0.bin")
        if args.full:
            bootloader = require_file(build_dir / "RoboTricklerUI.ino.bootloader.bin")
            run(common + ["erase-flash"])
            files = [
                "0x1000",
                str(bootloader),
                "0x8000",
                str(partitions),
                hex(otadata_partition.offset),
                str(boot_app),
                hex(app_partition.offset),
                str(firmware),
                hex(littlefs_partition.offset),
                str(littlefs),
            ]
        else:
            files = [
                "0x8000",
                str(partitions),
                hex(otadata_partition.offset),
                str(boot_app),
                hex(app_partition.offset),
                str(firmware),
                hex(littlefs_partition.offset),
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
    except (FileNotFoundError, ValueError, json.JSONDecodeError, subprocess.CalledProcessError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
