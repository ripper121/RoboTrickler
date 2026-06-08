from __future__ import annotations

import random
import os
import shutil
import sys
import threading
import time
from collections import deque
from dataclasses import dataclass, field
from typing import Iterable

try:
    import msvcrt
except ImportError:  # pragma: no cover - Windows-only immediate keypress support
    msvcrt = None

try:
    import serial
    from serial.tools import list_ports
except ImportError as exc:  # pragma: no cover - only used on machines without pyserial
    raise SystemExit(
        "pyserial is required. Install it with: python -m pip install pyserial"
    ) from exc


DEFAULT_BAUD = 9600
DEFAULT_DECIMALS = 3
DEFAULT_STEP = 0.02
UNITS = ("gr", "g", "kg", "lb", "oz", "ct")

COMMANDS = {
    "p": "print",
    "q": "calibration",
    "r": "counting",
    "s": "unit",
    "t": "tare",
    "u": "backlight",
}

HELP_LINES = (
    "Console commands",
    "----------------",
    "+ / Up arrow    increase weight by the current step",
    "- / Down arrow  decrease weight by the current step",
    "w <value>       set the displayed weight, for example: w 12.345",
    "step <value>    set keypress increment/decrement size",
    "u <unit>        set unit: gr, g, kg, lb, oz, ct",
    "d <0-5>         set decimal places used in the output",
    "j <value>       set random +/- jitter added to each print",
    "print           send one weight frame, like pressing PRINT on the scale",
    "tare            set displayed weight to zero",
    "status          show current emulator state and request rate",
    "help            show this help",
    "quit            close the serial port and exit",
)


class RequestRateMeter:
    def __init__(self, window_seconds: float = 1.0) -> None:
        self.window_seconds = window_seconds
        self.timestamps: deque[float] = deque()
        self.total = 0
        self.lock = threading.Lock()

    def record(self) -> float:
        now = time.monotonic()
        with self.lock:
            self.total += 1
            self.timestamps.append(now)
            self._prune(now)
            return len(self.timestamps) / self.window_seconds

    def snapshot(self) -> tuple[float, int]:
        now = time.monotonic()
        with self.lock:
            self._prune(now)
            return len(self.timestamps) / self.window_seconds, self.total

    def _prune(self, now: float) -> None:
        cutoff = now - self.window_seconds
        while self.timestamps and self.timestamps[0] < cutoff:
            self.timestamps.popleft()


class FixedConsole:
    def __init__(self, menu_lines: Iterable[str], prompt: str = "emu> ") -> None:
        self.menu_lines = list(menu_lines)
        self.prompt = prompt
        self.input_buffer = ""
        self.log_lines: deque[str] = deque(maxlen=200)
        self.lock = threading.RLock()
        self.active = False

    def __enter__(self) -> FixedConsole:
        os.system("")
        self.active = True
        sys.stdout.write("\x1b[?1049h\x1b[?25l")
        sys.stdout.flush()
        self.render()
        return self

    def __exit__(self, exc_type, exc, traceback) -> None:
        with self.lock:
            self.active = False
            sys.stdout.write("\x1b[?25h\x1b[?1049l")
            sys.stdout.flush()

    def write(self, message: str = "") -> None:
        with self.lock:
            lines = str(message).splitlines() or [""]
            self.log_lines.extend(lines)
            self.render()

    def set_input(self, value: str) -> None:
        with self.lock:
            self.input_buffer = value
            self.render()

    def render(self) -> None:
        if not self.active:
            return

        size = shutil.get_terminal_size((100, 30))
        width = max(size.columns, 20)
        height = max(size.lines, len(self.menu_lines) + 4)
        log_height = max(1, height - len(self.menu_lines) - 2)
        visible_logs = list(self.log_lines)[-log_height:]

        lines: list[str] = []
        lines.extend(self._fit(line, width) for line in self.menu_lines)
        lines.append("-" * width)
        lines.extend(self._fit(line, width) for line in visible_logs)

        body_height = height - 1
        if len(lines) < body_height:
            lines.extend("" for _ in range(body_height - len(lines)))
        else:
            lines = lines[:body_height]

        prompt_line = self._fit(f"{self.prompt}{self.input_buffer}", width)
        sys.stdout.write("\x1b[H\x1b[2J")
        sys.stdout.write("\n".join(line.ljust(width) for line in lines))
        sys.stdout.write("\n")
        sys.stdout.write(prompt_line.ljust(width))
        sys.stdout.flush()

    @staticmethod
    def _fit(line: str, width: int) -> str:
        if len(line) <= width:
            return line
        return line[: max(0, width - 1)]


@dataclass
class ScaleState:
    weight: float = 0.0
    unit: str = "gr"
    decimals: int = DEFAULT_DECIMALS
    step: float = DEFAULT_STEP
    jitter: float = 0.0
    counting_enabled: bool = False
    backlight_enabled: bool = True
    lock: threading.Lock = field(default_factory=threading.Lock)

    def snapshot(self) -> tuple[float, str, int]:
        with self.lock:
            weight = self.weight
            if self.jitter:
                weight += random.uniform(-self.jitter, self.jitter)
            return weight, self.unit, self.decimals

    def set_weight(self, value: float) -> None:
        with self.lock:
            self.weight = value

    def adjust_weight(self, direction: int) -> float:
        with self.lock:
            self.weight += self.step * direction
            return self.weight

    def tare(self) -> None:
        with self.lock:
            self.weight = 0.0

    def set_unit(self, unit: str) -> None:
        unit = unit.strip().lower()
        if unit not in UNITS:
            raise ValueError(f"Unsupported unit {unit!r}; choose one of: {', '.join(UNITS)}")
        with self.lock:
            self.unit = unit

    def next_unit(self) -> str:
        with self.lock:
            index = UNITS.index(self.unit)
            self.unit = UNITS[(index + 1) % len(UNITS)]
            return self.unit

    def set_decimals(self, value: int) -> None:
        if not 0 <= value <= 5:
            raise ValueError("Decimals must be between 0 and 5")
        with self.lock:
            self.decimals = value

    def set_step(self, value: float) -> None:
        if value <= 0:
            raise ValueError("Step must be greater than zero")
        with self.lock:
            self.step = value

    def set_jitter(self, value: float) -> None:
        if value < 0:
            raise ValueError("Jitter must be zero or greater")
        with self.lock:
            self.jitter = value

    def toggle_counting(self) -> bool:
        with self.lock:
            self.counting_enabled = not self.counting_enabled
            return self.counting_enabled

    def toggle_backlight(self) -> bool:
        with self.lock:
            self.backlight_enabled = not self.backlight_enabled
            return self.backlight_enabled


def available_ports() -> list:
    return list(list_ports.comports())


def print_port_menu(ports: Iterable) -> None:
    print()
    print("Available RS232 / COM ports")
    print("--------------------------")
    ports = list(ports)
    if ports:
        for index, port in enumerate(ports, start=1):
            detail = f"{port.description}"
            if port.hwid and port.hwid != "n/a":
                detail = f"{detail} [{port.hwid}]"
            print(f"{index:>2}. {port.device:<8} {detail}")
    else:
        print("No serial ports detected.")
    print()
    print("Type a number, a COM name such as COM4, R to rescan, or Q to quit.")


def choose_port() -> str:
    while True:
        ports = available_ports()
        print_port_menu(ports)
        choice = input("Select port: ").strip()
        if not choice:
            continue
        if choice.lower() == "q":
            raise SystemExit(0)
        if choice.lower() == "r":
            continue
        if choice.isdigit():
            index = int(choice)
            if 1 <= index <= len(ports):
                return ports[index - 1].device
            print(f"Choose a number from 1 to {len(ports)}.")
            continue
        return choice.upper() if choice.lower().startswith("com") else choice


def choose_baud() -> int:
    while True:
        choice = input(f"Baud rate [{DEFAULT_BAUD}]: ").strip()
        if not choice:
            return DEFAULT_BAUD
        try:
            baud = int(choice)
        except ValueError:
            print("Enter a numeric baud rate.")
            continue
        if baud <= 0:
            print("Baud rate must be greater than zero.")
            continue
        return baud


def format_data_field(weight: float, decimals: int) -> str:
    absolute = abs(weight)
    for places in range(decimals, -1, -1):
        text = f"{absolute:.{places}f}"
        if len(text) <= 7:
            return text.rjust(7)

    scientific = f"{absolute:.1E}"
    if len(scientific) <= 7:
        return scientific.rjust(7)

    return "9999999"


def format_weight_packet(weight: float, unit: str, decimals: int = DEFAULT_DECIMALS) -> bytes:
    """Build the G&G print frame: sign, 7-byte value, 3-byte unit, CR, LF."""
    sign = "-" if weight < 0 else " "
    data = format_data_field(weight, decimals)
    unit_field = unit[:3].rjust(3)
    return f"{sign}{data}{unit_field}\r\n".encode("ascii")


def open_serial(port: str, baud: int) -> serial.Serial:
    return serial.Serial(
        port=port,
        baudrate=baud,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=0.1,
        write_timeout=1.0,
    )


class ScaleEmulator:
    def __init__(self, connection: serial.Serial, state: ScaleState) -> None:
        self.connection = connection
        self.state = state
        self.stop_event = threading.Event()
        self.rx_thread = threading.Thread(target=self._read_loop, name="serial-rx", daemon=True)
        self.pending_escape = False
        self.request_rate = RequestRateMeter()
        self.log = print

    def start(self) -> None:
        self.rx_thread.start()

    def stop(self) -> None:
        self.stop_event.set()
        self.rx_thread.join(timeout=1.0)

    def send_weight(self, reason: str) -> None:
        weight, unit, decimals = self.state.snapshot()
        packet = format_weight_packet(weight, unit, decimals)
        try:
            self.connection.write(packet)
            self.connection.flush()
        except serial.SerialException as exc:
            self.log(f"Serial write failed: {exc}")
            self.stop_event.set()
            return
        self.log(f"TX {reason}: {packet!r}")

    def handle_command(self, command: str, prefixed: bool) -> None:
        requests_per_second = self.request_rate.record()
        action = COMMANDS.get(command)
        prefix = "ESC " if prefixed else ""
        if not action:
            self.log(f"RX unknown command: {prefix}{command!r} ({requests_per_second:.1f} req/s)")
            return

        self.log(f"RX command: {prefix}{command} ({action}, {requests_per_second:.1f} req/s)")
        if action == "print":
            self.send_weight("print command")
        elif action == "tare":
            self.state.tare()
            self.log("Scale display tared to 0.000")
        elif action == "unit":
            unit = self.state.next_unit()
            self.log(f"Unit changed to {unit}")
        elif action == "counting":
            enabled = self.state.toggle_counting()
            self.log(f"Counting mode {'enabled' if enabled else 'disabled'}")
        elif action == "backlight":
            enabled = self.state.toggle_backlight()
            self.log(f"Backlight {'enabled' if enabled else 'disabled'}")
        elif action == "calibration":
            self.log("Calibration command acknowledged; no calibration is performed by the emulator.")

    def _process_byte(self, value: int) -> None:
        if value in (0x0A, 0x0D):
            return

        if self.pending_escape:
            self.pending_escape = False
            self.handle_command(chr(value).lower(), prefixed=True)
            return

        if value == 0x1B:
            self.pending_escape = True
            return

        try:
            command = chr(value).lower()
        except ValueError:
            self.log(f"RX non-ASCII byte: 0x{value:02X}")
            return
        self.handle_command(command, prefixed=False)

    def _read_loop(self) -> None:
        while not self.stop_event.is_set():
            try:
                data = self.connection.read(1)
            except serial.SerialException as exc:
                self.log(f"Serial read failed: {exc}")
                self.stop_event.set()
                return

            for value in data:
                self._process_byte(value)


def print_help(log=print) -> None:
    log("")
    for line in HELP_LINES:
        log(line)
    log("")


def print_status(state: ScaleState, connection: serial.Serial, emulator: ScaleEmulator) -> None:
    with state.lock:
        requests_per_second, total_requests = emulator.request_rate.snapshot()
        emulator.log(
            "Status: "
            f"port={connection.port} baud={connection.baudrate} "
            f"weight={state.weight:.{state.decimals}f} unit={state.unit} "
            f"decimals={state.decimals} step={state.step} jitter={state.jitter} "
            f"requests={requests_per_second:.1f}/s total_requests={total_requests}"
        )


def adjust_weight_from_key(emulator: ScaleEmulator, direction: int) -> None:
    emulator.state.adjust_weight(direction)
    print_status(emulator.state, emulator.connection, emulator)


def read_console_line(emulator: ScaleEmulator, ui: FixedConsole | None = None) -> str | None:
    if msvcrt is None or not sys.stdin.isatty():
        try:
            return input("emu> ")
        except (EOFError, KeyboardInterrupt):
            print()
            return None

    buffer: list[str] = []
    if ui is None:
        print("emu> ", end="", flush=True)
    else:
        ui.set_input("")
    while not emulator.stop_event.is_set():
        if not msvcrt.kbhit():
            time.sleep(0.05)
            continue

        char = msvcrt.getwch()
        if char == "\x03":
            return None
        if char in ("\r", "\n"):
            line = "".join(buffer)
            if ui is None:
                print()
            else:
                ui.write(f"emu> {line}")
                ui.set_input("")
            return line
        if char == "\x08":
            if buffer:
                buffer.pop()
                if ui is None:
                    print("\b \b", end="", flush=True)
                else:
                    ui.set_input("".join(buffer))
            continue
        if char in ("\x00", "\xe0"):
            key = msvcrt.getwch()
            if key == "H":
                if ui is None:
                    print()
                adjust_weight_from_key(emulator, 1)
                if ui is None:
                    print(f"emu> {''.join(buffer)}", end="", flush=True)
                else:
                    ui.set_input("".join(buffer))
            elif key == "P":
                if ui is None:
                    print()
                adjust_weight_from_key(emulator, -1)
                if ui is None:
                    print(f"emu> {''.join(buffer)}", end="", flush=True)
                else:
                    ui.set_input("".join(buffer))
            continue
        if char == "+" and not buffer:
            if ui is None:
                print()
            adjust_weight_from_key(emulator, 1)
            if ui is None:
                print("emu> ", end="", flush=True)
            else:
                ui.set_input("")
            continue
        if char == "-" and not buffer:
            if ui is None:
                print()
            adjust_weight_from_key(emulator, -1)
            if ui is None:
                print("emu> ", end="", flush=True)
            else:
                ui.set_input("")
            continue
        if char.isprintable():
            buffer.append(char)
            if ui is None:
                print(char, end="", flush=True)
            else:
                ui.set_input("".join(buffer))

    return None


def run_console(emulator: ScaleEmulator) -> None:
    use_fixed_ui = msvcrt is not None and sys.stdin.isatty() and sys.stdout.isatty()
    if use_fixed_ui:
        with FixedConsole(HELP_LINES) as ui:
            emulator.log = ui.write
            ui.write("Console ready.")
            run_console_loop(emulator, ui)
        emulator.log = print
        return

    print_help()
    run_console_loop(emulator, None)


def run_console_loop(emulator: ScaleEmulator, ui: FixedConsole | None) -> None:
    while not emulator.stop_event.is_set():
        line = read_console_line(emulator, ui)
        if line is None:
            break

        line = line.strip()
        if not line:
            continue

        command, _, argument = line.partition(" ")
        command = command.lower()
        argument = argument.strip()

        try:
            if command in {"q", "quit", "exit"}:
                break
            if command == "+":
                adjust_weight_from_key(emulator, 1)
            elif command == "-":
                adjust_weight_from_key(emulator, -1)
            elif command in {"h", "help", "?"}:
                print_help(emulator.log)
            elif command in {"w", "weight"}:
                if not argument:
                    emulator.log("Usage: w <value>")
                    continue
                emulator.state.set_weight(float(argument))
                print_status(emulator.state, emulator.connection, emulator)
            elif command in {"u", "unit"}:
                if not argument:
                    emulator.log("Usage: u <unit>")
                    continue
                emulator.state.set_unit(argument)
                print_status(emulator.state, emulator.connection, emulator)
            elif command in {"d", "decimals"}:
                if not argument:
                    emulator.log("Usage: d <0-5>")
                    continue
                emulator.state.set_decimals(int(argument))
                print_status(emulator.state, emulator.connection, emulator)
            elif command in {"step", "increment", "inc"}:
                if not argument:
                    emulator.log("Usage: step <value>")
                    continue
                emulator.state.set_step(float(argument))
                print_status(emulator.state, emulator.connection, emulator)
            elif command in {"j", "jitter"}:
                if not argument:
                    emulator.log("Usage: j <value>")
                    continue
                emulator.state.set_jitter(float(argument))
                print_status(emulator.state, emulator.connection, emulator)
            elif command in {"p", "print"}:
                emulator.send_weight("local print")
            elif command in {"t", "tare"}:
                emulator.state.tare()
                print_status(emulator.state, emulator.connection, emulator)
            elif command == "status":
                print_status(emulator.state, emulator.connection, emulator)
            else:
                emulator.log(f"Unknown console command {command!r}. Type help for commands.")
        except ValueError as exc:
            emulator.log(str(exc))


def main() -> int:
    print("G&G Scale Side Emulator")
    print("=======================")
    print("Supports Bluetooth commands (p/q/r/s/t/u) and RS232 commands (ESC p, ESC t, ...).")

    while True:
        port = choose_port()
        baud = choose_baud()
        try:
            with open_serial(port, baud) as connection:
                print(f"Opened {connection.port} at {connection.baudrate} 8N1.")
                state = ScaleState()
                emulator = ScaleEmulator(connection, state)
                emulator.start()
                try:
                    run_console(emulator)
                finally:
                    emulator.stop()
                return 0
        except serial.SerialException as exc:
            print(f"Could not open {port}: {exc}")
            retry = input("Choose another port? [Y/n]: ").strip().lower()
            if retry == "n":
                return 1


if __name__ == "__main__":
    raise SystemExit(main())
