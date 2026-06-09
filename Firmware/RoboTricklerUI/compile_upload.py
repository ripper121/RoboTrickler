#!/usr/bin/env python3
"""Compile RoboTricklerUI and upload the application binary via HTTP OTA."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile
import time
import uuid
from pathlib import Path
from urllib import error, request


SKETCH_DIR = Path(__file__).resolve().parent
SKETCH_FILE = SKETCH_DIR / "RoboTricklerUI.ino"
DEFAULT_BUILD_DIR = SKETCH_DIR.parent / "build"
DEFAULT_BUILD_CACHE = Path(tempfile.gettempdir()) / "rtui-arduino-core-cache"
DEFAULT_UPDATE_URL = "http://robo-trickler.local/update"
DEFAULT_FQBN = (
    "esp32:esp32:esp32:"
    "JTAGAdapter=default,"
    "PSRAM=disabled,"
    "PartitionScheme=min_spiffs,"
    "CPUFreq=240,"
    "FlashMode=dio,"
    "FlashFreq=80,"
    "FlashSize=4M,"
    "UploadSpeed=921600,"
    "LoopCore=1,"
    "EventsCore=0,"
    "DebugLevel=none,"
    "EraseFlash=all,"
    "ZigbeeMode=default"
)

ARDUINO_BUILDER = Path(r"C:\Program Files (x86)\Arduino\arduino-builder.exe")
ARDUINO_HARDWARE = Path(r"C:\Program Files (x86)\Arduino\hardware")
ARDUINO_TOOLS_BUILDER = Path(r"C:\Program Files (x86)\Arduino\tools-builder")
ARDUINO_AVR_TOOLS = Path(r"C:\Program Files (x86)\Arduino\hardware\tools\avr")
ARDUINO_BUILTIN_LIBS = Path(r"C:\Program Files (x86)\Arduino\libraries")
ARDUINO15_PACKAGES = Path.home() / r"AppData\Local\Arduino15\packages"
USER_LIBRARIES = Path.home() / r"Documents\Arduino\libraries"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compile RoboTricklerUI with arduino-builder and upload it to /update."
    )
    parser.add_argument(
        "--url",
        default=DEFAULT_UPDATE_URL,
        help=f"Firmware update URL. Default: {DEFAULT_UPDATE_URL}",
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=DEFAULT_BUILD_DIR,
        help=f"Arduino build directory. Default: {DEFAULT_BUILD_DIR}",
    )
    parser.add_argument(
        "--build-cache",
        type=Path,
        default=DEFAULT_BUILD_CACHE,
        help=f"Arduino core build cache directory. Default: {DEFAULT_BUILD_CACHE}",
    )
    parser.add_argument(
        "--bin",
        type=Path,
        help="Existing application .bin to upload. Implies --skip-compile.",
    )
    parser.add_argument(
        "--compile-only",
        action="store_true",
        help="Compile and print the generated .bin path without uploading.",
    )
    parser.add_argument(
        "--skip-compile",
        action="store_true",
        help="Upload an existing .bin from --bin or the build directory.",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=120,
        help="HTTP upload timeout in seconds. Default: 120",
    )
    parser.add_argument(
        "--compile-timeout",
        type=int,
        default=600,
        help="Compile timeout in seconds. Default: 600",
    )
    parser.add_argument(
        "--jobs",
        type=int,
        help="Concurrent compiler jobs for arduino-builder. Default: builder auto-detect.",
    )
    return parser.parse_args()


def require_path(path: Path, label: str) -> None:
    if not path.exists():
        raise FileNotFoundError(f"{label} not found: {path}")


def run_builder(build_dir: Path, build_cache: Path, compile_timeout: int, jobs: int | None) -> Path:
    require_path(ARDUINO_BUILDER, "arduino-builder")
    require_path(SKETCH_FILE, "sketch")

    build_dir.mkdir(parents=True, exist_ok=True)
    build_cache.mkdir(parents=True, exist_ok=True)
    cmd = [
        str(ARDUINO_BUILDER),
        "-compile",
        "-quiet=true",
        "-logger=machine",
        "-hardware",
        str(ARDUINO_HARDWARE),
        "-hardware",
        str(ARDUINO15_PACKAGES),
        "-tools",
        str(ARDUINO_TOOLS_BUILDER),
        "-tools",
        str(ARDUINO_AVR_TOOLS),
        "-tools",
        str(ARDUINO15_PACKAGES),
        "-built-in-libraries",
        str(ARDUINO_BUILTIN_LIBS),
        "-libraries",
        str(USER_LIBRARIES),
        "-fqbn",
        DEFAULT_FQBN,
        "-ide-version=10819",
        "-build-path",
        str(build_dir),
        "-build-cache",
        str(build_cache),
        "-warnings=none",
        str(SKETCH_FILE),
    ]
    if jobs is not None:
        cmd[5:5] = ["-jobs", str(max(1, jobs))]

    print("Compiling firmware...")
    print(f"Build directory: {build_dir}")
    print(f"Build cache: {build_cache}")
    proc = subprocess.Popen(cmd, cwd=SKETCH_DIR)
    started = time.monotonic()
    while proc.poll() is None:
        if time.monotonic() - started > compile_timeout:
            kill_process_tree(proc.pid)
            raise TimeoutError(f"Compile timed out after {compile_timeout} seconds")
        time.sleep(0.25)

    if proc.returncode != 0:
        raise subprocess.CalledProcessError(proc.returncode, cmd)

    return find_firmware_bin(build_dir)


def kill_process_tree(pid: int) -> None:
    if os.name == "nt":
        subprocess.run(["taskkill", "/PID", str(pid), "/T", "/F"], check=False)
    else:
        subprocess.run(["kill", "-TERM", str(pid)], check=False)


def find_firmware_bin(build_dir: Path) -> Path:
    preferred = build_dir / "RoboTricklerUI.ino.bin"
    if preferred.exists():
        return preferred

    matches = sorted(build_dir.glob("*.ino.bin"), key=lambda path: path.stat().st_mtime, reverse=True)
    if matches:
        return matches[0]

    raise FileNotFoundError(f"No application .bin found in {build_dir}")


def make_multipart_body(field_name: str, file_path: Path) -> tuple[bytes, str]:
    boundary = f"----RoboTricklerUI{uuid.uuid4().hex}"
    header = (
        f"--{boundary}\r\n"
        f'Content-Disposition: form-data; name="{field_name}"; filename="{file_path.name}"\r\n'
        "Content-Type: application/octet-stream\r\n"
        "\r\n"
    ).encode("ascii")
    footer = f"\r\n--{boundary}--\r\n".encode("ascii")

    return header + file_path.read_bytes() + footer, boundary


def upload_firmware(url: str, bin_path: Path, timeout: int) -> None:
    require_path(bin_path, "firmware binary")
    body, boundary = make_multipart_body("update", bin_path)
    headers = {
        "Content-Type": f"multipart/form-data; boundary={boundary}",
        "Content-Length": str(len(body)),
        "Connection": "close",
    }

    print(f"Uploading {bin_path} ({bin_path.stat().st_size} bytes) to {url}...")
    req = request.Request(url, data=body, headers=headers, method="POST")
    try:
        with request.urlopen(req, timeout=timeout) as resp:
            response_body = resp.read().decode("utf-8", errors="replace")
            print(f"Upload response: HTTP {resp.status}")
            if response_body.strip():
                print(response_body.strip())
            if resp.status >= 400:
                raise RuntimeError(f"Upload failed with HTTP {resp.status}")
    except error.HTTPError as exc:
        details = exc.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"Upload failed with HTTP {exc.code}: {details}") from exc
    except error.URLError as exc:
        raise RuntimeError(f"Upload failed: {exc.reason}") from exc


def main() -> int:
    args = parse_args()
    build_dir = args.build_dir.resolve()
    build_cache = args.build_cache.resolve()

    try:
        if args.bin:
            bin_path = args.bin.resolve()
        elif args.skip_compile:
            bin_path = find_firmware_bin(build_dir)
        else:
            bin_path = run_builder(build_dir, build_cache, args.compile_timeout, args.jobs)

        print(f"Firmware binary: {bin_path}")
        if args.compile_only:
            return 0

        upload_firmware(args.url, bin_path, args.timeout)
        print("Done. The device should reboot after accepting the update.")
        return 0
    except (FileNotFoundError, subprocess.CalledProcessError, RuntimeError, TimeoutError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
