#!/usr/bin/env python3
"""Read the custom ESP32 partition layout used by the build tools."""

from __future__ import annotations

import csv
from dataclasses import dataclass
from pathlib import Path


SKETCH_DIR = Path(__file__).resolve().parent.parent
PARTITIONS_CSV = SKETCH_DIR / "partitions.csv"


@dataclass(frozen=True)
class Partition:
    name: str
    type: str
    subtype: str
    offset: int
    size: int


def read_partitions(path: Path = PARTITIONS_CSV) -> dict[str, Partition]:
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


def require_partition(name: str, path: Path = PARTITIONS_CSV) -> Partition:
    try:
        return read_partitions(path)[name]
    except KeyError as exc:
        raise ValueError(f"Partition {name!r} not found in {path}") from exc
