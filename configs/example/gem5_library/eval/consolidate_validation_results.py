#!/usr/bin/env python3

"""
Consolidate integrated gem5+McPAT+HotSpot runtime thermal traces and
standalone HotSpot traces into comparison CSVs and simple validation plots.

Expected integrated layout:

    m5out_eval_large/
      saxpy/
        thermal_trace.csv
      iaxpy/
        thermal_trace.csv
      daxpy/
        thermal_trace.csv

Expected standalone HotSpot layout (matches run_standalone_hotspot.sh and
run_all_integrated_and_standalone.sh, which place converted power under ptraces/):

    configs/example/gem5_library/eval/standalone_hotspot_out/
      ptraces/saxpy.ptrace
      ptraces/iaxpy.ptrace
      ptraces/daxpy.ptrace
      saxpy.ttrace
      iaxpy.ttrace
      daxpy.ttrace

    resolve_standalone_ptrace() also accepts <standalone_root>/<app>.ptrace if
    you place files at the top level instead.

    Use cold-start standalone .ttrace output from the run scripts. Warm-started
    (_warm.ttrace) traces will not match integrated ROI cold start.

    Native HotSpot .ttrace output is often rounded (~2 decimal places); integrated
    thermal_trace.csv has higher precision, so expect small quantization error.

Integrated thermal_trace.csv format:

    tick,domain,power_w,temp_k
    1000,cpu0,0.12,300.01
    ...

Standalone .ptrace / .ttrace format:

    cpu0 l1i0 l1d0 l2
    0.12 0.01 0.01 0.10
    ...

Outputs:

    validation_comparison.csv
        One row per benchmark/domain/sample with integrated vs standalone
        power and temperature.

    validation_summary.csv
        Per benchmark/domain MAE, RMSE, max error, final temps, peak temps.

    validation_mae_barplot.png
        Grouped bar plot of mean absolute temperature error by benchmark/domain.
        This is the recommended paper-friendly validation figure.

    validation_error_boxplot_by_domain.png
        Box plot of absolute temperature error by domain.

    validation_final_temp_barplot.png
        Grouped bars comparing final integrated vs standalone temperatures.
"""

import argparse
import csv
import math
import os
from collections import defaultdict

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

DEFAULT_BENCHMARKS = ["saxpy", "iaxpy", "daxpy"]
DEFAULT_DOMAIN_ORDER = ["cpu0", "l1i0", "l1d0", "l2"]


def ensure_dir(path):
    os.makedirs(path, exist_ok=True)


def mean(values):
    return sum(values) / len(values) if values else float("nan")


def rmse(values):
    return (
        math.sqrt(sum(v * v for v in values) / len(values))
        if values
        else float("nan")
    )


def safe_float(value):
    if value is None:
        return float("nan")
    return float(value)


def read_integrated_thermal_trace(path):
    """
    Read integrated gem5 thermal_trace.csv (legacy or current schema).
    """
    by_domain = defaultdict(list)

    with open(path, newline="") as fp:
        reader = csv.DictReader(fp)
        fieldnames = set(reader.fieldnames or [])

        if "tick" not in fieldnames or "temp_k" not in fieldnames:
            raise RuntimeError(
                f"{path} does not look like integrated thermal_trace.csv. "
                f"Header was: {reader.fieldnames}"
            )

        domain_col = (
            "domain"
            if "domain" in fieldnames
            else "label" if "label" in fieldnames else None
        )
        power_col = (
            "power_w"
            if "power_w" in fieldnames
            else "total_power_w" if "total_power_w" in fieldnames else None
        )
        if domain_col is None or power_col is None:
            raise RuntimeError(
                f"{path} missing domain/label or power column. "
                f"Header was: {reader.fieldnames}"
            )

        for row in reader:
            if not row:
                continue
            if row.get("tick", "").strip() == "tick":
                continue

            domain = row[domain_col].strip()
            by_domain[domain].append(
                {
                    "tick": int(float(row["tick"])),
                    "power_w": safe_float(row[power_col]),
                    "temp_k": safe_float(row["temp_k"]),
                }
            )

    for domain, rows in by_domain.items():
        for idx, row in enumerate(rows):
            row["sample_idx"] = idx

    return dict(by_domain)


def read_hotspot_table(path):
    """
    Read standalone HotSpot .ptrace or .ttrace.

    Format:
        header: whitespace-separated domain names
        rows:   whitespace-separated numeric values

    Returns:
        dict[str, list[float]]
        domain -> values
    """
    with open(path) as fp:
        lines = [line.strip() for line in fp if line.strip()]

    if not lines:
        raise RuntimeError(f"Empty HotSpot file: {path}")

    headers = lines[0].split()
    data = {h: [] for h in headers}

    for line_no, line in enumerate(lines[1:], start=2):
        fields = line.split()

        if len(fields) != len(headers):
            raise RuntimeError(
                f"Malformed row in {path}:{line_no}. "
                f"Expected {len(headers)} values, got {len(fields)}. "
                f"Line: {line}"
            )

        for header, value in zip(headers, fields):
            data[header].append(float(value))

    return data


def ordered_domains(domains):
    domains = list(domains)
    ordered = [d for d in DEFAULT_DOMAIN_ORDER if d in domains]
    ordered += sorted(d for d in domains if d not in ordered)
    return ordered


def resolve_standalone_ptrace(standalone_root, benchmark):
    """Prefer <root>/<bench>.ptrace, else <root>/ptraces/<bench>.ptrace."""
    primary = os.path.join(standalone_root, f"{benchmark}.ptrace")
    if os.path.exists(primary):
        return primary
    alt = os.path.join(standalone_root, "ptraces", f"{benchmark}.ptrace")
    return alt


def consolidate_results(benchmarks, integrated_root, standalone_root):
    """
    Build comparison and summary rows.

    Returns:
        comparison_rows, summary_rows
    """
    comparison_rows = []
    summary_rows = []

    for benchmark in benchmarks:
        integrated_trace = os.path.join(
            integrated_root, benchmark, "thermal_trace.csv"
        )
        standalone_ptrace = resolve_standalone_ptrace(
            standalone_root, benchmark
        )
        standalone_ttrace = os.path.join(
            standalone_root, f"{benchmark}.ttrace"
        )

        if not os.path.exists(integrated_trace):
            raise FileNotFoundError(
                f"Missing integrated trace: {integrated_trace}"
            )

        if not os.path.exists(standalone_ptrace):
            raise FileNotFoundError(
                f"Missing standalone power trace: {standalone_ptrace}"
            )

        if not os.path.exists(standalone_ttrace):
            raise FileNotFoundError(
                f"Missing standalone temperature trace: {standalone_ttrace}"
            )

        integrated = read_integrated_thermal_trace(integrated_trace)
        standalone_power = read_hotspot_table(standalone_ptrace)
        standalone_temp = read_hotspot_table(standalone_ttrace)

        common_domains = (
            set(integrated.keys())
            & set(standalone_power.keys())
            & set(standalone_temp.keys())
        )

        if not common_domains:
            raise RuntimeError(
                f"No common domains for benchmark={benchmark}. "
                f"Integrated domains={sorted(integrated.keys())}; "
                f"standalone ptrace domains={sorted(standalone_power.keys())}; "
                f"standalone ttrace domains={sorted(standalone_temp.keys())}"
            )

        for domain in ordered_domains(common_domains):
            int_rows = integrated[domain]
            st_power_values = standalone_power[domain]
            st_temp_values = standalone_temp[domain]

            n = min(len(int_rows), len(st_power_values), len(st_temp_values))

            if n == 0:
                continue

            temp_errors = []
            power_errors = []
            int_temps = []
            st_temps = []
            int_powers = []
            st_powers = []

            for sample_idx in range(n):
                int_row = int_rows[sample_idx]

                int_power = int_row["power_w"]
                st_power = st_power_values[sample_idx]

                int_temp = int_row["temp_k"]
                st_temp = st_temp_values[sample_idx]

                power_error = int_power - st_power
                temp_error = int_temp - st_temp

                comparison_rows.append(
                    {
                        "benchmark": benchmark,
                        "domain": domain,
                        "sample_idx": sample_idx,
                        "integrated_tick": int_row["tick"],
                        "integrated_power_w": int_power,
                        "standalone_power_w": st_power,
                        "power_error_w": power_error,
                        "power_abs_error_w": abs(power_error),
                        "integrated_temp_k": int_temp,
                        "standalone_temp_k": st_temp,
                        "temp_error_k": temp_error,
                        "temp_abs_error_k": abs(temp_error),
                    }
                )

                temp_errors.append(temp_error)
                power_errors.append(power_error)
                int_temps.append(int_temp)
                st_temps.append(st_temp)
                int_powers.append(int_power)
                st_powers.append(st_power)

            summary_rows.append(
                {
                    "benchmark": benchmark,
                    "domain": domain,
                    "n_integrated": len(int_rows),
                    "n_standalone_power": len(st_power_values),
                    "n_standalone_temp": len(st_temp_values),
                    "n_compared": n,
                    "mean_integrated_power_w": mean(int_powers),
                    "mean_standalone_power_w": mean(st_powers),
                    "mean_integrated_temp_k": mean(int_temps),
                    "mean_standalone_temp_k": mean(st_temps),
                    "final_integrated_temp_k": int_temps[-1],
                    "final_standalone_temp_k": st_temps[-1],
                    "final_temp_error_k": int_temps[-1] - st_temps[-1],
                    "final_temp_abs_error_k": abs(
                        int_temps[-1] - st_temps[-1]
                    ),
                    "peak_integrated_temp_k": max(int_temps),
                    "peak_standalone_temp_k": max(st_temps),
                    "peak_temp_error_k": max(int_temps) - max(st_temps),
                    "peak_temp_abs_error_k": abs(
                        max(int_temps) - max(st_temps)
                    ),
                    "mae_temp_k": mean([abs(v) for v in temp_errors]),
                    "rmse_temp_k": rmse(temp_errors),
                    "max_abs_temp_error_k": max(abs(v) for v in temp_errors),
                    "mae_power_w": mean([abs(v) for v in power_errors]),
                    "rmse_power_w": rmse(power_errors),
                    "max_abs_power_error_w": max(abs(v) for v in power_errors),
                }
            )

    all_temp_errors = [row["temp_error_k"] for row in comparison_rows]
    all_power_errors = [row["power_error_w"] for row in comparison_rows]
    all_int_temps = [row["integrated_temp_k"] for row in comparison_rows]
    all_st_temps = [row["standalone_temp_k"] for row in comparison_rows]

    if comparison_rows:
        summary_rows.append(
            {
                "benchmark": "ALL",
                "domain": "ALL",
                "n_integrated": "",
                "n_standalone_power": "",
                "n_standalone_temp": "",
                "n_compared": len(comparison_rows),
                "mean_integrated_power_w": "",
                "mean_standalone_power_w": "",
                "mean_integrated_temp_k": mean(all_int_temps),
                "mean_standalone_temp_k": mean(all_st_temps),
                "final_integrated_temp_k": "",
                "final_standalone_temp_k": "",
                "final_temp_error_k": "",
                "final_temp_abs_error_k": "",
                "peak_integrated_temp_k": max(all_int_temps),
                "peak_standalone_temp_k": max(all_st_temps),
                "peak_temp_error_k": max(all_int_temps) - max(all_st_temps),
                "peak_temp_abs_error_k": abs(
                    max(all_int_temps) - max(all_st_temps)
                ),
                "mae_temp_k": mean([abs(v) for v in all_temp_errors]),
                "rmse_temp_k": rmse(all_temp_errors),
                "max_abs_temp_error_k": max(abs(v) for v in all_temp_errors),
                "mae_power_w": mean([abs(v) for v in all_power_errors]),
                "rmse_power_w": rmse(all_power_errors),
                "max_abs_power_error_w": max(abs(v) for v in all_power_errors),
            }
        )

    return comparison_rows, summary_rows


def write_csv(path, rows, fieldnames):
    with open(path, "w", newline="") as fp:
        writer = csv.DictWriter(fp, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def plot_mae_barplot(summary_rows, benchmarks, out_path):
    """
    Grouped bar plot:
        x-axis: benchmark
        bars: domains
        y-axis: MAE temperature error in K
    """
    rows = [
        r
        for r in summary_rows
        if r["benchmark"] != "ALL" and r["domain"] != "ALL"
    ]

    domains = ordered_domains({r["domain"] for r in rows})
    metric_by_pair = {
        (r["benchmark"], r["domain"]): float(r["mae_temp_k"]) for r in rows
    }

    x_positions = list(range(len(benchmarks)))
    n_domains = len(domains)
    group_width = 0.8
    bar_width = group_width / max(n_domains, 1)

    plt.figure(figsize=(8.5, 4.8))

    for domain_idx, domain in enumerate(domains):
        offsets = [
            x - group_width / 2 + bar_width / 2 + domain_idx * bar_width
            for x in x_positions
        ]
        values = [
            metric_by_pair.get((benchmark, domain), float("nan"))
            for benchmark in benchmarks
        ]
        plt.bar(offsets, values, width=bar_width, label=domain)

    plt.xticks(x_positions, benchmarks)
    plt.ylabel("Mean absolute temperature error (K)")
    plt.xlabel("Benchmark")
    plt.title("Integrated runtime thermal model vs. standalone HotSpot")
    plt.legend(title="Domain")
    plt.grid(axis="y", alpha=0.3)
    plt.tight_layout()
    plt.savefig(out_path, dpi=300)
    plt.close()


def plot_error_boxplot_by_domain(comparison_rows, out_path):
    """
    Box plot:
        x-axis: domain
        y-axis: absolute temperature error in K
    """
    errors_by_domain = defaultdict(list)

    for row in comparison_rows:
        errors_by_domain[row["domain"]].append(float(row["temp_abs_error_k"]))

    domains = ordered_domains(errors_by_domain.keys())
    data = [errors_by_domain[d] for d in domains]

    plt.figure(figsize=(7.5, 4.8))
    plt.boxplot(data, labels=domains, showfliers=True)
    plt.ylabel("Absolute temperature error (K)")
    plt.xlabel("Domain")
    plt.title("Distribution of integrated vs. standalone HotSpot error")
    plt.grid(axis="y", alpha=0.3)
    plt.tight_layout()
    plt.savefig(out_path, dpi=300)
    plt.close()


def plot_final_temp_barplot(summary_rows, benchmarks, out_path):
    """
    Bar plot of final temperatures:
        x-axis: benchmark-domain pairs
        paired bars: integrated and standalone final temperature
    """
    rows = [
        r
        for r in summary_rows
        if r["benchmark"] != "ALL" and r["domain"] != "ALL"
    ]

    domains = ordered_domains({r["domain"] for r in rows})

    ordered_pairs = []
    for benchmark in benchmarks:
        for domain in domains:
            for row in rows:
                if row["benchmark"] == benchmark and row["domain"] == domain:
                    ordered_pairs.append(row)
                    break

    labels = [f"{r['benchmark']}\n{r['domain']}" for r in ordered_pairs]
    integrated = [float(r["final_integrated_temp_k"]) for r in ordered_pairs]
    standalone = [float(r["final_standalone_temp_k"]) for r in ordered_pairs]

    x_positions = list(range(len(labels)))
    width = 0.38

    plt.figure(figsize=(11.0, 4.8))

    plt.bar(
        [x - width / 2 for x in x_positions],
        integrated,
        width=width,
        label="Integrated",
    )
    plt.bar(
        [x + width / 2 for x in x_positions],
        standalone,
        width=width,
        label="Standalone HotSpot",
    )

    all_values = integrated + standalone
    y_min = min(all_values)
    y_max = max(all_values)
    padding = max(0.01, 0.1 * (y_max - y_min))

    plt.ylim(y_min - padding, y_max + padding)
    plt.xticks(x_positions, labels)
    plt.ylabel("Final temperature (K)")
    plt.xlabel("Benchmark / domain")
    plt.title("Final temperature: integrated runtime vs. standalone HotSpot")
    plt.legend()
    plt.grid(axis="y", alpha=0.3)
    plt.tight_layout()
    plt.savefig(out_path, dpi=300)
    plt.close()


def print_summary_to_stdout(summary_rows):
    print("\nValidation summary:")
    print(
        f"{'benchmark':<12} {'domain':<8} "
        f"{'MAE(K)':>12} {'RMSE(K)':>12} {'MaxErr(K)':>12} "
        f"{'FinalErr(K)':>14}"
    )
    print("-" * 76)

    for row in summary_rows:
        benchmark = row["benchmark"]
        domain = row["domain"]
        mae = row["mae_temp_k"]
        rmse_value = row["rmse_temp_k"]
        max_err = row["max_abs_temp_error_k"]
        final_err = row["final_temp_abs_error_k"]

        def fmt(v):
            if v == "":
                return ""
            return f"{float(v):.6f}"

        print(
            f"{benchmark:<12} {domain:<8} "
            f"{fmt(mae):>12} {fmt(rmse_value):>12} {fmt(max_err):>12} "
            f"{fmt(final_err):>14}"
        )


def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--integrated-root",
        default="m5out_eval_large",
        help=(
            "Root containing integrated benchmark outputs. "
            "Expected: <root>/<benchmark>/thermal_trace.csv"
        ),
    )

    parser.add_argument(
        "--standalone-root",
        default="configs/example/gem5_library/eval/standalone_hotspot_out",
        help=(
            "Directory containing standalone HotSpot outputs. "
            "Per-bench .ttrace at <root>/<benchmark>.ttrace; .ptrace from "
            "run_all_integrated_and_standalone.sh / run_standalone_hotspot.sh at "
            "<root>/ptraces/<benchmark>.ptrace, or <root>/<benchmark>.ptrace"
        ),
    )

    parser.add_argument(
        "--outdir",
        default="configs/example/gem5_library/eval/validation_summary",
        help="Output directory for CSVs and plots.",
    )

    parser.add_argument(
        "--benchmarks",
        nargs="+",
        default=DEFAULT_BENCHMARKS,
        help="Benchmarks to process.",
    )

    return parser.parse_args()


def main():
    args = parse_args()

    ensure_dir(args.outdir)

    comparison_rows, summary_rows = consolidate_results(
        benchmarks=args.benchmarks,
        integrated_root=args.integrated_root,
        standalone_root=args.standalone_root,
    )

    comparison_csv = os.path.join(args.outdir, "validation_comparison.csv")
    summary_csv = os.path.join(args.outdir, "validation_summary.csv")
    mae_plot = os.path.join(args.outdir, "validation_mae_barplot.png")
    box_plot = os.path.join(
        args.outdir, "validation_error_boxplot_by_domain.png"
    )
    final_plot = os.path.join(args.outdir, "validation_final_temp_barplot.png")

    comparison_fields = [
        "benchmark",
        "domain",
        "sample_idx",
        "integrated_tick",
        "integrated_power_w",
        "standalone_power_w",
        "power_error_w",
        "power_abs_error_w",
        "integrated_temp_k",
        "standalone_temp_k",
        "temp_error_k",
        "temp_abs_error_k",
    ]

    summary_fields = [
        "benchmark",
        "domain",
        "n_integrated",
        "n_standalone_power",
        "n_standalone_temp",
        "n_compared",
        "mean_integrated_power_w",
        "mean_standalone_power_w",
        "mean_integrated_temp_k",
        "mean_standalone_temp_k",
        "final_integrated_temp_k",
        "final_standalone_temp_k",
        "final_temp_error_k",
        "final_temp_abs_error_k",
        "peak_integrated_temp_k",
        "peak_standalone_temp_k",
        "peak_temp_error_k",
        "peak_temp_abs_error_k",
        "mae_temp_k",
        "rmse_temp_k",
        "max_abs_temp_error_k",
        "mae_power_w",
        "rmse_power_w",
        "max_abs_power_error_w",
    ]

    write_csv(comparison_csv, comparison_rows, comparison_fields)
    write_csv(summary_csv, summary_rows, summary_fields)

    plot_mae_barplot(summary_rows, args.benchmarks, mae_plot)
    plot_error_boxplot_by_domain(comparison_rows, box_plot)
    plot_final_temp_barplot(summary_rows, args.benchmarks, final_plot)

    print_summary_to_stdout(summary_rows)

    print("\nWrote:")
    print(f"  {comparison_csv}")
    print(f"  {summary_csv}")
    print(f"  {mae_plot}")
    print(f"  {box_plot}")
    print(f"  {final_plot}")


if __name__ == "__main__":
    main()
