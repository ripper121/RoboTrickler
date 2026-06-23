#!/usr/bin/env python3
"""Create trickler profiles on the device through the web file-editor API.

Profiles are generated with the same math the old in-browser "Profile Generator"
used and uploaded as ``/profiles/<name>.txt`` via a multipart POST to
``/system/resources/edit`` (the same endpoint the web editor and curl use).

Examples
--------
Create a single profile named ``powder`` from a 38.5 gn calibration run::

    python tools/create_profiles.py --name powder --weight 38.5

Bulk-create 999 profiles (profile001 .. profile999)::

    python tools/create_profiles.py --count 999

Generate the JSON locally without touching the device::

    python tools/create_profiles.py --count 10 --dry-run --out-dir ./generated

Note: the firmware only lists/uses PROFILE_LIST_MAX (32) profiles at a time, but
the device happily stores more files than that.
"""

from __future__ import annotations

import argparse
import json
import math
import re
import sys
import urllib.error
import urllib.request
import uuid
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent

# Hard cap requested for this tool. Profile names are zero-padded to 3 digits so
# they sort correctly up to this many.
MAX_PROFILES = 999

# Conversion factor used by the original generator (grains per gram).
GRAM_TO_GRAIN = 15.4323583529

# Canonical diff-weight ladder in grains and the per-step measurement counts that
# pair with it, copied verbatim from the removed profileGenerator.html.
DIFF_WEIGHTS_GRAIN = [1.929, 0.965, 0.482, 0.241, 0.121, 0.060, 0.030, 0.000]
MAP_MEASUREMENTS = [2, 2, 5, 10, 15, 15, 20, 25]

# Reserved Windows/FAT device names that may not be used as bare filenames.
_RESERVED_NAMES = {"CON", "PRN", "AUX", "NUL"} | {f"COM{i}" for i in range(1, 10)} | {
    f"LPT{i}" for i in range(1, 10)
}


def js_round(value: float) -> int:
    """Round half away from +inf, matching JavaScript ``Math.round``."""
    return int(math.floor(value + 0.5))


def round3(value: float) -> float:
    """Match the generator's ``parseFloat(x.toFixed(3))`` cleanup."""
    return round(value + 0.0, 3)


def fat32_safe_name(name: str) -> str:
    """Mirror the generator's convertToLegalFAT32Filename (sans the .txt)."""
    legal = name.strip()[:255]
    legal = re.sub(r'[<>:"/\\|?*]', "_", legal)
    if legal.upper() in _RESERVED_NAMES:
        legal += "_file"
    return legal


def generate_profile(
    *,
    weight: float,
    target_weight: float,
    weight_gap: float,
    rpm: int,
    measurements: int,
    bulk_stepper: str,
    calc_tolerance: float,
    unit: str,
    alarm_threshold: float,
    tolerance: float,
    start_at_zero: bool,
    trickle_counter: bool,
) -> dict:
    """Build a profile dict using the original generator algorithm.

    ``weight`` is the powder weight measured during a 20000-step calibration run;
    it drives ``weightPerRev`` and the per-entry step counts.
    """
    if weight <= 0:
        raise ValueError("calibration weight must be greater than 0")

    bulk = "stepper2" if bulk_stepper == "stepper2" else "stepper1"

    profile = {
        "general": {
            "targetWeight": round3(target_weight),
            "tolerance": round3(tolerance),
            "alarmThreshold": round3(alarm_threshold),
            "weightGap": round3(weight_gap),
            "bulkStepper": bulk,
            "startAtZero": start_at_zero,
            "trickleCounter": trickle_counter,
            "measurements": int(round(measurements)),
        },
        "steppers": {
            "stepper1": {
                "enabled": True,
                "weightPerRev": round3(weight / 100.0),
                "rpm": rpm,
            },
            "stepper2": {
                "enabled": bulk == "stepper2",
                "weightPerRev": 10.000,
                "rpm": rpm if bulk == "stepper2" else 200,
            },
        },
        "trickleMap": [],
    }

    weights = list(DIFF_WEIGHTS_GRAIN)
    if unit == "gram":
        weights = [w / GRAM_TO_GRAIN for w in weights]
    weights = [round3(w) for w in weights]

    general_measurements = int(round(measurements))
    for i, diff_weight in enumerate(weights):
        steps = js_round((diff_weight / weight) * ((20000 / 100) * calc_tolerance))
        steps = max(steps, 5)
        entry_measurements = max(MAP_MEASUREMENTS[i], general_measurements)
        profile["trickleMap"].append(
            {
                "diffWeight": diff_weight,
                "stepper": "stepper1",
                "steps": steps,
                "rpm": rpm,
                "measurements": entry_measurements,
            }
        )

    return profile


def profile_to_text(profile: dict) -> str:
    return json.dumps(profile, indent=4)


def build_multipart(device_path: str, body_text: str) -> tuple[bytes, str]:
    """Build a multipart/form-data body. The filename is the on-device path."""
    boundary = f"----RoboTrickler{uuid.uuid4().hex}"
    payload = body_text.encode("utf-8")
    head = (
        f"--{boundary}\r\n"
        f'Content-Disposition: form-data; name="data"; filename="{device_path}"\r\n'
        f"Content-Type: text/json\r\n\r\n"
    ).encode("utf-8")
    tail = f"\r\n--{boundary}--\r\n".encode("utf-8")
    return head + payload + tail, f"multipart/form-data; boundary={boundary}"


def upload_profile(host: str, device_path: str, body_text: str, timeout: float) -> None:
    """POST one profile to the device. Raises urllib errors on failure."""
    body, content_type = build_multipart(device_path, body_text)
    url = f"http://{host}/system/resources/edit"
    request = urllib.request.Request(
        url,
        data=body,
        method="POST",
        headers={"Content-Type": content_type, "Expect": ""},
    )
    with urllib.request.urlopen(request, timeout=timeout) as response:
        status = response.getcode()
    if status != 200:
        raise RuntimeError(f"unexpected HTTP status {status}")


def profile_names(args: argparse.Namespace) -> list[tuple[str, str]]:
    """Return (name, device_path) pairs for every profile to create."""
    pairs: list[tuple[str, str]] = []
    if args.count == 1 and args.name:
        names = [args.name]
    else:
        names = [
            f"{args.prefix}{i:03d}"
            for i in range(args.start_index, args.start_index + args.count)
        ]
    for raw in names:
        clean = raw[:-4] if raw.lower().endswith(".txt") else raw
        clean = fat32_safe_name(clean.lower())
        pairs.append((clean, f"/profiles/{clean}.txt"))
    return pairs


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate and upload trickler profiles via the web API.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )

    target = parser.add_argument_group("target")
    target.add_argument("--host", default="robo-trickler.local", help="device hostname or IP")
    target.add_argument("--timeout", type=float, default=15.0, help="HTTP timeout (s) per upload")
    target.add_argument(
        "--dry-run",
        action="store_true",
        help="generate profiles without uploading (implied when no host is reachable only if set)",
    )
    target.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="also write generated profiles to this directory",
    )

    naming = parser.add_argument_group("naming")
    naming.add_argument(
        "--count",
        type=int,
        default=1,
        help=f"how many profiles to create (1..{MAX_PROFILES})",
    )
    naming.add_argument("--name", default="powder", help="name for a single profile (--count 1)")
    naming.add_argument("--prefix", default="profile", help="name prefix when --count > 1")
    naming.add_argument("--start-index", type=int, default=1, help="first index when --count > 1")

    gen = parser.add_argument_group("profile parameters (match the old web generator)")
    gen.add_argument("--weight", type=float, default=40.0, help="calibration-run weight")
    gen.add_argument("--target-weight", type=float, default=40.0, help="stored target weight")
    gen.add_argument("--weight-gap", type=float, default=1.0, help="weight gap")
    gen.add_argument("--rpm", type=int, default=200, help="stepper speed (rpm)")
    gen.add_argument("--measurements", type=int, default=2, help="general measurement count")
    gen.add_argument(
        "--bulk-stepper",
        choices=["stepper1", "stepper2"],
        default="stepper1",
        help="bulk stepper",
    )
    gen.add_argument(
        "--calc-tolerance",
        type=float,
        default=65.0,
        help="calculation tolerance in percent",
    )
    gen.add_argument("--unit", choices=["grain", "gram"], default="grain", help="weight unit")
    gen.add_argument("--alarm-threshold", type=float, default=1.0, help="alarm threshold")
    gen.add_argument("--tolerance", type=float, default=0.0, help="tolerance")
    gen.add_argument("--start-at-zero", action="store_true", help="only start trickling at 0.000")
    gen.add_argument("--trickle-counter", action="store_true", help="enable per-profile counter")

    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)

    if not 1 <= args.count <= MAX_PROFILES:
        print(f"Error: --count must be between 1 and {MAX_PROFILES}", file=sys.stderr)
        return 2
    if args.start_index < 0:
        print("Error: --start-index must be >= 0", file=sys.stderr)
        return 2

    try:
        template = generate_profile(
            weight=args.weight,
            target_weight=args.target_weight,
            weight_gap=args.weight_gap,
            rpm=args.rpm,
            measurements=args.measurements,
            bulk_stepper=args.bulk_stepper,
            calc_tolerance=args.calc_tolerance,
            unit=args.unit,
            alarm_threshold=args.alarm_threshold,
            tolerance=args.tolerance,
            start_at_zero=args.start_at_zero,
            trickle_counter=args.trickle_counter,
        )
    except ValueError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 2

    body_text = profile_to_text(template)
    pairs = profile_names(args)

    out_dir = args.out_dir.resolve() if args.out_dir else None
    if out_dir:
        out_dir.mkdir(parents=True, exist_ok=True)

    failures = 0
    for index, (name, device_path) in enumerate(pairs, start=1):
        prefix = f"[{index}/{len(pairs)}] {name}"

        if out_dir:
            (out_dir / f"{name}.txt").write_text(body_text, encoding="utf-8")

        if args.dry_run:
            print(f"{prefix}: generated (dry-run) -> {device_path}")
            continue

        try:
            upload_profile(args.host, device_path, body_text, args.timeout)
            print(f"{prefix}: uploaded -> {device_path}")
        except (urllib.error.URLError, RuntimeError, OSError) as exc:
            failures += 1
            print(f"{prefix}: FAILED -> {exc}", file=sys.stderr)

    if args.dry_run:
        print(f"Done. Generated {len(pairs)} profile(s).")
    else:
        ok = len(pairs) - failures
        print(f"Done. Uploaded {ok}/{len(pairs)} profile(s) to {args.host}.")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
