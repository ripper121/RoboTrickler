#!/usr/bin/env python3
"""Read the ESP32 partition layout selected by the project board settings."""

from __future__ import annotations

import csv
import json
import os
from dataclasses import dataclass
from pathlib import Path


SKETCH_DIR = Path(__file__).resolve().parent.parent
ARDUINO_JSON = SKETCH_DIR / ".vscode" / "arduino.json"
ESP32_CORE_VERSION = "3.3.10"
DEFAULT_PARTITION_SCHEME = "default_8MB"
PARTITION_SCHEME_ENV = "RTUI_PARTITION_SCHEME"


@dataclass(frozen=True)
class Partition:
    name: str
    type: str
    subtype: str
    offset: int
    size: int


def selected_partition_scheme() -> str:
    override = os.environ.get(PARTITION_SCHEME_ENV)
    if override:
        return override

    if not ARDUINO_JSON.is_file():
        return DEFAULT_PARTITION_SCHEME

    with ARDUINO_JSON.open("r", encoding="utf-8") as config_file:
        config = json.load(config_file)

    for option in str(config.get("configuration", "")).split(","):
        if option.startswith("PartitionScheme="):
            return option.split("=", 1)[1]
    return DEFAULT_PARTITION_SCHEME


def selected_partitions_csv() -> Path:
    scheme = selected_partition_scheme()
    if scheme == "custom":
        path = SKETCH_DIR / "partitions.csv"
    else:
        path = (
            Path(os.environ.get("LOCALAPPDATA", ""))
            / "Arduino15"
            / "packages"
            / "esp32"
            / "hardware"
            / "esp32"
            / ESP32_CORE_VERSION
            / "tools"
            / "partitions"
            / f"{scheme}.csv"
        )

    if not path.is_file():
        raise FileNotFoundError(f"Partition CSV not found for scheme {scheme!r}: {path}")
    return path


def read_partitions(path: Path | None = None) -> dict[str, Partition]:
    if path is None:
        path = selected_partitions_csv()
    partitions: dict[str, Partition] = {}
    with path.open("r", encoding="utf-8", newline="") as partition_file:
        rows = (line for line in partition_file if not line.lstrip().startswith("#"))
        for row in csv.reader(rows, skipinitialspace=True):
            if not row or not row[0].strip():
                continue
            name, partition_type, subtype, offset, size = (value.strip() for value in row[:5])
            partitions[name] = Partition(
                name=name,
                type=partition_type,
                subtype=subtype,
                offset=int(offset, 0),
                size=int(size, 0),
            )
    return partitions


def require_partition(name: str, path: Path | None = None) -> Partition:
    if path is None:
        path = selected_partitions_csv()
    try:
        return read_partitions(path)[name]
    except KeyError as exc:
        raise ValueError(f"Partition {name!r} not found in {path}") from exc
