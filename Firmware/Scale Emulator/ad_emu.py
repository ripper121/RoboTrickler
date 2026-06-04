from __future__ import annotations

import random
import re
import sys
import threading
import time
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
DEFAULT_SERIAL_FORMAT = "8N1"
DEFAULT_DECIMALS = 3
DEFAULT_STEP = 0.001
DEFAULT_STREAM_HZ = 5.0
TERMINATOR = b"\r\n"
ACK_PACKET = b"\x06\r\n"
ESC = b"\x1b"

UNITS = ("g", "gn", "gr", "kg", "mg", "oz", "lb", "ct", "pcs", "%")

SERIAL_FORMATS = {
    "8N1": (serial.EIGHTBITS, serial.PARITY_NONE, serial.STOPBITS_ONE),
    "7E1": (serial.SEVENBITS, serial.PARITY_EVEN, serial.STOPBITS_ONE),
    "7O1": (serial.SEVENBITS, serial.PARITY_ODD, serial.STOPBITS_ONE),
}

IMMEDIATE_WEIGHT_COMMANDS = {b"Q", b"SI"}
STABLE_WEIGHT_COMMANDS = {b"S", ESC + b"P"}
REZERO_COMMANDS = {b"R", b"Z", ESC + b"T"}
TARE_COMMANDS = {b"T"}
NO_OP_CONTROL_COMMANDS = {b"CAL", b"EXC", b"SMP"}


@dataclass
class ScaleState:
    weight: float = 0.0
    tare_value: float = 0.0
    unit: str = "gr"
    decimals: int = DEFAULT_DECIMALS
    step: float = DEFAULT_STEP
    jitter: float = 0.0
    stable: bool = True
    counting_enabled: bool = False
    display_on: bool = True
    overload: int = 0
    ack_enabled: bool = False
    unit_mass_value: float = 0.0
    key_lock: int = 0
    lock: threading.Lock = field(default_factory=threading.Lock)

    def snapshot(self) -> tuple[float, str, int, bool, bool, int, bool]:
        with self.lock:
            weight = self.weight
            if self.jitter and self.overload == 0:
                weight += random.uniform(-self.jitter, self.jitter)
            return (
                weight,
                self.unit,
                self.decimals,
                self.stable,
                self.counting_enabled,
                self.overload,
                self.display_on,
            )

    def set_weight(self, value: float) -> None:
        with self.lock:
            self.weight = value
            self.overload = 0

    def adjust_weight(self, direction: int) -> float:
        with self.lock:
            self.weight += self.step * direction
            self.overload = 0
            return self.weight

    def tare(self) -> None:
        with self.lock:
            self.tare_value += self.weight
            self.weight = 0.0
            self.overload = 0

    def set_tare_value(self, value: float) -> None:
        if value < 0:
            raise ValueError("Preset tare value cannot be negative")
        with self.lock:
            self.tare_value = value

    def set_unit_mass_value(self, value: float) -> None:
        if value < 0:
            raise ValueError("Unit mass value cannot be negative")
        with self.lock:
            self.unit_mass_value = value

    def set_key_lock(self, value: int) -> None:
        if value not in (0, 1):
            raise ValueError("Key lock value must be 000 or 001")
        with self.lock:
            self.key_lock = value

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

    def set_stable(self, stable: bool) -> None:
        with self.lock:
            self.stable = stable

    def set_overload(self, direction: int) -> None:
        if direction not in (-1, 0, 1):
            raise ValueError("Overload direction must be -1, 0, or 1")
        with self.lock:
            self.overload = direction

    def set_ack_enabled(self, enabled: bool) -> None:
        with self.lock:
            self.ack_enabled = enabled

    def set_display_on(self, enabled: bool) -> None:
        with self.lock:
            self.display_on = enabled

    def toggle_display(self) -> bool:
        with self.lock:
            self.display_on = not self.display_on
            return self.display_on

    def toggle_counting(self) -> bool:
        with self.lock:
            self.counting_enabled = not self.counting_enabled
            return self.counting_enabled


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


def choose_serial_format() -> str:
    formats = "/".join(SERIAL_FORMATS)
    while True:
        choice = input(f"Serial format {formats} [{DEFAULT_SERIAL_FORMAT}]: ").strip().upper()
        if not choice:
            return DEFAULT_SERIAL_FORMAT
        if choice in SERIAL_FORMATS:
            return choice
        print(f"Choose one of: {', '.join(SERIAL_FORMATS)}.")


def format_data_field(weight: float, decimals: int) -> str | None:
    sign = "-" if weight < 0 else "+"
    numeric = f"{abs(weight):0{8}.{decimals}f}"
    if len(numeric) > 8:
        return None
    return f"{sign}{numeric}"


def format_ad_standard_packet(
    weight: float,
    unit: str,
    decimals: int = DEFAULT_DECIMALS,
    stable: bool = True,
    counting: bool = False,
    overload: int = 0,
) -> bytes:
    """Build the A&D standard 15-character data frame plus CR/LF."""
    if overload:
        sign = "-" if overload < 0 else "+"
        return f"OL,{sign}9999999E+19\r\n".encode("ascii")

    data = format_data_field(weight, decimals)
    if data is None:
        sign = "-" if weight < 0 else "+"
        return f"OL,{sign}9999999E+19\r\n".encode("ascii")

    header = "QT" if counting and stable else "ST" if stable else "US"
    unit_field = unit[:3].rjust(3)
    return f"{header},{data}{unit_field}\r\n".encode("ascii")


def format_labeled_value_packet(
    label: str,
    value: float,
    unit: str,
    decimals: int = DEFAULT_DECIMALS,
) -> bytes:
    data = format_data_field(value, decimals)
    if data is None:
        sign = "-" if value < 0 else "+"
        data = f"{sign}9999999E+19"
    unit_field = unit[:3].rjust(3)
    return f"{label},{data}{unit_field}\r\n".encode("ascii")


def render_command(command: bytes) -> str:
    return command.replace(ESC, b"<ESC>").decode("ascii", errors="replace")


def command_to_bytes(command: bytes | str) -> bytes:
    if isinstance(command, bytes):
        return command
    return command.encode("latin1", errors="replace")


def parse_value_command(command: bytes, prefix: bytes) -> float:
    pattern = rb"^" + re.escape(prefix) + rb":\s*([+]?\d+(?:[\.,]\d+)?)(?:\s*[A-Za-z%]{1,3})?$"
    match = re.fullmatch(pattern, command)
    if not match:
        raise ValueError(f"Invalid {prefix.decode('ascii')} command format")
    return float(match.group(1).decode("ascii").replace(",", "."))


def parse_number_command(command: bytes, prefix: bytes, digits: int) -> int:
    pattern = rb"^" + re.escape(prefix) + rb":(\d{" + str(digits).encode("ascii") + rb"})$"
    match = re.fullmatch(pattern, command)
    if not match:
        raise ValueError(f"Invalid {prefix.decode('ascii')} command format")
    return int(match.group(1).decode("ascii"))


def open_serial(port: str, baud: int, serial_format: str) -> serial.Serial:
    bytesize, parity, stopbits = SERIAL_FORMATS[serial_format]
    return serial.Serial(
        port=port,
        baudrate=baud,
        bytesize=bytesize,
        parity=parity,
        stopbits=stopbits,
        timeout=0.1,
        write_timeout=1.0,
    )


class ScaleEmulator:
    def __init__(self, connection: serial.Serial, state: ScaleState) -> None:
        self.connection = connection
        self.state = state
        self.stop_event = threading.Event()
        self.rx_thread = threading.Thread(target=self._read_loop, name="serial-rx", daemon=True)
        self.stream_thread = threading.Thread(target=self._stream_loop, name="serial-stream", daemon=True)
        self.command_buffer = bytearray()
        self.write_lock = threading.Lock()
        self.stream_lock = threading.Lock()
        self.stream_enabled = False
        self.stream_interval = 1.0 / DEFAULT_STREAM_HZ
        self.pending_stable_requests = 0

    def start(self) -> None:
        self.rx_thread.start()
        self.stream_thread.start()

    def stop(self) -> None:
        self.stop_event.set()
        self.rx_thread.join(timeout=1.0)
        self.stream_thread.join(timeout=1.0)

    def _write_packet(self, packet: bytes, reason: str) -> None:
        try:
            with self.write_lock:
                self.connection.write(packet)
                self.connection.flush()
        except serial.SerialException as exc:
            print(f"Serial write failed: {exc}")
            self.stop_event.set()
            return
        print(f"TX {reason}: {packet!r}")

    def send_weight(self, reason: str) -> None:
        weight, unit, decimals, stable, counting, overload, display_on = self.state.snapshot()
        if not display_on:
            self.send_error("E02", "display is off")
            return
        packet = format_ad_standard_packet(weight, unit, decimals, stable, counting, overload)
        self._write_packet(packet, reason)

    def send_ack(self, reason: str) -> None:
        if self.state.ack_enabled:
            self._write_packet(ACK_PACKET, reason)

    def send_error(self, code: str, reason: str) -> None:
        if self.state.ack_enabled:
            self._write_packet(f"EC,{code}\r\n".encode("ascii"), reason)
        else:
            print(f"Suppressed A&D error {code}: {reason}")

    def send_tare_value(self, reason: str) -> None:
        with self.state.lock:
            tare_value = self.state.tare_value
            unit = self.state.unit
            decimals = self.state.decimals
        packet = format_labeled_value_packet("PT", tare_value, unit, decimals)
        self._write_packet(packet, reason)

    def send_labeled_value(self, label: str, value: float, reason: str) -> None:
        with self.state.lock:
            unit = self.state.unit
            decimals = self.state.decimals
        packet = format_labeled_value_packet(label, value, unit, decimals)
        self._write_packet(packet, reason)

    def send_text(self, text: str, reason: str) -> None:
        self._write_packet(f"{text}\r\n".encode("ascii"), reason)

    def set_stream(self, enabled: bool) -> None:
        with self.stream_lock:
            self.stream_enabled = enabled
        print(f"Stream output {'enabled' if enabled else 'disabled'}.")

    def set_stream_rate(self, hz: float) -> None:
        if hz <= 0:
            raise ValueError("Stream rate must be greater than zero")
        with self.stream_lock:
            self.stream_interval = 1.0 / hz
        print(f"Stream rate set to {hz:g} Hz.")

    def maybe_send_pending_stable(self) -> None:
        with self.state.lock:
            stable = self.state.stable
        if not stable or self.pending_stable_requests <= 0:
            return

        pending = self.pending_stable_requests
        self.pending_stable_requests = 0
        for _ in range(pending):
            self.send_weight("deferred stable request")

    def handle_command(self, raw_command: bytes | str) -> None:
        command = command_to_bytes(raw_command)
        if not command:
            return

        original = render_command(command)
        print(f"RX command: {original!r}")

        try:
            if command in IMMEDIATE_WEIGHT_COMMANDS:
                self.send_weight(f"{original} command")
            elif command in STABLE_WEIGHT_COMMANDS:
                with self.state.lock:
                    stable = self.state.stable
                if stable:
                    self.send_weight(f"{original} command")
                else:
                    self.pending_stable_requests += 1
                    print("Stable request queued until the emulator is marked stable.")
            elif command == b"SIR":
                self.set_stream(True)
            elif command == b"C":
                self.pending_stable_requests = 0
                self.set_stream(False)
                self.send_ack("cancel command")
            elif command in TARE_COMMANDS:
                self.state.tare()
                print("Scale display tared to 0.000")
                self.send_ack("tare command")
            elif command in REZERO_COMMANDS:
                self.state.tare()
                print("Scale display re-zeroed to 0.000")
                self.send_ack("re-zero command")
            elif command == b"PRT":
                self.send_weight("PRINT key command")
            elif command == b"U":
                unit = self.state.next_unit()
                print(f"Unit changed to {unit}")
                self.send_ack("MODE key command")
            elif command == b"P":
                enabled = self.state.toggle_display()
                print(f"Display {'on' if enabled else 'off'}")
                self.send_ack("ON/OFF key command")
            elif command == b"ON":
                self.state.set_display_on(True)
                print("Display on")
                self.send_ack("ON command")
            elif command == b"OFF":
                self.state.set_display_on(False)
                print("Display off")
                self.send_ack("OFF command")
            elif command in NO_OP_CONTROL_COMMANDS:
                print(f"{original} command acknowledged; no calibration/setup is performed by the emulator.")
                self.send_ack(f"{original} command")
            elif command == b"?PT":
                self.send_tare_value(f"{original} command")
            elif command == b"?UW":
                with self.state.lock:
                    value = self.state.unit_mass_value
                self.send_labeled_value("UW", value, "?UW command")
            elif command == b"?KL":
                with self.state.lock:
                    key_lock = self.state.key_lock
                self.send_text(f"KL,{key_lock:03d}", "?KL command")
            elif command == b"?ID":
                self.send_text("ROBOTRICKLER", "?ID command")
            elif command == b"?SN":
                self.send_text("EMU00001", "?SN command")
            elif command == b"?TN":
                self.send_text("FX-120I-EMU", "?TN command")
            elif command.startswith(b"PT:"):
                value = parse_value_command(command, b"PT")
                self.state.set_tare_value(value)
                print(f"Preset tare value set to {value:g}")
                self.send_ack("preset tare command")
            elif command.startswith(b"UW:"):
                value = parse_value_command(command, b"UW")
                self.state.set_unit_mass_value(value)
                print(f"Unit mass value set to {value:g}")
                self.send_ack("unit mass command")
            elif command.startswith(b"KL:"):
                value = parse_number_command(command, b"KL", 3)
                self.state.set_key_lock(value)
                print(f"Key lock set to {value:03d}")
                self.send_ack("key lock command")
            else:
                print(f"RX unknown command: {original!r}")
                self.send_error("E01", "undefined command")
        except UnicodeDecodeError:
            self.send_error("E00", "non-ASCII command")
        except ValueError as exc:
            print(f"RX invalid command format: {exc}")
            self.send_error("E06", "format error")

    def _process_byte(self, value: int) -> None:
        if value in (0x0A, 0x0D):
            if not self.command_buffer:
                return
            raw = bytes(self.command_buffer)
            self.command_buffer.clear()
            self.handle_command(raw)
            return

        self.command_buffer.append(value)
        if len(self.command_buffer) > 64:
            print(f"RX command too long: {self.command_buffer!r}")
            self.command_buffer.clear()
            self.send_error("E04", "excess characters")

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

    def _stream_loop(self) -> None:
        while not self.stop_event.is_set():
            with self.stream_lock:
                enabled = self.stream_enabled
                interval = self.stream_interval

            if enabled:
                self.send_weight("stream")
                time.sleep(interval)
            else:
                time.sleep(0.05)


def print_help() -> None:
    print()
    print("Console commands")
    print("----------------")
    print("+ / Up arrow    increase weight by the current step")
    print("- / Down arrow  decrease weight by the current step")
    print("w <value>       set the displayed weight, for example: w 12.345")
    print("step <value>    set keypress increment/decrement size")
    print("u <unit>        set unit: g, gn, gr, kg, mg, oz, lb, ct, pcs, %")
    print("d <0-5>         set decimal places used in the output")
    print("j <value>       set random plus/minus jitter added to each print")
    print("stable          output ST/QT headers and release pending S commands")
    print("unstable        output US headers")
    print("overload +/-    output OL plus or OL minus frames")
    print("overload off    return to normal weighing output")
    print("stream <hz>     send frames continuously at the given rate")
    print("stream off      stop local stream output")
    print("ack on|off      enable or disable A&D AK/error responses")
    print("print           send one A&D standard weight frame")
    print("tare            set displayed weight to zero")
    print("status          show current emulator state")
    print("help            show this help")
    print("quit            close the serial port and exit")
    print()


def print_status(state: ScaleState, connection: serial.Serial, emulator: ScaleEmulator) -> None:
    with state.lock:
        overload = "off" if state.overload == 0 else "+" if state.overload > 0 else "-"
        status = "stable" if state.stable else "unstable"
        display = "on" if state.display_on else "off"
        ack = "on" if state.ack_enabled else "off"
        print(
            "Status: "
            f"port={connection.port} baud={connection.baudrate} "
            f"format={connection.bytesize}{connection.parity}{connection.stopbits:g} "
            f"weight={state.weight:.{state.decimals}f} unit={state.unit} "
            f"decimals={state.decimals} step={state.step} jitter={state.jitter} "
            f"{status} overload={overload} display={display} ack={ack}"
        )
    with emulator.stream_lock:
        print(
            "Stream: "
            f"{'on' if emulator.stream_enabled else 'off'} "
            f"rate={1.0 / emulator.stream_interval:g} Hz"
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


def parse_bool(argument: str) -> bool:
    value = argument.strip().lower()
    if value in {"1", "on", "true", "yes", "y"}:
        return True
    if value in {"0", "off", "false", "no", "n"}:
        return False
    raise ValueError("Use on/off, true/false, yes/no, or 1/0")


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
            elif command == "stable":
                emulator.state.set_stable(True)
                emulator.maybe_send_pending_stable()
                print_status(emulator.state, emulator.connection, emulator)
            elif command == "unstable":
                emulator.state.set_stable(False)
                print_status(emulator.state, emulator.connection, emulator)
            elif command == "overload":
                value = argument.lower()
                if value in {"off", "0", "none"}:
                    emulator.state.set_overload(0)
                elif value in {"+", "plus", "positive"}:
                    emulator.state.set_overload(1)
                elif value in {"-", "minus", "negative"}:
                    emulator.state.set_overload(-1)
                else:
                    print("Usage: overload +|-|off")
                    continue
                print_status(emulator.state, emulator.connection, emulator)
            elif command == "stream":
                if not argument:
                    print("Usage: stream <hz>|off")
                    continue
                if argument.lower() in {"off", "stop", "0"}:
                    emulator.set_stream(False)
                else:
                    emulator.set_stream_rate(float(argument))
                    emulator.set_stream(True)
            elif command == "ack":
                if not argument:
                    print("Usage: ack on|off")
                    continue
                emulator.state.set_ack_enabled(parse_bool(argument))
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
    print("A&D Scale Side Emulator")
    print("=======================")
    print("Supports the AD FX-120i command list: Q/SI, S, ESC P, SIR/C, T, R/Z/ESC T, U, PRT, and setup queries.")

    while True:
        port = choose_port()
        baud = choose_baud()
        serial_format = choose_serial_format()
        try:
            with open_serial(port, baud, serial_format) as connection:
                print(f"Opened {connection.port} at {connection.baudrate} {serial_format}.")
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
