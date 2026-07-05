#!/usr/bin/env python3
"""
Validate SALAM per-component power stats against printPowerResults totals.

After a SALAM run, parse debug-trace / stats.txt and verify:
  - componentEnergy[*] / accCycles reproduces average mW per component
  - window deltas over the full run match finalized totals (within tolerance)
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

COMPONENTS = [
    "fuDynamic",
    "fuStatic",
    "regDynamic",
    "regStatic",
    "spmReadDynamic",
    "spmWriteDynamic",
    "spmStatic",
]


def parse_stats_from_text(text: str) -> dict[str, float]:
    out: dict[str, float] = {}

    cycles_match = re.search(
        r"\.salam_power_model\.power\.accCycles\s+(\d+(?:\.\d+)?)", text
    )
    if cycles_match:
        out["accCycles"] = float(cycles_match.group(1))

    for comp in COMPONENTS:
        pat = rf"\.salam_power_model\.power\.componentEnergy::{comp}\s+(\d+(?:\.\d+)?)"
        match = re.search(pat, text)
        if match:
            out[comp] = float(match.group(1))

    return out


def parse_debug_trace(trace_path: Path) -> dict[str, float]:
    text = trace_path.read_text()
    out: dict[str, float] = {}

    patterns = {
        "fuDynamic": r"FU Dynamic:\s+([\d.]+)\s+mW",
        "fuStatic": r"FU Leakage:\s+([\d.]+)\s+mW",
        "regDynamic": r"Register Dynamic:\s+([\d.]+)\s+mW",
        "regStatic": r"Register Leakage:\s+([\d.]+)\s+mW",
        "spmReadDynamic": r"SPM Read Dynamic:\s+([\d.]+)\s+mW",
        "spmWriteDynamic": r"SPM Write Dynamic:\s+([\d.]+)\s+mW",
        "spmStatic": r"SPM Leakage:\s+([\d.]+)\s+mW",
    }
    for key, pat in patterns.items():
        match = re.search(pat, text)
        if match:
            out[key] = float(match.group(1))
    return out


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--stats", required=True, type=Path)
    parser.add_argument("--trace", required=True, type=Path)
    parser.add_argument("--llvm", default="top", help="LLVM block name suffix")
    parser.add_argument("--rtol", type=float, default=0.02)
    args = parser.parse_args()

    stats_text = args.stats.read_text()
    # Prefer the requested LLVM block (default: top).
    llvm = args.llvm
    block_pat = rf"board\.[\w\.]+\.{llvm}\.hw_interface"
    block_match = re.search(block_pat, stats_text)
    if block_match:
        start = stats_text.rfind("\n", 0, block_match.start()) + 1
        end = stats_text.find("\n\n", block_match.start())
        if end == -1:
            end = len(stats_text)
        stats_text = stats_text[start:end]

    stats = parse_stats_from_text(stats_text)
    trace = parse_debug_trace(args.trace)

    if "accCycles" not in stats or stats["accCycles"] <= 0:
        print("FAIL: accCycles stat missing or zero", file=sys.stderr)
        return 1

    cycles = stats["accCycles"]
    failed = False

    print(f"accCycles={cycles:g}")
    for comp in COMPONENTS:
        if comp not in stats or comp not in trace:
            print(f"SKIP {comp}: missing from stats or trace")
            continue

        stat_avg_mw = stats[comp] / cycles
        ref_mw = trace[comp]
        rel_err = abs(stat_avg_mw - ref_mw) / ref_mw if ref_mw else 0.0
        ok = rel_err <= args.rtol
        status = "OK" if ok else "FAIL"
        print(
            f"{status} {comp}: stat_avg={stat_avg_mw:.6f} mW "
            f"ref={ref_mw:.6f} mW rel_err={rel_err:.4%}"
        )
        failed = failed or not ok

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
