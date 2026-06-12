#!/usr/bin/env python3
"""Verify benchmark YAML FU profiles match generated FunctionalUnits.py."""

import argparse
import re
import sys
from pathlib import Path

import yaml


def load_yaml_profile(profile_dir: Path, fu: str) -> dict:
    yml = profile_dir / fu / f"{fu}.yml"
    data = yaml.safe_load(yml.read_text())
    pm = data["functional_unit"]["power_model"]
    return {
        "alias": data["functional_unit"]["parameters"]["alias"],
        "fu_latency": pm["latency"],
        "area": float(pm["area"]),
        "dynamic_power": float(pm["dynamic_power"]),
        "leakage_power": float(pm["leakage_power"]),
    }


def parse_functional_units_py(path: Path) -> dict:
    text = path.read_text()
    out = {}
    for block in re.split(r"(?=^class \w+\(SimObject\):)", text, flags=re.M):
        m = re.match(r"class (\w+)\(SimObject\):", block)
        if not m:
            continue
        cls = m.group(1)
        area_m = re.search(r"area = Param\.(?:Float|UInt32)\(([^,)]+)", block)
        lat_m = re.search(r"fu_latency = Param\.UInt32\((\d+)", block)
        if area_m:
            out.setdefault(cls, {})["area"] = float(area_m.group(1))
        if lat_m:
            out.setdefault(cls, {})["fu_latency"] = int(lat_m.group(1))
    return out


def fu_to_class(fu: str) -> str:
    return "".join(part.capitalize() for part in fu.split("_"))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bench", default="bfs")
    parser.add_argument("--latency", choices=["5ns", "10ns"], required=True)
    parser.add_argument("--acc-bench-path", required=True)
    parser.add_argument("--m5-path", required=True)
    args = parser.parse_args()

    profile_dir = (
        Path(args.acc_bench_path)
        / "sys_validation"
        / args.bench
        / "configs"
        / "hw_interface"
        / "functional_units"
        / "40nm_model"
        / args.latency
        / "default_profile"
    )
    fu_py = Path(args.m5_path) / "src" / "salam" / "FunctionalUnits.py"
    if not profile_dir.is_dir():
        print(f"Missing profile dir: {profile_dir}", file=sys.stderr)
        return 1
    if not fu_py.is_file():
        print(f"Missing FunctionalUnits.py: {fu_py}", file=sys.stderr)
        return 1

    generated = parse_functional_units_py(fu_py)
    fus = sorted(
        p.name
        for p in profile_dir.iterdir()
        if p.is_dir() and (p / f"{p.name}.yml").is_file()
    )

    print(f"Profile epoch: {args.latency}")
    print(f"YAML source:   {profile_dir}")
    print(f"Generated:     {fu_py}")
    print()
    print(
        f"{'FU':<22} {'yaml_area':>12} {'py_area':>12} "
        f"{'yaml_lat':>8} {'py_lat':>6} {'match':>6}"
    )
    print("-" * 72)

    mismatches = 0
    for fu in fus:
        yaml_vals = load_yaml_profile(profile_dir, fu)
        cls = fu_to_class(fu)
        py_vals = generated.get(cls, {})
        area_ok = abs(yaml_vals["area"] - py_vals.get("area", -1)) < 0.01
        lat_ok = yaml_vals["fu_latency"] == py_vals.get("fu_latency", -1)
        ok = area_ok and lat_ok
        if not ok:
            mismatches += 1
        print(
            f"{fu:<22} {yaml_vals['area']:12.4f} "
            f"{py_vals.get('area', float('nan')):12.4f} "
            f"{yaml_vals['fu_latency']:8d} "
            f"{py_vals.get('fu_latency', -1):6d} "
            f"{'OK' if ok else 'FAIL':>6}"
        )

    print()
    if mismatches:
        print(f"FAILED: {mismatches} functional unit mismatch(es)")
        return 1
    print("PASS: all functional units match YAML profiles")
    return 0


if __name__ == "__main__":
    sys.exit(main())
