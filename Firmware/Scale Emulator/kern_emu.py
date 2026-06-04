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
DEFAULT_DECIMALS = 2
DEFAULT_STEP = 0.02
DEFAULT_STREAM_HZ = 15.0
TERMINATOR = b"\r\n"

UNITS = ("g", "kg", "mg", "lb", "pcs", "%", "N", "kN", "t", "tf", "lbf", "klbf")

IMPLEMENTED_COMMANDS = (
    (0, "@"),
    (0, "I0"),
    (0, "I1"),
    (0, "KCPV"),
    (0, "KCPC"),
    (0, "I2"),
    (0, "IBMT"),
    (0, "I3"),
    (0, "I4"),
    (0, "IBIS"),
    (0, "I5"),
    (0, "IBIM"),
    (0, "S"),
    (0, "SI"),
    (0, "SIR"),
    (0, "SX"),
    (0, "SXI"),
    (0, "SXIR"),
    (0, "T"),
    (0, "TI"),
    (0, "TZ"),
    (0, "U"),
    (0, "Z"),
    (0, "ZI"),
    (1, "D"),
    (1, "DM"),
    (1, "DW"),
    (1, "IBBS"),
    (1, "K"),
    (1, "PWR"),
    (1, "IBRL"),
    (1, "IBRT"),
    (1, "IVERS"),
    (1, "TA"),
    (1, "TAI"),
    (1, "TAC"),
)


@dataclass
class ScaleState:
    weight: float = 0.0
    tare_value: float = 0.0
    unit: str = "g"
    decimals: int = DEFAULT_DECIMALS
    step: float = DEFAULT_STEP
    jitter: float = 0.0
    stable: bool = True
    overload: int = 0
    display_mode: str = "DEF"
    display_text: str = ""
    key_mode: int = 1
    powered: bool = True
    model: str = "PCB 1000-2"
    capacity: float = 1000.0
    serial_number: str = "EMU00001"
    software_version: str = "1.1.2"
    verified: bool = False
    lock: threading.Lock = field(default_factory=threading.Lock)

    def snapshot(self) -> tuple[float, str, int, bool, int]:
        with self.lock:
            weight = self.weight
            if self.jitter and self.overload == 0:
                weight += random.uniform(-self.jitter, self.jitter)
            return weight, self.unit, self.decimals, self.stable, self.overload

    def set_weight(self, value: float) -> None:
        with self.lock:
            self.weight = value
            self.overload = 0

    def adjust_weight(self, direction: int) -> float:
        with self.lock:
            self.weight += self.step * direction
            self.overload = 0
            return self.weight

    def tare(self) -> tuple[float, bool]:
        with self.lock:
            tare_weight = self.weight
            self.tare_value = tare_weight
            self.weight = 0.0
            self.overload = 0
            return tare_weight, self.stable

    def zero(self) -> bool:
        with self.lock:
            self.weight = 0.0
            self.tare_value = 0.0
            self.overload = 0
            return self.stable

    def clear_tare(self) -> None:
        with self.lock:
            self.tare_value = 0.0

    def set_tare_value(self, value: float) -> None:
        if value < 0:
            raise ValueError("Preset tare value cannot be negative")
        with self.lock:
            self.tare_value = value

    def set_unit(self, unit: str) -> None:
        if unit not in UNITS:
            raise ValueError(f"Unsupported unit {unit!r}; choose one of: {', '.join(UNITS)}")
        with self.lock:
            self.unit = unit

    def set_decimals(self, value: int) -> None:
        if not 0 <= value <= 6:
            raise ValueError("Decimals must be between 0 and 6")
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


def format_value(value: float, decimals: int, width: int = 10) -> str | None:
    text = f"{value:.{decimals}f}"
    if len(text) <= width:
        return text.rjust(width)

    for places in range(decimals - 1, -1, -1):
        text = f"{value:.{places}f}"
        if len(text) <= width:
            return text.rjust(width)

    return None


def format_weight_response(
    command: str,
    weight: float,
    unit: str,
    decimals: int,
    stable: bool,
    overload: int,
    extra_digit: bool = False,
) -> bytes:
    if overload:
        return f"{command} {'+' if overload > 0 else '-'}\r\n".encode("ascii")

    status = "S" if stable else "D"
    width = 11 if extra_digit else 10
    places = decimals + 1 if extra_digit else decimals
    value = format_value(weight, places, width)
    if value is None:
        return f"{command} {'+' if weight >= 0 else '-'}\r\n".encode("ascii")
    return f"{command} {status} {value} {unit}\r\n".encode("ascii")


def parse_value_with_unit(parts: list[str], command: str) -> tuple[float, str]:
    if len(parts) != 3:
        raise ValueError(f"{command} requires <value> <unit>")
    unit = parts[2]
    if unit not in UNITS:
        raise ValueError(f"Unsupported unit {unit!r}")
    return float(parts[1].replace(",", ".")), unit


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
        self.stream_command = "S"
        self.stream_extra_digit = False
        self.stream_interval = 1.0 / DEFAULT_STREAM_HZ
        self.pending_stable_requests: list[tuple[str, bool]] = []

    def start(self) -> None:
        self.rx_thread.start()
        self.stream_thread.start()
        self.send_serial_number("startup")

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

    def send_text(self, text: str, reason: str) -> None:
        self._write_packet(f"{text}\r\n".encode("ascii"), reason)

    def send_ack(self, command: str, reason: str) -> None:
        self.send_text(f"{command} A", reason)

    def send_logical_error(self, command: str, reason: str) -> None:
        print(f"{command} logical error: {reason}")
        self.send_text(f"{command} L", reason)

    def send_serial_number(self, reason: str) -> None:
        with self.state.lock:
            serial_number = self.state.serial_number
        self.send_text(f'I4 A "{serial_number}"', reason)

    def send_weight(self, response_command: str, reason: str, extra_digit: bool = False) -> None:
        weight, unit, decimals, stable, overload = self.state.snapshot()
        packet = format_weight_response(
            response_command,
            weight,
            unit,
            decimals,
            stable,
            overload,
            extra_digit,
        )
        self._write_packet(packet, reason)

    def send_tare_value(self, command: str, tare_value: float, stable: bool, reason: str) -> None:
        with self.state.lock:
            unit = self.state.unit
            decimals = self.state.decimals
        value = format_value(tare_value, decimals, 10)
        if value is None:
            self.send_text(f"{command} {'+' if tare_value >= 0 else '-'}", reason)
            return
        status = "S" if stable else "D"
        self.send_text(f"{command} {status} {value} {unit}", reason)

    def send_tare_query(self, command: str, reason: str, extra_digit: bool = False) -> None:
        with self.state.lock:
            tare_value = self.state.tare_value
            unit = self.state.unit
            decimals = self.state.decimals + (1 if extra_digit else 0)
        value = format_value(tare_value, decimals, 10)
        if value is None:
            self.send_text(f"{command} {'+' if tare_value >= 0 else '-'}", reason)
            return
        self.send_text(f"{command} A {value} {unit}", reason)

    def set_stream(self, enabled: bool, command: str = "S", extra_digit: bool = False) -> None:
        with self.stream_lock:
            self.stream_enabled = enabled
            self.stream_command = command
            self.stream_extra_digit = extra_digit
        print(f"Stream output {'enabled' if enabled else 'disabled'}.")

    def set_stream_interval_ms(self, milliseconds: int) -> None:
        if milliseconds <= 0:
            raise ValueError("Stream interval must be greater than zero")
        with self.stream_lock:
            self.stream_interval = milliseconds / 1000.0
        print(f"Stream interval set to {milliseconds} ms.")

    def set_stream_rate(self, hz: float) -> None:
        if hz <= 0:
            raise ValueError("Stream rate must be greater than zero")
        with self.stream_lock:
            self.stream_interval = 1.0 / hz
        print(f"Stream rate set to {hz:g} Hz.")

    def maybe_send_pending_stable(self) -> None:
        with self.state.lock:
            stable = self.state.stable
        if not stable or not self.pending_stable_requests:
            return

        pending = self.pending_stable_requests[:]
        self.pending_stable_requests.clear()
        for command, extra_digit in pending:
            self.send_weight(command, "deferred stable request", extra_digit)

    def cancel_streams_and_pending(self) -> None:
        self.pending_stable_requests.clear()
        self.set_stream(False)

    def handle_command(self, raw_command: bytes | str) -> None:
        if isinstance(raw_command, bytes):
            try:
                line = raw_command.decode("ascii")
            except UnicodeDecodeError:
                self.send_text("ES", "non-ASCII command")
                return
        else:
            line = raw_command

        line = line.strip()
        if not line:
            return

        print(f"RX command: {line!r}")
        parts = re.findall(r'"[^"]*"|\S+', line)
        command = parts[0]

        try:
            if command in {"S", "SX"} and len(parts) == 1:
                self.cancel_streams_and_pending()
                extra_digit = command == "SX"
                with self.state.lock:
                    stable = self.state.stable
                if stable:
                    self.send_weight(command, f"{command} command", extra_digit)
                else:
                    self.pending_stable_requests.append((command, extra_digit))
                    print("Stable request queued until the emulator is marked stable.")
            elif command in {"SI", "SXI"} and len(parts) == 1:
                self.cancel_streams_and_pending()
                self.send_weight("SX" if command == "SXI" else "S", f"{command} command", command == "SXI")
            elif command in {"SIR", "SXIR"}:
                if len(parts) > 2:
                    self.send_logical_error(command, "too many arguments")
                    return
                if len(parts) == 2:
                    self.set_stream_interval_ms(int(parts[1]))
                self.set_stream(True, "SX" if command == "SXIR" else "S", command == "SXIR")
            elif command == "@":
                self.cancel_streams_and_pending()
                with self.state.lock:
                    self.state.key_mode = 1
                    self.state.powered = True
                self.send_serial_number("@ command")
            elif command in {"T", "TI"} and len(parts) == 1:
                tare_value, stable = self.state.tare()
                response_stable = stable if command == "TI" else True
                self.send_tare_value(command, tare_value, response_stable, f"{command} command")
            elif command == "TZ" and len(parts) == 1:
                with self.state.lock:
                    weight = self.state.weight
                if abs(weight) <= 10 ** -self.state.decimals:
                    self.state.zero()
                    self.send_text("TZ A Z", "TZ zero command")
                else:
                    tare_value, _stable = self.state.tare()
                    with self.state.lock:
                        unit = self.state.unit
                        decimals = self.state.decimals
                    value = format_value(tare_value, decimals, 10)
                    self.send_text(f"TZ A T {value} {unit}", "TZ tare command")
            elif command in {"Z", "ZI"} and len(parts) == 1:
                stable = self.state.zero()
                if command == "ZI":
                    self.send_text(f"ZI {'S' if stable else 'D'}", "ZI command")
                else:
                    self.send_ack("Z", "Z command")
            elif command == "U":
                self.handle_unit_command(parts)
            elif command in {"TA", "TAI"}:
                self.handle_tare_memory_command(parts, command)
            elif command == "TAC" and len(parts) == 1:
                self.state.clear_tare()
                self.send_ack("TAC", "TAC command")
            elif command == "I0" and len(parts) == 1:
                for index, (level, implemented_command) in enumerate(IMPLEMENTED_COMMANDS):
                    status = "A" if index == len(IMPLEMENTED_COMMANDS) - 1 else "B"
                    self.send_text(f'I0 {status} {level} "{implemented_command}"', "I0 command")
            elif command in {"I1", "KCPV"} and len(parts) == 1:
                self.send_text(f'{command} A "01" "1.1.2" "1.1.2" "" ""', f"{command} command")
            elif command == "KCPC" and len(parts) == 1:
                self.send_text('KCPC B "Device"', "KCPC command")
                self.send_text('KCPC B "Device Display"', "KCPC command")
                self.send_text('KCPC A "Weighing Basic"', "KCPC command")
            elif command in {"I2", "IBMT"} and len(parts) == 1:
                with self.state.lock:
                    model = self.state.model
                    capacity = self.state.capacity
                    unit = self.state.unit
                    decimals = self.state.decimals
                self.send_text(f'{command} A "{model} {capacity:.{decimals}f} {unit}"', f"{command} command")
            elif command in {"I3", "I5"} and len(parts) == 1:
                with self.state.lock:
                    software_version = self.state.software_version
                self.send_text(f'{command} A "{software_version}"', f"{command} command")
            elif command in {"I4", "IBIS"}:
                self.handle_serial_command(parts, command)
            elif command == "IBIM":
                self.handle_model_command(parts)
            elif command == "D" and len(parts) == 2:
                text = parts[1].strip('"')
                with self.state.lock:
                    self.state.display_text = text
                    self.state.display_mode = "TXT"
                self.send_ack("D", "D command")
            elif command == "DM":
                self.handle_display_mode_command(parts)
            elif command == "DW" and len(parts) == 1:
                with self.state.lock:
                    self.state.display_mode = "DEF"
                self.send_ack("DW", "DW command")
            elif command == "IBBS" and len(parts) == 1:
                self.send_text("IBBS A 100 % F", "IBBS command")
            elif command == "K":
                self.handle_key_command(parts)
            elif command == "PWR":
                self.handle_power_command(parts)
            elif command == "IBRL" and len(parts) == 1:
                with self.state.lock:
                    capacity = self.state.capacity
                    unit = self.state.unit
                    readout = 10 ** -self.state.decimals
                self.send_text(f"IBRL A 0 {capacity:g} {unit} {readout:g} {unit}", "IBRL command")
            elif command == "IBRT" and len(parts) == 1:
                self.send_text("IBRT A SR", "IBRT command")
            elif command == "IVERS":
                self.handle_verified_command(parts)
            else:
                print(f"RX unknown command: {line!r}")
                self.send_text("ES", "unknown command")
        except (ValueError, IndexError) as exc:
            print(f"RX invalid command format: {exc}")
            self.send_logical_error(command, "invalid parameter")

    def handle_unit_command(self, parts: list[str]) -> None:
        if len(parts) == 1:
            with self.state.lock:
                unit = self.state.unit
            self.send_text(f"U A {unit}", "U query")
            return
        if len(parts) != 2:
            self.send_logical_error("U", "wrong argument count")
            return
        self.state.set_unit(parts[1])
        self.send_ack("U", "U set")

    def handle_tare_memory_command(self, parts: list[str], command: str) -> None:
        if len(parts) == 1:
            self.send_tare_query(command, f"{command} query", command == "TAI")
            return
        value, _unit = parse_value_with_unit(parts, command)
        self.state.set_tare_value(value)
        self.send_tare_query(command, f"{command} preset", command == "TAI")

    def handle_serial_command(self, parts: list[str], command: str) -> None:
        if len(parts) == 1:
            with self.state.lock:
                serial_number = self.state.serial_number
            self.send_text(f'{command} A "{serial_number}"', f"{command} query")
            return
        if command != "IBIS" or len(parts) != 2:
            self.send_logical_error(command, "invalid serial command")
            return
        with self.state.lock:
            self.state.serial_number = parts[1].strip('"')
        self.send_ack("IBIS", "IBIS set")

    def handle_model_command(self, parts: list[str]) -> None:
        if len(parts) == 1:
            with self.state.lock:
                model = self.state.model
            self.send_text(f'IBIM A "{model}"', "IBIM query")
            return
        if len(parts) != 2:
            self.send_logical_error("IBIM", "invalid model command")
            return
        model = parts[1].strip('"')
        if not 1 <= len(model) <= 31:
            self.send_logical_error("IBIM", "model length invalid")
            return
        with self.state.lock:
            self.state.model = model
        self.send_ack("IBIM", "IBIM set")

    def handle_display_mode_command(self, parts: list[str]) -> None:
        if len(parts) == 1:
            with self.state.lock:
                mode = self.state.display_mode
            self.send_text(f"DM A {mode}", "DM query")
            return
        if len(parts) != 2 or parts[1] not in {"DEF", "OFF", "TXT"}:
            self.send_logical_error("DM", "invalid display mode")
            return
        with self.state.lock:
            self.state.display_mode = parts[1]
        self.send_ack("DM", "DM set")

    def handle_key_command(self, parts: list[str]) -> None:
        if len(parts) != 2:
            self.send_logical_error("K", "mode required")
            return
        mode = int(parts[1])
        if mode not in (1, 2, 3, 4):
            self.send_logical_error("K", "mode out of range")
            return
        with self.state.lock:
            self.state.key_mode = mode
        self.send_ack("K", "K command")

    def handle_power_command(self, parts: list[str]) -> None:
        if len(parts) == 1:
            with self.state.lock:
                powered = self.state.powered
            self.send_text(f"PWR A {1 if powered else 0}", "PWR query")
            return
        if len(parts) != 2 or parts[1] not in {"0", "1", "2"}:
            self.send_logical_error("PWR", "invalid power state")
            return
        powered = parts[1] == "1"
        with self.state.lock:
            self.state.powered = powered
        self.send_ack("PWR", "PWR set")
        if powered:
            self.send_serial_number("PWR on")

    def handle_verified_command(self, parts: list[str]) -> None:
        if len(parts) == 1:
            with self.state.lock:
                verified = self.state.verified
            self.send_text(f"IVERS A {1 if verified else 0}", "IVERS query")
            return
        if len(parts) != 2 or parts[1] not in {"0", "1"}:
            self.send_logical_error("IVERS", "invalid verification state")
            return
        with self.state.lock:
            self.state.verified = parts[1] == "1"
        self.send_ack("IVERS", "IVERS set")

    def _process_byte(self, value: int) -> None:
        if value in (0x0A, 0x0D):
            if not self.command_buffer:
                return
            raw = bytes(self.command_buffer)
            self.command_buffer.clear()
            self.handle_command(raw)
            return

        self.command_buffer.append(value)
        if len(self.command_buffer) > 128:
            print(f"RX command too long: {self.command_buffer!r}")
            self.command_buffer.clear()
            self.send_text("ES", "excess characters")

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
                command = self.stream_command
                extra_digit = self.stream_extra_digit
                interval = self.stream_interval

            if enabled:
                self.send_weight(command, "stream", extra_digit)
                time.sleep(interval)
            else:
                time.sleep(0.05)


def print_help() -> None:
    print()
    print("Console commands")
    print("----------------")
    print("+ / Up arrow    increase weight by the current step")
    print("- / Down arrow  decrease weight by the current step")
    print("w <value>       set the displayed weight, for example: w 12.34")
    print("step <value>    set keypress increment/decrement size")
    print("u <unit>        set unit: g, kg, mg, lb, pcs, %, N, kN, t, tf, lbf, klbf")
    print("d <0-6>         set decimal places used in the output")
    print("j <value>       set random +/- jitter added to each print")
    print("stable          output S status and release pending S/SX commands")
    print("unstable        output D status for immediate/repeat commands")
    print("overload +/-    output KCP overload or underload frames")
    print("overload off    return to normal weighing output")
    print("stream <hz>     send S frames continuously at the given rate")
    print("stream off      stop local stream output")
    print("print           send one KCP S weight frame")
    print("tare            set displayed weight to zero and store tare memory")
    print("zero            set displayed weight and tare memory to zero")
    print("status          show current emulator state")
    print("help            show this help")
    print("quit            close the serial port and exit")
    print()


def print_status(state: ScaleState, connection: serial.Serial, emulator: ScaleEmulator) -> None:
    with state.lock:
        overload = "off" if state.overload == 0 else "+" if state.overload > 0 else "-"
        status = "stable" if state.stable else "unstable"
        print(
            "Status: "
            f"port={connection.port} baud={connection.baudrate} format=8N1 "
            f"weight={state.weight:.{state.decimals}f} tare={state.tare_value:.{state.decimals}f} "
            f"unit={state.unit} decimals={state.decimals} step={state.step} jitter={state.jitter} "
            f"{status} overload={overload} display={state.display_mode}"
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
                    print("Usage: d <0-6>")
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
            elif command in {"p", "print"}:
                emulator.send_weight("S", "local print")
            elif command in {"t", "tare"}:
                emulator.state.tare()
                print_status(emulator.state, emulator.connection, emulator)
            elif command in {"z", "zero"}:
                emulator.state.zero()
                print_status(emulator.state, emulator.connection, emulator)
            elif command == "status":
                print_status(emulator.state, emulator.connection, emulator)
            else:
                print(f"Unknown console command {command!r}. Type help for commands.")
        except ValueError as exc:
            print(exc)


def main() -> int:
    print("KERN KCP Scale Side Emulator")
    print("============================")
    print("Supports KCP level 0 weighing/device commands plus common level 1 display and tare queries.")

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
