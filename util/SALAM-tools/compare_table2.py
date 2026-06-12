#!/usr/bin/env python3
# Copyright (c) 2025 Akanksha Chaudhari, Matt Sinclair
# SPDX-License-Identifier: BSD-3-Clause
"""
Compare gem5-SALAM stdlib run outputs against Spencer et al. JSA 2024 Table 2
(gem5-SALAMv2 column): performance (cycles), core power, SPM-inclusive power,
and FU area.

Usage:
  compare_table2.py OUTDIR [OUTDIR ...]

Each OUTDIR is expected to contain a debug-trace.txt produced by
run_salam_stdlib.py (e.g. BM_ARM_OUT/stdlib/<bench>). The benchmark name is
inferred from the directory name unless a SALAM_SUMMARY accelerator name makes
it obvious.
"""

import re
import sys
from pathlib import Path

# gem5-SALAMv2 ("v2") reference column from Table 2.
# value of None means the paper did not report that metric.
#   cycles, core_power (mW-equivalent), fu_area (um^2)
TABLE2_V2 = {
    "bfs": (15600, 1.3497, 7696.0),
    "fft": (91265, 59.3513, 39228.0),
    "gemm": (131900, 65.3655, 289400.0),
    "md_knn": (328025, 15.9594, 42791.0),
    "nw": (66962, 6.1975, 12152.0),
    "stencil2d": (109563, 43.4334, 9000.0),
    "stencil3d": (47210, None, None),
}

BANNER_RE = re.compile(r"^\s*(\S+\.llvm_interface)\s*$")
SUMMARY_RE = re.compile(
    r"^SALAM_SUMMARY\s+name=(\S+)\s+runtime_cycles=(\d+)"
    r"(?:\s+weighted_cycles=(\d+))?"
)
# Paper Table 2 cycles are upstream "Runtime" (wall-clock with no FU denial).
# With unlimited runtime FU slots (_available=0), mainline runtime_cycles
# matches upstream Runtime directly.  md_knn is the exception: the paper
# reports runtime * 5 (DP-mul pipeline depth = stages for double_multiplier).
PIPELINE_DEPTH_MULTIPLIER = {"md_knn": 5}
CORE_POWER_RE = re.compile(
    r"^\s*Accelerator Power \(core\):\s*([0-9.eE+-]+)\s*mW"
)
SPM_POWER_RE = re.compile(
    r"^\s*Accelerator Power \(SPM-inclusive\):\s*([0-9.eE+-]+)\s*mW"
)
FU_LEAK_RE = re.compile(r"^\s*FU Leakage:\s*([0-9.eE+-]+)\s*mW")
FU_DYN_RE = re.compile(r"^\s*FU Dynamic:\s*([0-9.eE+-]+)\s*mW")
SPM_LEAK_RE = re.compile(r"^\s*SPM Leakage:\s*([0-9.eE+-]+)\s*mW")
SPM_RD_RE = re.compile(r"^\s*SPM Read Dynamic:\s*([0-9.eE+-]+)\s*mW")
SPM_WR_RE = re.compile(r"^\s*SPM Write Dynamic:\s*([0-9.eE+-]+)\s*mW")
FU_AREA_RE = re.compile(r"^\s*FU Area:\s*([0-9.eE+-]+)\s*um\^2")
TOTAL_AREA_RE = re.compile(r"^\s*Total Area:\s*([0-9.eE+-]+)\s*um\^2")


def short_name(full):
    parts = full.split(".")
    if len(parts) >= 2 and parts[-1] == "llvm_interface":
        return parts[-2]
    return full


def parse_trace(path):
    """Return per-accelerator metrics parsed from debug-trace.txt."""
    accels = {}

    def slot(name):
        return accels.setdefault(
            name,
            {
                "cycles": None,
                "weighted_cycles": None,
                "core_power_mw": None,
                "spm_power_mw": None,
                "fu_leak_mw": None,
                "fu_dyn_mw": None,
                "spm_leak_mw": None,
                "spm_rd_mw": None,
                "spm_wr_mw": None,
                "fu_area": None,
                "total_area": None,
            },
        )

    text = path.read_text(errors="replace")
    current = None
    for line in text.splitlines():
        m = SUMMARY_RE.match(line)
        if m:
            entry = slot(short_name(m.group(1)))
            entry["cycles"] = int(m.group(2))
            if m.group(3) is not None:
                entry["weighted_cycles"] = int(m.group(3))
            continue
        m = BANNER_RE.match(line)
        if m:
            current = short_name(m.group(1))
            slot(current)
            continue
        if current is None:
            continue
        for regex, key in (
            (CORE_POWER_RE, "core_power_mw"),
            (SPM_POWER_RE, "spm_power_mw"),
            (FU_LEAK_RE, "fu_leak_mw"),
            (FU_DYN_RE, "fu_dyn_mw"),
            (SPM_LEAK_RE, "spm_leak_mw"),
            (SPM_RD_RE, "spm_rd_mw"),
            (SPM_WR_RE, "spm_wr_mw"),
            (FU_AREA_RE, "fu_area"),
            (TOTAL_AREA_RE, "total_area"),
        ):
            m = regex.search(line)
            if m:
                slot(current)[key] = float(m.group(1))
                break
    return accels


def pick_kernel(bench, accels):
    """Pick the kernel accelerator (not the 'top' controller)."""
    if bench in accels:
        return bench
    candidates = {
        n: d for n, d in accels.items() if n != "top" and d["cycles"]
    }
    if not candidates:
        candidates = {n: d for n, d in accels.items() if n != "top"}
    if not candidates:
        return None
    return max(
        candidates,
        key=lambda n: candidates[n]["cycles"] or 0,
    )


def fmt(v, prec=4):
    if v is None:
        return "-"
    if isinstance(v, int):
        return str(v)
    return f"{v:.{prec}f}"


def pct(meas, ref):
    if meas is None or ref is None or ref == 0:
        return None
    return (meas / ref - 1.0) * 100.0


def print_table(rows, power_key, title):
    header = (
        f"{'bench':<12}{'accel':<10}"
        f"{'cyc(sim)':>11}{'cyc(ref)':>11}{'d%':>8}  "
        f"{'pow(sim)':>10}{'pow(ref)':>10}{'d%':>8}  "
        f"{'area(sim)':>11}{'area(ref)':>11}{'d%':>8}"
    )
    print(title)
    print(header)
    print("-" * len(header))
    for bench, data, info in rows:
        if data is None:
            print(f"{bench:<12}{'-':<10}  ERROR: {info}")
            continue
        ref = TABLE2_V2.get(bench)
        rc = rp = ra = None
        if ref:
            rc, rp, ra = ref
        cyc = data["cycles"]
        if bench in PIPELINE_DEPTH_MULTIPLIER:
            cyc = cyc * PIPELINE_DEPTH_MULTIPLIER[bench]
        pw = data.get(power_key)
        ar = data["fu_area"]
        print(
            f"{bench:<12}{info:<10}"
            f"{fmt(cyc):>11}{fmt(rc):>11}{fmt(pct(cyc, rc), 2):>8}  "
            f"{fmt(pw):>10}{fmt(rp):>10}{fmt(pct(pw, rp), 2):>8}  "
            f"{fmt(ar, 1):>11}{fmt(ra, 1):>11}{fmt(pct(ar, ra), 2):>8}"
        )


def main():
    if len(sys.argv) < 2:
        print("usage: compare_table2.py OUTDIR [OUTDIR ...]", file=sys.stderr)
        sys.exit(1)

    rows = []
    for outdir in sys.argv[1:]:
        d = Path(outdir)
        bench = d.name
        trace = d / "debug-trace.txt"
        if not trace.is_file():
            rows.append((bench, None, "no debug-trace.txt"))
            continue
        accels = parse_trace(trace)
        kernel = pick_kernel(bench, accels)
        if kernel is None:
            rows.append((bench, None, "no accelerator found"))
            continue
        rows.append((bench, accels[kernel], kernel))

    print_table(
        rows, "core_power_mw", "=== Core power (Table 2 datapath row) ==="
    )
    print()
    print_table(
        rows,
        "spm_power_mw",
        "=== SPM-inclusive power (core + CACTI scratchpad) ===",
    )

    print()
    print("Power breakdown (kernel blocks with data):")
    print(
        f"{'bench':<12}{'accel':<10}"
        f"{'FU leak':>10}{'FU dyn':>10}"
        f"{'SPM leak':>10}{'SPM rd':>10}{'SPM wr':>10}"
    )
    print("-" * 72)
    for bench, data, info in rows:
        if data is None:
            continue
        print(
            f"{bench:<12}{info:<10}"
            f"{fmt(data.get('fu_leak_mw')):>10}"
            f"{fmt(data.get('fu_dyn_mw')):>10}"
            f"{fmt(data.get('spm_leak_mw')):>10}"
            f"{fmt(data.get('spm_rd_mw')):>10}"
            f"{fmt(data.get('spm_wr_mw')):>10}"
        )

    print()
    print("Notes:")
    print("  Table 2 paper power validated as core-only for BFS.")
    print("  Memory-heavy kernels: compare SPM-inclusive row to paper.")
    print("  md_knn cycles = runtime_cycles * 5 (DP-mul pipeline depth).")
    print("  area = FU Area [um^2].")


if __name__ == "__main__":
    main()
