#!/usr/bin/env python3
"""Compare power_trace.csv and thermal_trace.csv between two gem5 outdirs."""

import argparse
import csv
from collections import defaultdict
from pathlib import Path


def load_trace(path: Path):
    rows = defaultdict(list)
    with open(path) as f:
        for row in csv.DictReader(f):
            rows[row["label"]].append(
                {
                    "tick": int(row["tick"]),
                    "dyn": float(row["dynamic_power_w"]),
                    "st": float(row["static_power_w"]),
                    "total": float(row["total_power_w"]),
                    "temp_k": float(row["temp_k"]),
                }
            )
    return rows


def rel_err(x, y):
    if abs(x) < 1e-15 and abs(y) < 1e-15:
        return 0.0
    return abs(x - y) / max(abs(x), abs(y), 1e-15) * 100.0


def summarize(label, a, b, skip_first=False, skip_last=False):
    if skip_first:
        a, b = a[1:], b[1:]
    if skip_last and len(a) > 1 and len(b) > 1:
        a, b = a[:-1], b[:-1]
    n = min(len(a), len(b))
    if n == 0:
        return None
    a, b = a[:n], b[:n]
    out = {"label": label, "n": n}
    for field, name in [
        ("dyn", "dynamic_W"),
        ("st", "static_W"),
        ("total", "total_W"),
        ("temp_k", "temp_K"),
    ]:
        diffs = [a[i][field] - b[i][field] for i in range(n)]
        rels = [rel_err(a[i][field], b[i][field]) for i in range(n)]
        out[name] = {
            "mean_abs_diff": sum(abs(d) for d in diffs) / n,
            "max_abs_diff": max(abs(d) for d in diffs),
            "mean_rel_pct": sum(rels) / n,
            "max_rel_pct": max(rels),
            "a_last": a[-1][field],
            "b_last": b[-1][field],
        }
    return out


def main():
    parser = argparse.ArgumentParser(
        description="Compare gem5 power/thermal trace CSVs"
    )
    parser.add_argument("dir_a", type=Path, help="First m5out directory")
    parser.add_argument("dir_b", type=Path, help="Second m5out directory")
    parser.add_argument(
        "--label-a", default="gem5", help="Label for first directory"
    )
    parser.add_argument(
        "--label-b", default="gem5-pm", help="Label for second directory"
    )
    parser.add_argument(
        "--skip-first",
        action="store_true",
        help="Skip first interval (partial ROI window)",
    )
    parser.add_argument(
        "--skip-last",
        action="store_true",
        help="Skip last interval (ROI flush sample)",
    )
    args = parser.parse_args()

    skips = []
    if args.skip_first:
        skips.append("first")
    if args.skip_last:
        skips.append("last")
    skip_note = (
        f"(skipping {' and '.join(skips)} sample per label)" if skips else ""
    )

    for trace in ("power", "thermal"):
        a = load_trace(args.dir_a / f"{trace}_trace.csv")
        b = load_trace(args.dir_b / f"{trace}_trace.csv")
        print(
            f"\n=== {trace.upper()} TRACE: {args.label_a} vs {args.label_b} ==="
        )
        if skip_note:
            print(skip_note)
        labels = sorted(set(a) | set(b))
        for lab in labels:
            if lab not in a or lab not in b:
                print(f"  {lab}: missing in one side")
                continue
            s = summarize(
                lab,
                a[lab],
                b[lab],
                skip_first=args.skip_first,
                skip_last=args.skip_last,
            )
            print(f"\n  {lab} ({s['n']} aligned samples)")
            for key in ("dynamic_W", "static_W", "total_W", "temp_K"):
                m = s[key]
                print(
                    f"    {key:10s} mean|diff|={m['mean_abs_diff']:.6g}W "
                    f"max|diff|={m['max_abs_diff']:.6g}W "
                    f"mean_rel={m['mean_rel_pct']:.3f}% max_rel={m['max_rel_pct']:.3f}%"
                )
                print(
                    f"               last {args.label_a}={m['a_last']:.6g} "
                    f"{args.label_b}={m['b_last']:.6g}"
                )


if __name__ == "__main__":
    main()
