#!/usr/bin/env python3
"""Build RoboTricklerUI artifacts and upload them by web or serial."""

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


SCRIPT_DIR = Path(__file__).resolve().parent


def find_sketch_dir() -> Path:
    for candidate in (SCRIPT_DIR, SCRIPT_DIR.parent):
        if (candidate / "RoboTricklerUI.ino").exists():
            return candidate
    return SCRIPT_DIR


SKETCH_DIR = find_sketch_dir()
SKETCH_FILE = SKETCH_DIR / "RoboTricklerUI.ino"
COMPILE_OPTIONS_FILE = SKETCH_DIR / "compile_options.h"
ARDUINO_JSON = SKETCH_DIR / ".vscode" / "arduino.json"
GZIP_FILES = SCRIPT_DIR / "gzip_sd_files.py"
CREATE_FLASH_DATA = SCRIPT_DIR / "create_flash_data.py"
CREATE_LITTLEFS_IMAGE = SCRIPT_DIR / "create_flash_littlefs_image.py"
FLASH_DATA_DIR = SKETCH_DIR / "data"

DEFAULT_BUILD_DIR = SKETCH_DIR.parent / "build"
DEFAULT_UPDATE_URL = "http://robo-trickler.local/update"
DEFAULT_BOARD = "esp32:esp32:esp32"
DEFAULT_FLASH_SIZE = "4MB"
LEGACY_PARTITION_SCHEME = "min_spiffs"
LEGACY_FLASH_SIZE = "4MB"
PARTITION_SCHEME_ENV = "RTUI_PARTITION_SCHEME"
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
DEFAULT_CONFIGURATION = (
    "JTAGAdapter=default,"
    "PSRAM=disabled,"
    "PartitionScheme=custom,"
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
        description=(
            "Regenerate SD-Files-Gz, build firmware and LittleFS, then upload "
            "both through the web server or a serial port."
        )
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
        help="Build both .bin files without uploading.",
    )
    parser.add_argument(
        "--port",
        help="Flash through this serial port instead of the web server, for example COM36.",
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=921600,
        help="Serial upload baud rate. Default: 921600",
    )
    parser.add_argument(
        "--esptool",
        type=Path,
        default=DEFAULT_ESPTOOL,
        help=f"Path to esptool.exe. Default: {DEFAULT_ESPTOOL}",
    )
    parser.add_argument(
        "--full",
        action="store_true",
        help="With --port, erase flash and also write the bootloader.",
    )
    parser.add_argument(
        "-legacyPartition",
        "--legacy-partition",
        dest="legacy_partition",
        action="store_true",
        help=(
            "Use the 4 MB min_spiffs partition configuration from commit "
            "1b312860d6c47f76a9c749f1be4ec189b48fe0b1."
        ),
    )
    parser.add_argument(
        "--cli",
        dest="cli",
        action="store_true",
        default=True,
        help="Compile with arduino-cli (default).",
    )
    parser.add_argument(
        "--legacy-ide",
        dest="cli",
        action="store_false",
        help="Compile with the legacy Arduino IDE instead of arduino-cli.",
    )
    parser.add_argument(
        "--error",
        dest="error",
        action="store_true",
        default=True,
        help="Use ESP32 Core Debug Level Error (default).",
    )
    parser.add_argument(
        "--automatic-debug-level",
        dest="error",
        action="store_false",
        help="Derive the core debug level from the sketch DEBUG define.",
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
        "--reboot-timeout",
        type=int,
        default=60,
        help="Seconds to wait for the web server after uploading LittleFS. Default: 60",
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


def build_descriptor(
    config: dict[str, str],
    force_error_debug_level: bool = False,
    legacy_partition: bool = False,
) -> str:
    board = config.get("board", DEFAULT_BOARD)
    configuration = config.get("configuration", DEFAULT_CONFIGURATION)
    if legacy_partition:
        configuration = set_configuration_option(
            configuration, "PartitionScheme", LEGACY_PARTITION_SCHEME
        )
        configuration = set_configuration_option(
            configuration, "FlashSize", "4M"
        )
    if force_error_debug_level:
        configuration = set_debug_level(configuration, "error")
    else:
        configuration = set_debug_level(configuration, "debug" if read_sketch_debug_enabled() else "none")
    return f"{board}:{configuration}" if configuration else board


def read_sketch_debug_enabled() -> bool:
    require_path(COMPILE_OPTIONS_FILE, "compile options")
    with COMPILE_OPTIONS_FILE.open("r", encoding="utf-8") as source:
        for line in source:
            match = DEBUG_DEFINE_RE.match(line)
            if match:
                return match.group(1) != "0"

    return True


def set_debug_level(configuration: str, debug_level: str) -> str:
    return set_configuration_option(configuration, "DebugLevel", debug_level)


def set_configuration_option(configuration: str, name: str, value: str) -> str:
    parts = [part for part in configuration.split(",") if part]
    changed = False
    for index, part in enumerate(parts):
        if part.startswith(f"{name}="):
            parts[index] = f"{name}={value}"
            changed = True
            break

    if not changed:
        parts.append(f"{name}={value}")

    return ",".join(parts)


def clear_generated_partition_artifacts(build_dir: Path) -> None:
    for path in (
        build_dir / "partitions.csv",
        build_dir / "RoboTricklerUI.ino.partitions.bin",
    ):
        if path.exists():
            path.unlink()


def run_extension_compile(build_dir: Path, board_descriptor: str, compile_timeout: int) -> Path:
    require_path(ARDUINO_DEBUG, "arduino_debug.exe")
    require_path(SKETCH_FILE, "sketch")

    build_dir.mkdir(parents=True, exist_ok=True)
    clear_generated_partition_artifacts(build_dir)
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
    clear_generated_partition_artifacts(build_dir)
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


def build_littlefs_image(build_dir: Path, timeout: int) -> Path:
    require_path(GZIP_FILES, "GZ file generation script")
    require_path(CREATE_FLASH_DATA, "LittleFS staging script")
    require_path(CREATE_LITTLEFS_IMAGE, "LittleFS image script")

    image_path = build_dir / "littlefs.bin"
    subprocess.run(
        [sys.executable, str(GZIP_FILES)],
        cwd=SKETCH_DIR,
        check=True,
        timeout=timeout,
    )
    subprocess.run(
        [sys.executable, str(CREATE_FLASH_DATA)],
        cwd=SKETCH_DIR,
        check=True,
        timeout=timeout,
    )
    subprocess.run(
        [
            sys.executable,
            str(CREATE_LITTLEFS_IMAGE),
            "--output",
            str(image_path),
        ],
        cwd=SKETCH_DIR,
        check=True,
        timeout=timeout,
    )
    return image_path


def build_merged_image(
    esptool_path: Path,
    build_dir: Path,
    firmware_path: Path,
    littlefs_path: Path | None,
    flash_size: str,
    timeout: int,
) -> Path:
    from partition_layout import require_partition

    require_path(esptool_path, "esptool")
    bootloader_path = build_dir / "RoboTricklerUI.ino.bootloader.bin"
    partitions_path = build_dir / "RoboTricklerUI.ino.partitions.bin"
    boot_app0_path = build_dir / "boot_app0.bin"
    for path, description in (
        (bootloader_path, "bootloader"),
        (partitions_path, "partition table"),
        (boot_app0_path, "OTA boot data"),
        (firmware_path, "firmware"),
    ):
        require_path(path, description)
    if littlefs_path is not None:
        require_path(littlefs_path, "LittleFS image")

    output_path = build_dir / "RoboTricklerUI.ino.merged.bin"
    if output_path.exists():
        output_path.unlink()
    files = [
        "0x1000",
        str(bootloader_path),
        "0x8000",
        str(partitions_path),
        "0xe000",
        str(boot_app0_path),
        "0x10000",
        str(firmware_path),
    ]
    if littlefs_path is not None:
        littlefs_partition = require_partition("spiffs")
        files.extend([hex(littlefs_partition.offset), str(littlefs_path)])

    subprocess.run(
        [
            str(esptool_path),
            "--chip",
            "esp32",
            "merge-bin",
            "--flash-mode",
            "dio",
            "--flash-freq",
            "80m",
            "--flash-size",
            flash_size,
            "--pad-to-size",
            flash_size,
            "--output",
            str(output_path),
        ]
        + files,
        cwd=SKETCH_DIR,
        check=True,
        timeout=timeout,
    )
    return output_path


def format_size(size: int) -> str:
    if size >= 1024 * 1024:
        return f"{size / (1024 * 1024):.2f} MiB"
    return f"{size / 1024:.1f} KiB"


def print_space_summary(firmware_path: Path, include_littlefs: bool = True) -> None:
    from partition_layout import require_partition

    app_partition = require_partition("app0")
    firmware_used = firmware_path.stat().st_size
    firmware_free = max(0, app_partition.size - firmware_used)

    print("\nSpace summary:")
    print(
        f"  Firmware: {format_size(firmware_used)} used of "
        f"{format_size(app_partition.size)}, {format_size(firmware_free)} free "
        f"({firmware_free * 100 / app_partition.size:.1f}%)"
    )
    if include_littlefs:
        littlefs_partition = require_partition("spiffs")
        littlefs_used = sum(
            path.stat().st_size for path in FLASH_DATA_DIR.rglob("*") if path.is_file()
        )
        littlefs_free = max(0, littlefs_partition.size - littlefs_used)
        print(
            f"  LittleFS: {format_size(littlefs_used)} files of "
            f"{format_size(littlefs_partition.size)}, about "
            f"{format_size(littlefs_free)} free "
            f"({littlefs_free * 100 / littlefs_partition.size:.1f}%, before filesystem metadata)"
        )


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


def upload_web_artifact(url: str, field_name: str, bin_path: Path, timeout: int) -> None:
    require_path(bin_path, f"{field_name} binary")
    body, boundary = make_multipart_body(field_name, bin_path)
    headers = {
        "Content-Type": f"multipart/form-data; boundary={boundary}",
        "Content-Length": str(len(body)),
        "Connection": "close",
    }

    print(
        f"Uploading {field_name}: {bin_path} "
        f"({bin_path.stat().st_size} bytes) to {url}..."
    )
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


def wait_for_web_server(url: str, timeout: int) -> None:
    probe_url = url.rsplit("/", 1)[0] + "/fwupdate"
    deadline = time.monotonic() + timeout
    print(f"Waiting for {probe_url} after reboot...")
    while time.monotonic() < deadline:
        try:
            with request.urlopen(probe_url, timeout=3) as response:
                if response.status < 500:
                    print("Web server is ready.")
                    return
        except (error.HTTPError, error.URLError, TimeoutError):
            pass
        time.sleep(1)
    raise TimeoutError(f"Web server did not return within {timeout} seconds")


def run_command(command: list[str]) -> None:
    print(" ".join(f'"{item}"' if " " in item else item for item in command), flush=True)
    subprocess.run(command, check=True)


def flash_serial(
    port: str,
    baud: int,
    esptool_path: Path,
    build_dir: Path,
    firmware_path: Path,
    littlefs_path: Path | None,
    full: bool,
    flash_size: str,
) -> None:
    from partition_layout import require_partition

    require_path(esptool_path, "esptool.exe")
    require_path(firmware_path, "firmware binary")
    if littlefs_path is not None:
        require_path(littlefs_path, "LittleFS image")

    app_partition = require_partition("app0")
    otadata_partition = require_partition("otadata")
    if firmware_path.stat().st_size > app_partition.size:
        raise RuntimeError(
            f"Firmware is {firmware_path.stat().st_size} bytes but app0 is only "
            f"{app_partition.size} bytes"
        )
    if littlefs_path is not None:
        littlefs_partition = require_partition("spiffs")
        if littlefs_path.stat().st_size > littlefs_partition.size:
            raise RuntimeError(
                f"LittleFS image is {littlefs_path.stat().st_size} bytes but the partition "
                f"is only {littlefs_partition.size} bytes"
            )

    partitions = build_dir / "RoboTricklerUI.ino.partitions.bin"
    boot_app = build_dir / "boot_app0.bin"
    require_path(partitions, "partition table")
    require_path(boot_app, "OTA boot data")

    common = [
        str(esptool_path),
        "--chip",
        "esp32",
        "--port",
        port,
        "--baud",
        str(baud),
    ]
    files = [
        "0x8000",
        str(partitions),
        hex(otadata_partition.offset),
        str(boot_app),
        hex(app_partition.offset),
        str(firmware_path),
    ]
    if littlefs_path is not None:
        files.extend([hex(littlefs_partition.offset), str(littlefs_path)])

    if full:
        bootloader = build_dir / "RoboTricklerUI.ino.bootloader.bin"
        require_path(bootloader, "bootloader")
        run_command(common + ["erase-flash"])
        files = ["0x1000", str(bootloader)] + files

    run_command(
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
            flash_size,
        ]
        + files
    )
    print("Serial flash completed successfully.")


def main() -> int:
    args = parse_args()

    try:
        partition_scheme = (
            LEGACY_PARTITION_SCHEME if args.legacy_partition else None
        )
        if partition_scheme:
            os.environ[PARTITION_SCHEME_ENV] = partition_scheme
        else:
            os.environ.pop(PARTITION_SCHEME_ENV, None)
        flash_size = (
            LEGACY_FLASH_SIZE if args.legacy_partition else DEFAULT_FLASH_SIZE
        )

        config = load_arduino_config()
        build_dir = resolve_build_dir(config, args.build_dir)

        if args.bin:
            bin_path = args.bin.resolve()
        elif args.skip_compile:
            bin_path = find_firmware_bin(build_dir)
        else:
            board_descriptor = build_descriptor(
                config, args.error, args.legacy_partition
            )
            if args.cli:
                bin_path = run_cli_compile(args.cli_path.resolve(), build_dir, board_descriptor, args.compile_timeout)
            else:
                bin_path = run_extension_compile(build_dir, board_descriptor, args.compile_timeout)

        littlefs_path = None
        if not args.legacy_partition:
            littlefs_path = build_littlefs_image(build_dir, args.compile_timeout)
        else:
            print("Legacy partition mode: skipping LittleFS image (128 KiB partition).")
        merged_path = build_merged_image(
            args.esptool.resolve(),
            build_dir,
            bin_path,
            littlefs_path,
            flash_size,
            args.compile_timeout,
        )
        print(f"Firmware binary: {bin_path}")
        if littlefs_path is not None:
            from partition_layout import require_partition

            littlefs_partition = require_partition("spiffs")
            print(f"LittleFS image: {littlefs_path} (flash offset {hex(littlefs_partition.offset)})")
        print(f"Merged flash image: {merged_path}")
        if args.compile_only:
            print_space_summary(bin_path, littlefs_path is not None)
            return 0

        if args.port:
            flash_serial(
                args.port,
                args.baud,
                args.esptool.resolve(),
                build_dir,
                bin_path,
                littlefs_path,
                args.full,
                flash_size,
            )
        else:
            if args.full:
                raise RuntimeError("--full requires --port")
            if littlefs_path is not None:
                upload_web_artifact(args.url, "filesystem", littlefs_path, args.timeout)
                wait_for_web_server(args.url, args.reboot_timeout)
            upload_web_artifact(args.url, "firmware", bin_path, args.timeout)
            print(
                "Web upload completed. The partition table is unchanged; use --port "
                "when the selected partition scheme has changed."
            )
        print_space_summary(bin_path, littlefs_path is not None)
        return 0
    except (
        FileNotFoundError,
        json.JSONDecodeError,
        subprocess.CalledProcessError,
        subprocess.TimeoutExpired,
        RuntimeError,
        TimeoutError,
    ) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
