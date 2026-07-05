#!/usr/bin/env python3
"""
Convert gem5 power_trace.csv into a HotSpot .ptrace.

Accepts legacy (tick,obj,dyn_w,st_w,total_w) or current
(tick,label,dynamic_power_w,static_power_w,total_power_w,...) schemas.
"""

import argparse
import csv
from collections import OrderedDict

ORDER = ["cpu0", "l1i0", "l1d0", "l2"]


def map_obj_name(obj: str):
    s = obj.strip()
    if s in ORDER:
        return s
    if s == "board.processor.cores.core":
        return "cpu0"
    if s == "board.cache_hierarchy.l1icaches":
        return "l1i0"
    if s == "board.cache_hierarchy.l1dcaches":
        return "l1d0"
    if s == "board.cache_hierarchy.l2cache":
        return "l2"
    return None


def resolve_power_columns(fieldnames):
    names = set(fieldnames or [])
    if "tick" not in names:
        raise RuntimeError("Missing required column: tick")
    obj_col = (
        "label" if "label" in names else "obj" if "obj" in names else None
    )
    if obj_col is None:
        raise RuntimeError(f"Missing domain column. Header: {fieldnames}")
    resolved = {}
    for canonical, aliases in [
        ("dyn_w", ("dyn_w", "dynamic_power_w")),
        ("st_w", ("st_w", "static_power_w")),
        ("total_w", ("total_w", "total_power_w")),
    ]:
        for alias in aliases:
            if alias in names:
                resolved[canonical] = alias
                break
        else:
            raise RuntimeError(f"Missing power column for {canonical}")
    return obj_col, resolved


def parse_args():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--output", required=True)
    ap.add_argument(
        "--power-field",
        default="total_w",
        choices=["dyn_w", "st_w", "total_w"],
    )
    ap.add_argument("--drop-final-tail", action="store_true")
    ap.add_argument("--print-summary", action="store_true")
    return ap.parse_args()


def is_complete_row(rowdict):
    return all(name in rowdict for name in ORDER)


def detect_tail_tick(ticks):
    if len(ticks) < 3:
        return None
    deltas = [ticks[i] - ticks[i - 1] for i in range(1, len(ticks))]
    prev_delta = deltas[-2]
    last_delta = deltas[-1]
    if prev_delta > 0 and last_delta > 0 and last_delta < prev_delta:
        return ticks[-1]
    return None


def main():
    args = parse_args()
    by_tick = OrderedDict()

    with open(args.input, newline="") as fp:
        reader = csv.DictReader(fp)
        obj_col, power_cols = resolve_power_columns(reader.fieldnames)
        power_col = power_cols[args.power_field]
        for row in reader:
            tick = int(row["tick"])
            blk = map_obj_name(row[obj_col])
            if blk is None:
                continue
            power = float(row[power_col])
            if tick not in by_tick:
                by_tick[tick] = {}
            by_tick[tick][blk] = power

    ticks = [t for t, rowdict in by_tick.items() if is_complete_row(rowdict)]
    if not ticks:
        raise RuntimeError("No complete samples found.")

    tail_tick = detect_tail_tick(ticks)
    if args.drop_final_tail and tail_tick is not None:
        ticks = ticks[:-1]

    with open(args.output, "w", newline="") as fp:
        fp.write("\t".join(ORDER) + "\n")
        for tick in ticks:
            row = by_tick[tick]
            fp.write("\t".join(f"{row[name]:.12g}" for name in ORDER) + "\n")

    if args.print_summary:
        print(f"samples kept: {len(ticks)}")


if __name__ == "__main__":
    main()
