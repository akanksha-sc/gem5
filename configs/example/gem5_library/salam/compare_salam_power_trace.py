#!/usr/bin/env python3
"""Compare SALAM power_trace.csv interval rows against stats.txt totals."""

from __future__ import annotations

import argparse
import csv
import re
import sys
from pathlib import Path


def parse_stats_totals(stats_path: Path, llvm_suffix: str = "top") -> dict:
    text = stats_path.read_text()
    block_pat = rf"board\.[\w\.]+\.{llvm_suffix}\.hw_interface"
    block_match = re.search(block_pat, text)
    if not block_match:
        raise ValueError(f"No hw_interface block found for llvm={llvm_suffix}")

    start = text.rfind("\n", 0, block_match.start()) + 1
    end = text.find("\n\n", block_match.start())
    if end == -1:
        end = len(text)
    block_text = text[start:end]

    totals = {}
    cycles = 0.0
    for line in block_text.splitlines():
        if f".{llvm_suffix}.hw_interface.salam_power_model" not in line:
            continue

        m = re.search(r"\.power\.componentEnergy::(\w+)\s+([\d.eE+-]+)", line)
        if m:
            totals[m.group(1)] = float(m.group(2))
            continue

        m = re.search(r"\.power\.accCycles\s+([\d.eE+-]+)", line)
        if m:
            cycles = float(m.group(1))

    if cycles <= 0:
        raise ValueError("accCycles missing from stats.txt")

    return {"cycles": cycles, "energy": totals}


def parse_trace_averages(trace_path: Path, label_prefix: str) -> dict:
    dyn_active = {}
    dyn_weighted = {}
    st_weighted = {}
    weights = {}

    with trace_path.open() as f:
        reader = csv.DictReader(f)
        for row in reader:
            label = row.get("label", "")
            if label_prefix not in label:
                continue
            block = label.split(".")[-1]
            dyn = float(row.get("dynamic_power_w", row.get("dynamic_W", 0.0)))
            st = float(row.get("static_power_w", row.get("static_W", 0.0)))
            dur = float(row.get("sample_duration_ticks", 0.0))
            if dur <= 0:
                continue
            dyn_weighted[block] = dyn_weighted.get(block, 0.0) + dyn * dur
            st_weighted[block] = st_weighted.get(block, 0.0) + st * dur
            weights[block] = weights.get(block, 0.0) + dur
            if dyn > 0.0:
                dyn_active.setdefault(block, []).append(dyn)

    dyn_avg = {
        block: dyn_weighted[block] / weights[block]
        for block in dyn_weighted
        if weights[block] > 0
    }
    dyn_active_avg = {
        block: sum(vals) / len(vals) for block, vals in dyn_active.items()
    }
    st_avg = {
        block: st_weighted[block] / weights[block]
        for block in st_weighted
        if weights[block] > 0
    }
    return {
        "dynamic": dyn_avg,
        "dynamic_active": dyn_active_avg,
        "static": st_avg,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--stats", required=True, type=Path)
    parser.add_argument("--trace", required=True, type=Path)
    parser.add_argument("--label-prefix", default="board.bfs_clstr.top")
    parser.add_argument("--llvm", default="top")
    parser.add_argument("--rtol", type=float, default=0.05)
    args = parser.parse_args()

    stats = parse_stats_totals(args.stats, args.llvm)
    cycles = stats["cycles"]
    energy = stats["energy"]

    stat_dyn = {
        "datapath": (energy.get("fuDynamic", 0) / cycles) * 1e-3,
        "registers": (energy.get("regDynamic", 0) / cycles) * 1e-3,
        "spm": (
            energy.get("spmReadDynamic", 0) + energy.get("spmWriteDynamic", 0)
        )
        / cycles
        * 1e-3,
    }

    trace_avg = parse_trace_averages(args.trace, args.label_prefix)

    failed = False
    for block in ("datapath", "registers", "spm"):
        if block not in trace_avg["dynamic_active"]:
            print(f"SKIP {block}: no active trace rows")
            continue
        ref = stat_dyn[block]
        got = trace_avg["dynamic_active"][block]
        rel = abs(got - ref) / ref if ref else 0.0
        ok = ref == 0.0 or rel <= args.rtol
        status = "OK" if ok else "FAIL"
        print(
            f"{status} {block}: trace_dyn={got:.6f}W "
            f"stat_dyn={ref:.6f}W rel={rel:.4%} (active intervals)"
        )
        if block in trace_avg["static"]:
            print(
                f"  static(trace)={trace_avg['static'][block]:.6f}W "
                "(regression-based, not compared to stats)"
            )
        failed = failed or not ok

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
