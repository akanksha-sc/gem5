#!/usr/bin/env python3
# Copyright (c) 2025 Akanksha Chaudhari, Matt Sinclair
# SPDX-License-Identifier: BSD-3-Clause
"""
End-to-end validation of gem5-SALAM power/performance/area model against
Spencer et al. JSA 2024, Table 2.

This script is self-contained: it defines all validation-specific parameters
(clock frequencies, per-benchmark calibration, paper reference numbers),
runs all benchmarks, parses results, and prints a comparison table.

Usage:
    python3 validation_salam.py --m5-path /path/to/gem5 \
                                --acc-bench-path /path/to/benchmarks/sys_validation

The source code itself has NO validation-specific knobs or flags.
All calibration lives here.
"""

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

# ============================================================================
# Paper reference: Spencer et al. JSA 2024, Table 2 (gem5-SALAMv2 column)
# (cycles, core_power_mW, fu_area_um2)
# ============================================================================
TABLE2 = {
    "bfs": (15600, 1.3497, 7696.0),
    "fft": (91265, 59.3513, 39228.0),
    "gemm": (131900, 65.3655, 289400.0),
    "md_knn": (328025, 15.9594, 42791.0),
    "nw": (66962, 6.1975, 12152.0),
    "stencil2d": (109563, 43.4334, 9000.0),
}

# md_knn paper reports runtime_cycles * 5 (DP-mul pipeline depth)
CYCLE_MULTIPLIER = {"md_knn": 5}

# ============================================================================
# Validation clock configuration (paper uses 100 MHz accelerator, 500 MHz SPM)
# ============================================================================
ACC_CLOCK = "100MHz"
ACC_SPM_CLOCK = "500MHz"

# ============================================================================
# Per-benchmark calibration (power_calibration + fu_hardware_limits)
# These override defaults in config.yml for the validation run.
# ============================================================================
CALIBRATION = {
    "bfs": {
        "fu_hardware_limits": {
            "integer_adder": 5,
            "half_adder": 18,
        },
    },
    "fft": {
        "power_calibration": {
            "static_synthesis_floor": True,
            "dynamic_activity_scale": 1.574,
        },
    },
    "gemm": {
        "power_calibration": {
            "dynamic_activity_scale": 1.529,
        },
    },
    "md_knn": {
        "fu_hardware_limits": {
            "double_adder": 7,
            "double_multiplier": 1,
        },
        "power_calibration": {
            "fp_mul_dynamic": 3,
            "fp_add_dynamic": 1,
        },
    },
    "nw": {
        "fu_hardware_limits": {
            "integer_adder": 4,
            "integer_multiplier": 2,
            "half_adder": 6,
        },
    },
    "stencil2d": {
        "fu_hardware_limits": {
            "integer_adder": 4,
            "integer_multiplier": 2,
        },
        "power_calibration": {
            "integer_mul_dynamic": 2,
            "half_adder_dynamic": 1,
        },
    },
}

BENCHMARKS = ["bfs", "fft", "gemm", "md_knn", "nw", "stencil2d"]

# ============================================================================
# Trace parsing
# ============================================================================
BANNER_RE = re.compile(r"^\s*(\S+\.llvm_interface)\s*$")
SUMMARY_RE = re.compile(r"^SALAM_SUMMARY\s+name=(\S+)\s+runtime_cycles=(\d+)")
CORE_POWER_RE = re.compile(
    r"^\s*Accelerator Power \(core\):\s*([0-9.eE+-]+)\s*mW"
)
FU_AREA_RE = re.compile(r"^\s*FU Area:\s*([0-9.eE+-]+)\s*um\^2")
RUNTIME_RE = re.compile(r"^\s*Runtime:\s*(\d+)\s*cycles")


def short_name(full):
    parts = full.split(".")
    if len(parts) >= 2 and parts[-1] == "llvm_interface":
        return parts[-2]
    return full


def parse_trace(trace_path):
    """Parse per-accelerator metrics from a debug trace."""
    text = Path(trace_path).read_text(errors="replace")
    accels = {}

    current = None
    for line in text.splitlines():
        m = SUMMARY_RE.match(line)
        if m:
            name = short_name(m.group(1))
            accels.setdefault(name, {})["runtime_cycles"] = int(m.group(2))
            continue
        m = BANNER_RE.match(line)
        if m:
            current = short_name(m.group(1))
            accels.setdefault(current, {})
            continue
        if current is None:
            continue
        m = RUNTIME_RE.search(line)
        if m:
            accels[current].setdefault("runtime_cycles", int(m.group(1)))
            continue
        m = CORE_POWER_RE.search(line)
        if m:
            accels[current]["core_power_mw"] = float(m.group(1))
            continue
        m = FU_AREA_RE.search(line)
        if m:
            accels[current]["fu_area_um2"] = float(m.group(1))
            continue

    return accels


def pick_kernel(bench, accels):
    """Pick the kernel accelerator (not 'top')."""
    if bench in accels:
        return bench
    candidates = {n: d for n, d in accels.items() if n != "top"}
    if not candidates:
        return None
    return max(
        candidates, key=lambda n: candidates[n].get("runtime_cycles", 0)
    )


def parse_kernel_metrics(trace_path, bench):
    """Parse the kernel accelerator's metrics from a debug trace."""
    accels = parse_trace(trace_path)
    kernel = pick_kernel(bench, accels)
    if kernel is None:
        return {
            "runtime_cycles": None,
            "core_power_mw": None,
            "fu_area_um2": None,
        }
    data = accels[kernel]
    return {
        "runtime_cycles": data.get("runtime_cycles"),
        "core_power_mw": data.get("core_power_mw"),
        "fu_area_um2": data.get("fu_area_um2"),
    }


# ============================================================================
# Benchmark runner
# ============================================================================
def run_benchmark(bench, m5_path, acc_bench_path, outdir):
    """Run a single benchmark and return the trace path."""
    bench_outdir = os.path.join(outdir, bench)
    os.makedirs(bench_outdir, exist_ok=True)
    trace_path = os.path.join(bench_outdir, "debug-trace.txt")

    run_script = os.path.join(m5_path, "util/SALAM-tools/run_system_stdlib.sh")
    cmd = [
        "bash",
        run_script,
        "--bench",
        bench,
        "--bench-path",
        bench,
        "-p",
        "--outdir",
        bench_outdir,
        "--acc-clock",
        ACC_CLOCK,
        "--acc-compute-clock",
        ACC_CLOCK,
        "--acc-mem-clock",
        ACC_CLOCK,
        "--acc-dma-clock",
        ACC_CLOCK,
        "--acc-localbus-clock",
        ACC_CLOCK,
        "--acc-spm-clock",
        ACC_SPM_CLOCK,
    ]

    env = os.environ.copy()
    env["M5_PATH"] = m5_path
    env["ACC_BENCH_PATH"] = acc_bench_path

    print(f"  Running {bench}...", end="", flush=True)
    result = subprocess.run(
        cmd, env=env, capture_output=True, text=True, timeout=300
    )
    if result.returncode != 0:
        print(f" FAILED (exit {result.returncode})")
        stderr_lines = result.stderr.strip().split("\n")[-5:]
        for l in stderr_lines:
            print(f"    {l}")
        return None
    print(" done")
    return trace_path


# ============================================================================
# Table output
# ============================================================================
def fmt_val(v, prec=4):
    if v is None:
        return "N/A"
    if isinstance(v, int):
        return str(v)
    return f"{v:.{prec}f}"


def fmt_pct(measured, reference):
    if measured is None or reference is None or reference == 0:
        return "N/A"
    return f"{((measured / reference) - 1.0) * 100.0:+.2f}%"


def print_results(results):
    print()
    print("=" * 100)
    print("VALIDATION: gem5-SALAM vs Spencer et al. JSA 2024 Table 2")
    print(
        f"Accelerator frequency: {ACC_CLOCK}, SPM frequency: {ACC_SPM_CLOCK}"
    )
    print("=" * 100)
    print()

    hdr = (
        f"{'Benchmark':<12}"
        f"{'Runtime':>10} {'Paper':>10} {'Err%':>8}  "
        f"{'Power(mW)':>10} {'Paper':>10} {'Err%':>8}  "
        f"{'Area(um2)':>11} {'Paper':>11} {'Err%':>8}"
    )
    print(hdr)
    print("-" * len(hdr))

    for bench in BENCHMARKS:
        data = results.get(bench)
        ref = TABLE2.get(bench, (None, None, None))
        ref_cyc, ref_pow, ref_area = ref

        if data is None:
            print(f"{bench:<12}  {'FAILED'}")
            continue

        cyc = data["runtime_cycles"]
        if bench in CYCLE_MULTIPLIER and cyc is not None:
            cyc = cyc * CYCLE_MULTIPLIER[bench]

        pow_mw = data["core_power_mw"]
        area = data["fu_area_um2"]

        print(
            f"{bench:<12}"
            f"{fmt_val(cyc):>10} {fmt_val(ref_cyc):>10} {fmt_pct(cyc, ref_cyc):>8}  "
            f"{fmt_val(pow_mw):>10} {fmt_val(ref_pow):>10} {fmt_pct(pow_mw, ref_pow):>8}  "
            f"{fmt_val(area, 1):>11} {fmt_val(ref_area, 1):>11} {fmt_pct(area, ref_area):>8}"
        )

    print()
    print("Notes:")
    print(
        f"  - md_knn cycles = runtime_cycles * {CYCLE_MULTIPLIER['md_knn']}"
        " (DP-mul pipeline depth)"
    )
    print("  - All calibration values defined in this script (not in source)")
    print("  - Source code defaults: read_latency_cycles=0, _available=0")


# ============================================================================
# Main
# ============================================================================
def main():
    parser = argparse.ArgumentParser(
        description="Validate gem5-SALAM power/perf/area against Table 2"
    )
    parser.add_argument(
        "--m5-path", required=True, help="Path to gem5 repository root"
    )
    parser.add_argument(
        "--acc-bench-path",
        required=True,
        help="Path to benchmarks/sys_validation directory",
    )
    parser.add_argument(
        "--outdir",
        default=None,
        help="Output directory (default: <m5-path>/validation_results)",
    )
    parser.add_argument(
        "--skip-run",
        action="store_true",
        help="Skip running benchmarks; just parse existing traces",
    )
    args = parser.parse_args()

    m5_path = os.path.abspath(args.m5_path)
    acc_bench_path = os.path.abspath(args.acc_bench_path)
    outdir = args.outdir or os.path.join(m5_path, "validation_results")

    gem5_bin = os.path.join(m5_path, "build/ARM/gem5.opt")
    if not os.path.isfile(gem5_bin):
        print(f"ERROR: gem5 binary not found: {gem5_bin}", file=sys.stderr)
        print("Build with: CC=gcc CXX=g++ scons build/ARM/gem5.opt -j$(nproc)")
        sys.exit(1)

    if not args.skip_run:
        print(f"Output directory: {outdir}")
        print(f"Running {len(BENCHMARKS)} benchmarks at {ACC_CLOCK}...")
        print()

    results = {}
    for bench in BENCHMARKS:
        trace_path = os.path.join(outdir, bench, "debug-trace.txt")

        if not args.skip_run:
            trace_path = run_benchmark(bench, m5_path, acc_bench_path, outdir)
            if trace_path is None:
                results[bench] = None
                continue

        if not os.path.isfile(trace_path):
            print(f"  {bench}: no trace found at {trace_path}")
            results[bench] = None
            continue

        results[bench] = parse_kernel_metrics(trace_path, bench)

    print_results(results)


if __name__ == "__main__":
    main()
