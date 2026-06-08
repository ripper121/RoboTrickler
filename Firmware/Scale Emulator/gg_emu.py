from __future__ import annotations

import random
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
            print(f"Serial write failed: {exc}")
            self.stop_event.set()
            return
        print(f"TX {reason}: {packet!r}")

    def handle_command(self, command: str, prefixed: bool) -> None:
        requests_per_second = self.request_rate.record()
        action = COMMANDS.get(command)
        prefix = "ESC " if prefixed else ""
        if not action:
            print(f"RX unknown command: {prefix}{command!r} ({requests_per_second:.1f} req/s)")
            return

        print(f"RX command: {prefix}{command} ({action}, {requests_per_second:.1f} req/s)")
        if action == "print":
            self.send_weight("print command")
        elif action == "tare":
            self.state.tare()
            print("Scale display tared to 0.000")
        elif action == "unit":
            unit = self.state.next_unit()
            print(f"Unit changed to {unit}")
        elif action == "counting":
            enabled = self.state.toggle_counting()
            print(f"Counting mode {'enabled' if enabled else 'disabled'}")
        elif action == "backlight":
            enabled = self.state.toggle_backlight()
            print(f"Backlight {'enabled' if enabled else 'disabled'}")
        elif action == "calibration":
            print("Calibration command acknowledged; no calibration is performed by the emulator.")

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
            print(f"RX non-ASCII byte: 0x{value:02X}")
            return
        self.handle_command(command, prefixed=False)

    def _read_loop(self) -> None:
        while not self.stop_event.is_set():
            try:
                data = self.connection.read(64)
            except serial.SerialException as exc:
                print(f"Serial read failed: {exc}")
                self.stop_event.set()
                return

            for value in data:
                self._process_byte(value)


def print_help() -> None:
    print()
    print("Console commands")
    print("----------------")
    print("+ / Up arrow    increase weight by the current step")
    print("- / Down arrow  decrease weight by the current step")
    print("w <value>       set the displayed weight, for example: w 12.345")
    print("step <value>    set keypress increment/decrement size")
    print("u <unit>        set unit: gr, g, kg, lb, oz, ct")
    print("d <0-5>         set decimal places used in the output")
    print("j <value>       set random +/- jitter added to each print")
    print("print           send one weight frame, like pressing PRINT on the scale")
    print("tare            set displayed weight to zero")
    print("status          show current emulator state and request rate")
    print("help            show this help")
    print("quit            close the serial port and exit")
    print()


def print_status(state: ScaleState, connection: serial.Serial, emulator: ScaleEmulator) -> None:
    with state.lock:
        requests_per_second, total_requests = emulator.request_rate.snapshot()
        print(
            "Status: "
            f"port={connection.port} baud={connection.baudrate} "
            f"weight={state.weight:.{state.decimals}f} unit={state.unit} "
            f"decimals={state.decimals} step={state.step} jitter={state.jitter} "
            f"requests={requests_per_second:.1f}/s total_requests={total_requests}"
        )


def adjust_weight_from_key(emulator: ScaleEmulator, direction: int) -> None:
    emulator.state.adjust_weight(direction)
    print_status(emulator.state, emulator.connection, emulator)


def read_console_line(emulator: ScaleEmulator) -> str | None:
    if msvcrt is None or not sys.stdin.isatty():
        try:
            return input("emu> ")
        except (EOFError, KeyboardInterrupt):
            print()
            return None

    buffer: list[str] = []
    print("emu> ", end="", flush=True)
    while not emulator.stop_event.is_set():
        if not msvcrt.kbhit():
            time.sleep(0.05)
            continue

        char = msvcrt.getwch()
        if char == "\x03":
            print()
            return None
        if char in ("\r", "\n"):
            print()
            return "".join(buffer)
        if char == "\x08":
            if buffer:
                buffer.pop()
                print("\b \b", end="", flush=True)
            continue
        if char in ("\x00", "\xe0"):
            key = msvcrt.getwch()
            if key == "H":
                print()
                adjust_weight_from_key(emulator, 1)
                print(f"emu> {''.join(buffer)}", end="", flush=True)
            elif key == "P":
                print()
                adjust_weight_from_key(emulator, -1)
                print(f"emu> {''.join(buffer)}", end="", flush=True)
            continue
        if char == "+" and not buffer:
            print()
            adjust_weight_from_key(emulator, 1)
            print("emu> ", end="", flush=True)
            continue
        if char == "-" and not buffer:
            print()
            adjust_weight_from_key(emulator, -1)
            print("emu> ", end="", flush=True)
            continue
        if char.isprintable():
            buffer.append(char)
            print(char, end="", flush=True)

    return None


def run_console(emulator: ScaleEmulator) -> None:
    print_help()
    while not emulator.stop_event.is_set():
        line = read_console_line(emulator)
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
                print_help()
            elif command in {"w", "weight"}:
                if not argument:
                    print("Usage: w <value>")
                    continue
                emulator.state.set_weight(float(argument))
                print_status(emulator.state, emulator.connection, emulator)
            elif command in {"u", "unit"}:
                if not argument:
                    print("Usage: u <unit>")
                    continue
                emulator.state.set_unit(argument)
                print_status(emulator.state, emulator.connection, emulator)
            elif command in {"d", "decimals"}:
                if not argument:
                    print("Usage: d <0-5>")
                    continue
                emulator.state.set_decimals(int(argument))
                print_status(emulator.state, emulator.connection, emulator)
            elif command in {"step", "increment", "inc"}:
                if not argument:
                    print("Usage: step <value>")
                    continue
                emulator.state.set_step(float(argument))
                print_status(emulator.state, emulator.connection, emulator)
            elif command in {"j", "jitter"}:
                if not argument:
                    print("Usage: j <value>")
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
                print(f"Unknown console command {command!r}. Type help for commands.")
        except ValueError as exc:
            print(exc)


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
