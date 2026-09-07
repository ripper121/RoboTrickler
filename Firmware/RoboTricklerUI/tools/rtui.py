#!/usr/bin/env python3
"""Unified command-line and interactive interface for RoboTricklerUI tools."""

from __future__ import annotations

import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
SKETCH_DIR = SCRIPT_DIR.parent


@dataclass(frozen=True)
class MenuOption:
    flag: str
    label: str
    kind: str = "value"
    default: str = ""
    choices: tuple[tuple[str, str], ...] = ()


@dataclass(frozen=True)
class ToolCommand:
    script: str
    description: str
    options: tuple[MenuOption, ...]


def value(flag: str, label: str, default: str = "") -> MenuOption:
    return MenuOption(flag, label, default=default)


def toggle(flag: str, label: str) -> MenuOption:
    return MenuOption(flag, label, kind="toggle", default="off")


def choice(
    flag: str,
    label: str,
    choices: tuple[tuple[str, str], ...],
    default: str,
) -> MenuOption:
    return MenuOption(flag, label, kind="choice", default=default, choices=choices)


def flag_choice(
    label: str,
    choices: tuple[tuple[str, str], ...],
    default: str,
) -> MenuOption:
    return MenuOption("", label, kind="flag_choice", default=default, choices=choices)


COMMANDS = {
    "release": ToolCommand(
        "release_update_usb_flash.py",
        "Build and update the USB-Flash release package.",
        (
            value("--build-dir", "Compiled artifact directory", "Firmware/build"),
            value("--output-dir", "USB-Flash package directory", "Firmware/USB-Flash"),
            toggle("--legacy", "Build legacy 4 MB package"),
            toggle("--prod", "Production build with clean build directory"),
            toggle("--pdf", "Generate PDF manual"),
            toggle("--flash", "Flash a connected device after packaging"),
        ),
    ),
    "firmware": ToolCommand(
        "firmware_build_upload.py",
        "Build firmware and optionally upload it over web or serial.",
        (
            value("--url", "Firmware update URL", "firmware default"),
            value("--build-dir", "Arduino build directory", ".vscode/arduino.json"),
            value("--bin", "Existing application binary", "not set"),
            toggle("--compile-only", "Compile without uploading"),
            value("--port", "Serial port", "web upload"),
            value("--baud", "Serial upload baud rate", "921600"),
            value("--esptool", "Path to esptool.exe", "automatic"),
            toggle("--full", "Erase flash and write the bootloader"),
            toggle("--legacy-partition", "Use legacy 4 MB partition layout"),
            flag_choice(
                "Compiler",
                (("--cli", "Arduino CLI"), ("--legacy-ide", "Legacy Arduino IDE")),
                "Arduino CLI",
            ),
            flag_choice(
                "Core debug level",
                (("--error", "Error"), ("--automatic-debug-level", "From sketch DEBUG")),
                "Error",
            ),
            value("--cli-path", "Path to arduino-cli.exe", "automatic"),
            toggle("--skip-compile", "Upload an existing binary without compiling"),
            value("--timeout", "HTTP upload timeout in seconds", "120"),
            value("--reboot-timeout", "Web-server reboot timeout in seconds", "60"),
            value("--compile-timeout", "Compile timeout in seconds", "600"),
        ),
    ),
    "filesystem": ToolCommand(
        "filesystem_generate_sd_trees.py",
        "Generate SD-Files-Gz and SD-Files-LittleFS.",
        (
            value("--source", "Source files directory", "SD-Files"),
            value("--output", "Full generated SD-card directory", "SD-Files-Gz"),
            value("--littlefs-output", "Minimal LittleFS directory", "SD-Files-LittleFS"),
            toggle("--dry-run", "Show changes without writing"),
        ),
    ),
    "stage": ToolCommand(
        "filesystem_stage_littlefs_data.py",
        "Stage SD-Files-LittleFS in the data directory.",
        (
            value("--source", "LittleFS source directory", "SD-Files-LittleFS"),
            value("--output", "Staging output directory", "data"),
        ),
    ),
    "littlefs": ToolCommand(
        "filesystem_build_littlefs_image.py",
        "Build the LittleFS image from the data directory.",
        (
            value("--data-dir", "LittleFS data directory", "data"),
            value("--output", "LittleFS image output", "Firmware/build/littlefs.bin"),
            value("--mklittlefs", "Path to mklittlefs", "automatic"),
            value("--size", "LittleFS partition size", "partition default"),
        ),
    ),
    "manual": ToolCommand(
        "manual_generate_pdf.py",
        "Generate the PDF manual.",
        (
            value("--manual", "Source Markdown file", "manual.md"),
            value("--output", "Output PDF file", "SD-Files-Gz/Manual.pdf"),
        ),
    ),
    "profiles": ToolCommand(
        "create_profiles.py",
        "Generate or upload trickler profiles.",
        (
            value("--host", "Device hostname or IP", "robo-trickler.local"),
            value("--timeout", "Upload timeout in seconds", "15"),
            toggle("--dry-run", "Generate without uploading"),
            value("--out-dir", "Also write profiles to this directory", "not set"),
            value("--count", "Number of profiles", "1"),
            value("--name", "Single profile name", "powder"),
            value("--prefix", "Multiple-profile name prefix", "profile"),
            value("--start-index", "First multiple-profile index", "1"),
            value("--weight", "Calibration-run weight", "40.0"),
            value("--target-weight", "Stored target weight", "40.0"),
            value("--weight-gap", "Weight gap", "1.0"),
            value("--rpm", "Stepper speed", "200"),
            value("--measurements", "General measurement count", "2"),
            choice(
                "--bulk-stepper",
                "Bulk stepper",
                (("stepper1", "Stepper 1"), ("stepper2", "Stepper 2")),
                "Stepper 1",
            ),
            value("--calc-tolerance", "Calculation tolerance percent", "65.0"),
            choice(
                "--unit",
                "Weight unit",
                (("grain", "Grain"), ("gram", "Gram")),
                "Grain",
            ),
            value("--alarm-threshold", "Alarm threshold", "1.0"),
            value("--tolerance", "Tolerance", "0.0"),
            toggle("--start-at-zero", "Only start trickling at zero"),
            toggle("--trickle-counter", "Enable per-profile counter"),
        ),
    ),
}


def command_line(command: str, arguments: list[str] | tuple[str, ...]) -> list[str]:
    tool = COMMANDS[command]
    script = SCRIPT_DIR / tool.script
    if not script.is_file():
        raise FileNotFoundError(f"Tool script not found: {script}")
    return [sys.executable, str(script), *arguments]


def run_command(command: str, arguments: list[str] | tuple[str, ...]) -> int:
    process_command = command_line(command, arguments)
    print(f"\nRunning: {subprocess.list2cmdline(process_command)}\n", flush=True)
    try:
        return subprocess.run(process_command, cwd=SKETCH_DIR).returncode
    except OSError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


def print_help() -> None:
    print("RoboTricklerUI tools")
    print("\nUsage:")
    print("  python tools/rtui.py                 Open the interactive menu")
    print("  python tools/rtui.py <command> ...   Run a tool directly")
    print("\nCommands:")
    width = max(len(name) for name in COMMANDS)
    for name, tool in COMMANDS.items():
        print(f"  {name:<{width}}  {tool.description}")
    print("\nUse '<command> --help' to see that tool's complete options.")


def read_selection(prompt: str) -> str | None:
    try:
        return input(prompt).strip()
    except (EOFError, KeyboardInterrupt):
        print("\nCancelled.")
        return None


def option_display(option: MenuOption, selected: str | bool | None) -> str:
    if selected is not None:
        for choice_value, choice_label in option.choices:
            if selected == choice_value:
                return choice_label
        return str(selected)
    return f"default: {option.default}"


def option_line(index: int, option: MenuOption, selected: str | bool | None) -> str:
    if option.kind == "toggle":
        checkbox = "[x]" if selected is True else "[ ]"
        return f"  {index:2}. {checkbox} {option.label}"
    current = option_display(option, selected)
    if option.kind in ("choice", "flag_choice"):
        return f"  {index:2}. ({current}) {option.label}"
    return f"  {index:2}. [{current}] {option.label}"


def configure_option(
    option: MenuOption, selected: str | bool | None
) -> str | bool | None:
    if option.kind == "toggle":
        return selected is not True
    if option.kind == "value":
        entered = read_selection(
            f"{option.label} (blank uses script default '{option.default}'): "
        )
        return entered or None if entered is not None else selected

    print(f"\n{option.label}:")
    print(f"   0. Script default ({option.default})")
    for index, (_, label) in enumerate(option.choices, start=1):
        print(f"  {index:2}. {label}")
    entered = read_selection("Select a value: ")
    if entered is None:
        return selected
    try:
        choice_number = int(entered)
    except ValueError:
        print(f"Invalid selection: {entered}", file=sys.stderr)
        return selected
    if choice_number == 0:
        return None
    if not 1 <= choice_number <= len(option.choices):
        print(f"Invalid selection: {entered}", file=sys.stderr)
        return selected
    return option.choices[choice_number - 1][0]


def selected_arguments(
    options: tuple[MenuOption, ...], selections: dict[int, str | bool]
) -> list[str]:
    arguments: list[str] = []
    for index, option in enumerate(options):
        selected = selections.get(index)
        if selected is None or selected is False:
            continue
        if option.kind == "toggle":
            arguments.append(option.flag)
        elif option.kind == "flag_choice":
            arguments.append(str(selected))
        else:
            arguments.extend((option.flag, str(selected)))
    return arguments


def configure_tool(command: str) -> list[str] | None:
    tool = COMMANDS[command]
    selections: dict[int, str | bool] = {}
    while True:
        print(f"\nConfigure: {command} - {tool.description}\n")
        for index, option in enumerate(tool.options, start=1):
            print(option_line(index, option, selections.get(index - 1)))
        print("\n   R. Run with these settings")
        print("   H. Show the tool's full help")
        print("   B. Back to tool selection")
        print("\nSelect multiple checkboxes with spaces or commas, for example: 3, 4, 5")

        entered = read_selection("\nSelect an option: ")
        if entered is None or entered.lower() == "b":
            return None
        if entered.lower() == "h":
            run_command(command, ("--help",))
            continue
        if entered.lower() == "r":
            arguments = selected_arguments(tool.options, selections)
            preview = ["python", f"tools/{tool.script}", *arguments]
            print(f"\nCommand: {subprocess.list2cmdline(preview)}")
            confirmed = read_selection("Run now? [y/N]: ")
            if confirmed is not None and confirmed.lower() in ("y", "yes"):
                return arguments
            continue
        parts = [part for part in re.split(r"[\s,]+", entered) if part]
        try:
            option_numbers = [int(part) for part in parts]
        except ValueError:
            print(f"Invalid selection: {entered}", file=sys.stderr)
            continue
        if not option_numbers or any(
            not 1 <= option_number <= len(tool.options)
            for option_number in option_numbers
        ):
            print(f"Invalid selection: {entered}", file=sys.stderr)
            continue
        for option_number in option_numbers:
            option_index = option_number - 1
            configured = configure_option(
                tool.options[option_index], selections.get(option_index)
            )
            if configured is None or configured is False:
                selections.pop(option_index, None)
            else:
                selections[option_index] = configured


def interactive_menu() -> int:
    while True:
        print("\nRoboTricklerUI Tool Menu\n")
        for index, (name, tool) in enumerate(COMMANDS.items(), start=1):
            print(f"  {index:2}. {name:<10} {tool.description}")
        print("   0. Exit")

        entered = read_selection("\nSelect a tool: ")
        if entered is None or entered in ("", "0"):
            return 0
        try:
            command_number = int(entered)
        except ValueError:
            print(f"Invalid selection: {entered}", file=sys.stderr)
            continue
        if not 1 <= command_number <= len(COMMANDS):
            print(f"Invalid selection: {entered}", file=sys.stderr)
            continue
        command = tuple(COMMANDS)[command_number - 1]
        arguments = configure_tool(command)
        if arguments is not None:
            return run_command(command, arguments)


def main(argv: list[str] | None = None) -> int:
    arguments = list(sys.argv[1:] if argv is None else argv)
    if not arguments:
        return interactive_menu()
    if arguments[0] in ("-h", "--help"):
        print_help()
        return 0

    command = arguments.pop(0).lower()
    if command not in COMMANDS:
        print(f"Unknown command: {command}\n", file=sys.stderr)
        print_help()
        return 2
    try:
        return run_command(command, arguments)
    except FileNotFoundError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
