#!/usr/bin/env python3
"""Validate SALAM closed-loop thermal feedback from trace files."""

from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path


def _read_csv(path: Path):
    with path.open() as f:
        return list(csv.DictReader(f))


def _f(row, *keys, default=0.0):
    for key in keys:
        if key in row and row[key] not in ("", None):
            return float(row[key])
    return default


def analyze_thermal_trace(rows, label_prefix: str) -> dict:
    temps = []
    powers = []
    statics = []
    for row in rows:
        label = row.get("label", "")
        if label_prefix and label_prefix not in label:
            continue
        temps.append(_f(row, "temp_k", "temperature_k"))
        powers.append(_f(row, "total_power_w", "power_w"))
        statics.append(_f(row, "static_power_w"))

    if len(temps) < 2:
        return {
            "count": len(temps),
            "delta_k": 0.0,
            "max_power_w": 0.0,
            "static_rise": 0.0,
            "first_temp_k": temps[0] if temps else 0.0,
            "last_temp_k": temps[-1] if temps else 0.0,
        }

    return {
        "count": len(temps),
        "delta_k": temps[-1] - temps[0],
        "max_power_w": max(powers) if powers else 0.0,
        "static_rise": (statics[-1] - statics[0]) if statics else 0.0,
        "first_temp_k": temps[0],
        "last_temp_k": temps[-1],
    }


def analyze_power_trace(rows, label_prefix: str) -> dict:
    dyn = []
    for row in rows:
        label = row.get("label", "")
        if label_prefix and label_prefix not in label:
            continue
        dyn.append(_f(row, "dynamic_power_w", "dynamic_W"))
    active = [v for v in dyn if v > 0.0]
    return {
        "intervals": len(dyn),
        "active_intervals": len(active),
        "max_dynamic_w": max(active) if active else 0.0,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--power-trace", type=Path, required=True)
    parser.add_argument("--thermal-trace", type=Path, required=True)
    parser.add_argument(
        "--label-prefix",
        default="",
        help="Filter trace rows to labels containing this prefix.",
    )
    parser.add_argument(
        "--min-temp-rise-k",
        type=float,
        default=0.0003,
        help="Minimum expected temperature rise when power is non-zero.",
    )
    args = parser.parse_args()

    if not args.power_trace.is_file():
        print(f"Missing power trace: {args.power_trace}", file=sys.stderr)
        return 2
    if not args.thermal_trace.is_file():
        print(f"Missing thermal trace: {args.thermal_trace}", file=sys.stderr)
        return 2

    power_rows = _read_csv(args.power_trace)
    thermal_rows = _read_csv(args.thermal_trace)

    power = analyze_power_trace(power_rows, args.label_prefix)
    thermal = analyze_thermal_trace(thermal_rows, args.label_prefix)

    print(
        f"power intervals={power['intervals']} active={power['active_intervals']}"
    )
    print(f"max dynamic={power['max_dynamic_w']:.6f} W")
    print(
        f"thermal samples={thermal['count']} "
        f"delta={thermal['delta_k']:.6f} K "
        f"({thermal['first_temp_k']:.3f} -> {thermal['last_temp_k']:.3f})"
    )

    failed = False
    if power["active_intervals"] == 0:
        print("FAIL: no active power intervals")
        failed = True
    if thermal["count"] < 2:
        print("FAIL: insufficient thermal samples")
        failed = True
    elif (
        power["max_dynamic_w"] > 0
        and thermal["delta_k"] < args.min_temp_rise_k
    ):
        print(
            f"FAIL: temperature rise {thermal['delta_k']:.6f} K "
            f"< {args.min_temp_rise_k} K with non-zero power"
        )
        failed = True
    else:
        print("OK: closed-loop thermal response observed")

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
