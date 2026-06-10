#!/usr/bin/env python3
"""Compile RoboTricklerUI like the VS Code Arduino extension and upload via OTA."""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import time
import uuid
from pathlib import Path
from urllib import error, request


SKETCH_DIR = Path(__file__).resolve().parent
SKETCH_FILE = SKETCH_DIR / "RoboTricklerUI.ino"
ARDUINO_JSON = SKETCH_DIR / ".vscode" / "arduino.json"

DEFAULT_BUILD_DIR = SKETCH_DIR.parent / "build"
DEFAULT_UPDATE_URL = "http://robo-trickler.local/update"
DEFAULT_BOARD = "esp32:esp32:esp32"
DEFAULT_CONFIGURATION = (
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
    "DebugLevel=debug,"
    "EraseFlash=all,"
    "ZigbeeMode=default"
)

ARDUINO_DEBUG = Path(r"C:\Program Files (x86)\Arduino\arduino_debug.exe")
ARDUINO_CLI = Path(os.environ.get("LOCALAPPDATA", "")) / "ArduinoCLI" / "arduino-cli.exe"
USER_LIBRARIES = Path.home() / r"Documents\Arduino\libraries"
DEBUG_DEFINE_RE = re.compile(r"^\s*#\s*define\s+DEBUG\s+([01])\b")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compile RoboTricklerUI using the VS Code Arduino extension variant, then upload to /update."
    )
    parser.add_argument(
        "--url",
        default=DEFAULT_UPDATE_URL,
        help=f"Firmware update URL. Default: {DEFAULT_UPDATE_URL}",
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=None,
        help=f"Arduino build directory. Default: .vscode/arduino.json output or {DEFAULT_BUILD_DIR}",
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
        "--cli",
        action="store_true",
        help="Compile with arduino-cli instead of the VS Code extension's legacy Arduino IDE variant.",
    )
    parser.add_argument(
        "--cli-path",
        type=Path,
        default=ARDUINO_CLI,
        help=f"Path to arduino-cli.exe for --cli. Default: {ARDUINO_CLI}",
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
    return parser.parse_args()


def require_path(path: Path, label: str) -> None:
    if not path.exists():
        raise FileNotFoundError(f"{label} not found: {path}")


def load_arduino_config() -> dict[str, str]:
    if not ARDUINO_JSON.exists():
        return {}

    with ARDUINO_JSON.open("r", encoding="utf-8") as config_file:
        data = json.load(config_file)

    if not isinstance(data, dict):
        raise RuntimeError(f"Invalid Arduino config: {ARDUINO_JSON}")

    return {str(key): str(value) for key, value in data.items() if value is not None}


def resolve_build_dir(config: dict[str, str], override: Path | None) -> Path:
    if override is not None:
        return override.resolve()

    output = config.get("output")
    if output:
        output_path = Path(output)
        if not output_path.is_absolute():
            output_path = SKETCH_DIR / output_path
        return output_path.resolve()

    return DEFAULT_BUILD_DIR.resolve()


def build_descriptor(config: dict[str, str]) -> str:
    board = config.get("board", DEFAULT_BOARD)
    configuration = config.get("configuration", DEFAULT_CONFIGURATION)
    configuration = set_debug_level(configuration, read_sketch_debug_enabled())
    return f"{board}:{configuration}" if configuration else board


def read_sketch_debug_enabled() -> bool:
    require_path(SKETCH_FILE, "sketch")
    with SKETCH_FILE.open("r", encoding="utf-8") as sketch:
        for line in sketch:
            match = DEBUG_DEFINE_RE.match(line)
            if match:
                return match.group(1) != "0"

    return True


def set_debug_level(configuration: str, debug_enabled: bool) -> str:
    debug_level = "debug" if debug_enabled else "none"
    parts = [part for part in configuration.split(",") if part]
    changed = False
    for index, part in enumerate(parts):
        if part.startswith("DebugLevel="):
            parts[index] = f"DebugLevel={debug_level}"
            changed = True
            break

    if not changed:
        parts.append(f"DebugLevel={debug_level}")

    return ",".join(parts)


def run_extension_compile(build_dir: Path, board_descriptor: str, compile_timeout: int) -> Path:
    require_path(ARDUINO_DEBUG, "arduino_debug.exe")
    require_path(SKETCH_FILE, "sketch")

    build_dir.mkdir(parents=True, exist_ok=True)
    cmd = [
        str(ARDUINO_DEBUG),
        "--verify",
        "--board",
        board_descriptor,
        "--pref",
        f"build.path={build_dir}",
        "--verbose-build",
        str(SKETCH_FILE),
    ]

    print("Compiling firmware with Arduino Community Edition variant...")
    print(f"Build directory: {build_dir}")
    print(f"Board: {board_descriptor}")
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


def run_cli_compile(cli_path: Path, build_dir: Path, board_descriptor: str, compile_timeout: int) -> Path:
    require_path(cli_path, "arduino-cli.exe")
    require_path(SKETCH_FILE, "sketch")

    build_dir.mkdir(parents=True, exist_ok=True)
    cmd = [
        str(cli_path),
        "compile",
        "--fqbn",
        board_descriptor,
        "--build-path",
        str(build_dir),
        "--warnings",
        "all",
        "--no-color",
        "--verbose",
    ]
    if USER_LIBRARIES.exists():
        cmd.extend(["--libraries", str(USER_LIBRARIES)])
    cmd.append(str(SKETCH_DIR))

    print("Compiling firmware with arduino-cli...")
    print(f"CLI: {cli_path}")
    print(f"Build directory: {build_dir}")
    print(f"Board: {board_descriptor}")
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

    try:
        config = load_arduino_config()
        build_dir = resolve_build_dir(config, args.build_dir)

        if args.bin:
            bin_path = args.bin.resolve()
        elif args.skip_compile:
            bin_path = find_firmware_bin(build_dir)
        else:
            board_descriptor = build_descriptor(config)
            if args.cli:
                bin_path = run_cli_compile(args.cli_path.resolve(), build_dir, board_descriptor, args.compile_timeout)
            else:
                bin_path = run_extension_compile(build_dir, board_descriptor, args.compile_timeout)

        print(f"Firmware binary: {bin_path}")
        if args.compile_only:
            return 0

        upload_firmware(args.url, bin_path, args.timeout)
        print("Done. The device should reboot after accepting the update.")
        return 0
    except (FileNotFoundError, json.JSONDecodeError, subprocess.CalledProcessError, RuntimeError, TimeoutError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
