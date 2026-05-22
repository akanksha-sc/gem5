#!/usr/bin/env python3
# Copyright (c) 2025 Akanksha Chaudhari, Matt Sinclair
# All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause

"""Compare SALAM_SUMMARY lines between legacy and stdlib debug traces."""

import argparse
import sys
from collections import Counter
from pathlib import Path

SUMMARY_PREFIX = "SALAM_SUMMARY "

EXACT_KEYS = [
    "inst_issue",
    "inst_commit",
    "load_issue",
    "load_commit",
    "store_issue",
    "store_commit",
    "call_issue",
    "call_commit",
]


def as_int(d, key, default=0):
    v = d.get(key, default)
    try:
        return int(v)
    except (TypeError, ValueError):
        try:
            return int(float(v))
        except (TypeError, ValueError):
            return default


def parse_summary_line(line: str):
    if not line.startswith(SUMMARY_PREFIX):
        return None
    payload = line[len(SUMMARY_PREFIX) :].strip()
    fields = {}
    for item in payload.split():
        if "=" not in item:
            continue
        k, v = item.split("=", 1)
        fields[k] = v
    return fields


def load_summaries(path: Path):
    text = path.read_text()
    out = []
    for line in text.splitlines():
        p = parse_summary_line(line)
        if p is not None:
            out.append(p)
    return out


def within_tolerance(a: int, b: int, tol: float) -> bool:
    if a == 0 and b == 0:
        return True
    ref = max(abs(a), abs(b), 1)
    return abs(a - b) / ref <= tol


def check_invariants(label: str, s: dict, errors: list) -> None:
    name = s.get("name", "<unknown>")
    inst_issue = as_int(s, "inst_issue")
    inst_commit = as_int(s, "inst_commit")
    load_issue = as_int(s, "load_issue")
    load_commit = as_int(s, "load_commit")
    store_issue = as_int(s, "store_issue")
    store_commit = as_int(s, "store_commit")
    cmp_try = as_int(s, "cmp_try")
    cmp_launch = as_int(s, "cmp_launch")
    cmp_commit = as_int(s, "cmp_commit")
    call_issue = as_int(s, "call_issue")
    call_commit = as_int(s, "call_commit")

    def req(cond, msg):
        if not cond:
            errors.append(f"{label} {name}: {msg}")

    req(as_int(s, "runtime_cycles") > 0, "runtime_cycles must be > 0")
    req(inst_issue >= inst_commit, "inst_issue < inst_commit")
    req(load_issue >= load_commit, "load_issue < load_commit")
    req(store_issue >= store_commit, "store_issue < store_commit")
    req(cmp_try >= cmp_launch, "cmp_try < cmp_launch")
    req(cmp_launch >= cmp_commit, "cmp_launch < cmp_commit")
    req(call_issue >= call_commit, "call_issue < call_commit")


def main():
    ap = argparse.ArgumentParser(
        description=(
            "Compare SALAM_SUMMARY records in two debug-trace.txt files. "
            "Prefer --legacy/--stdlib; positional arguments are a fallback "
            "for older invocations."
        )
    )
    ap.add_argument(
        "--legacy",
        type=Path,
        default=None,
        help="Path to legacy BM_ARM_OUT/.../debug-trace.txt",
    )
    ap.add_argument(
        "--stdlib",
        type=Path,
        default=None,
        help="Path to stdlib BM_ARM_OUT/.../debug-trace.txt",
    )
    ap.add_argument(
        "legacy_trace",
        nargs="?",
        type=Path,
        default=None,
        help="(Deprecated) same as --legacy",
    )
    ap.add_argument(
        "stdlib_trace",
        nargs="?",
        type=Path,
        default=None,
        help="(Deprecated) same as --stdlib",
    )
    ap.add_argument(
        "--runtime-tol",
        type=float,
        default=0.05,
        help="Relative tolerance for runtime_cycles (default 0.05)",
    )
    ap.add_argument(
        "--compute-tol",
        type=float,
        default=0.05,
        help="Relative tolerance for useful_compute (default 0.05)",
    )
    ap.add_argument(
        "--memory-tol",
        type=float,
        default=0.10,
        help="Relative tolerance for useful_memory (default 0.10)",
    )
    args = ap.parse_args()

    legacy_p = args.legacy or args.legacy_trace
    stdlib_p = args.stdlib or args.stdlib_trace
    if legacy_p is None or stdlib_p is None:
        ap.error(
            "Provide both traces via --legacy and --stdlib, or two "
            "positional paths (legacy first, then stdlib)."
        )

    tolerance_keys = {
        "runtime_cycles": args.runtime_tol,
        "useful_compute": args.compute_tol,
        "useful_memory": args.memory_tol,
    }

    errors = []
    for p in (legacy_p, stdlib_p):
        if not p.is_file():
            errors.append(f"not a file: {p}")

    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1

    leg = load_summaries(legacy_p)
    std = load_summaries(stdlib_p)

    names_leg = Counter(s.get("name", "") for s in leg)
    names_std = Counter(s.get("name", "") for s in std)
    if names_leg != names_std:
        errors.append(
            f"accelerator name counts differ: legacy={dict(names_leg)} "
            f"stdlib={dict(names_std)}"
        )

    if len(leg) != len(std):
        errors.append(
            f"summary count differs: legacy={len(leg)} stdlib={len(std)}"
        )

    leg_s = sorted(leg, key=lambda x: x.get("name", ""))
    std_s = sorted(std, key=lambda x: x.get("name", ""))

    for s in leg_s:
        check_invariants("legacy", s, errors)
    for s in std_s:
        check_invariants("stdlib", s, errors)

    for la, sb in zip(leg_s, std_s):
        na = la.get("name", "")
        nb = sb.get("name", "")
        if na != nb:
            errors.append(f"name order mismatch: {na!r} vs {nb!r}")
            continue
        for k in EXACT_KEYS:
            va, vb = as_int(la, k), as_int(sb, k)
            if va != vb:
                errors.append(f"{na}: {k} legacy={va} stdlib={vb} (exact)")
        for k, tol in tolerance_keys.items():
            va, vb = as_int(la, k), as_int(sb, k)
            if not within_tolerance(va, vb, tol):
                errors.append(
                    f"{na}: {k} legacy={va} stdlib={vb} "
                    f"(>{tol * 100:.0f}% rel diff)"
                )

    if errors:
        print("compare_salam_summary: FAIL", file=sys.stderr)
        for e in errors:
            print(e, file=sys.stderr)
        return 1

    print("compare_salam_summary: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
