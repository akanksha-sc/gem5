#!/usr/bin/env python3
import sys
from pathlib import Path

SUMMARY_PREFIX = "SALAM_SUMMARY "


def as_int(d, key, default=0):
    try:
        return int(d.get(key, default))
    except ValueError:
        return int(float(d.get(key, default)))


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


class TestRunner:
    def __init__(self, log_path: Path, test_log_path: Path):
        self.log_path = log_path
        self.test_log_path = test_log_path
        self.total = 0
        self.passed = 0
        self.lines = []

    def record(self, name: str, cond: bool, detail: str = ""):
        self.total += 1
        if cond:
            self.passed += 1
            self.lines.append(f"PASS | {name}")
        else:
            suffix = f" | {detail}" if detail else ""
            self.lines.append(f"FAIL | {name}{suffix}")

    def finish(self):
        self.test_log_path.write_text("\n".join(self.lines) + "\n")
        failed = self.total - self.passed
        if failed == 0:
            print(f"ALL tests passed ({self.passed}/{self.total}).")
            return 0
        print(
            f"{self.passed}/{self.total} tests passed, "
            "{failed} failed -- see {self.test_log_path} for details"
        )
        return 1


def check_summary(tr: TestRunner, s: dict):
    name = s.get("name", "<unknown>")

    runtime = as_int(s, "runtime_cycles")
    tr.record(
        f"{name}: runtime_cycles > 0", runtime > 0, f"runtime_cycles={runtime}"
    )

    disjoint = (
        as_int(s, "useful_compute")
        + as_int(s, "useful_memory")
        + as_int(s, "useful_control")
        + as_int(s, "dep_stall")
        + as_int(s, "fu_stall")
        + as_int(s, "cmp_wait")
        + as_int(s, "mem_wait")
        + as_int(s, "mem_bp_wait")
        + as_int(s, "both_outstanding_wait")
        + as_int(s, "sched_blocked")
        + as_int(s, "idle")
    )
    tr.record(
        f"{name}: disjoint sum == runtime",
        disjoint == runtime,
        f"disjoint={disjoint}, runtime={runtime}",
    )

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

    tr.record(
        f"{name}: inst_issue >= inst_commit",
        inst_issue >= inst_commit,
        f"{inst_issue} < {inst_commit}",
    )
    tr.record(
        f"{name}: load_issue >= load_commit",
        load_issue >= load_commit,
        f"{load_issue} < {load_commit}",
    )
    tr.record(
        f"{name}: store_issue >= store_commit",
        store_issue >= store_commit,
        f"{store_issue} < {store_commit}",
    )
    tr.record(
        f"{name}: cmp_try >= cmp_launch",
        cmp_try >= cmp_launch,
        f"{cmp_try} < {cmp_launch}",
    )
    tr.record(
        f"{name}: cmp_launch >= cmp_commit",
        cmp_launch >= cmp_commit,
        f"{cmp_launch} < {cmp_commit}",
    )
    tr.record(
        f"{name}: call_issue >= call_commit",
        call_issue >= call_commit,
        f"{call_issue} < {call_commit}",
    )

    cycle_records = as_int(s, "cycle_records")
    if cycle_records > 0:
        tr.record(
            f"{name}: cycle_load_issue matches",
            as_int(s, "cycle_load_issue") == load_issue,
            f"{as_int(s, 'cycle_load_issue')} != {load_issue}",
        )
        tr.record(
            f"{name}: cycle_load_commit matches",
            as_int(s, "cycle_load_commit") == load_commit,
            f"{as_int(s, 'cycle_load_commit')} != {load_commit}",
        )
        tr.record(
            f"{name}: cycle_store_issue matches",
            as_int(s, "cycle_store_issue") == store_issue,
            f"{as_int(s, 'cycle_store_issue')} != {store_issue}",
        )
        tr.record(
            f"{name}: cycle_store_commit matches",
            as_int(s, "cycle_store_commit") == store_commit,
            f"{as_int(s, 'cycle_store_commit')} != {store_commit}",
        )
        tr.record(
            f"{name}: cycle_cmp_try matches",
            as_int(s, "cycle_cmp_try") == cmp_try,
            f"{as_int(s, 'cycle_cmp_try')} != {cmp_try}",
        )
        tr.record(
            f"{name}: cycle_cmp_launch matches",
            as_int(s, "cycle_cmp_launch") == cmp_launch,
            f"{as_int(s, 'cycle_cmp_launch')} != {cmp_launch}",
        )
        tr.record(
            f"{name}: cycle_cmp_commit matches",
            as_int(s, "cycle_cmp_commit") == cmp_commit,
            f"{as_int(s, 'cycle_cmp_commit')} != {cmp_commit}",
        )
        tr.record(
            f"{name}: cycle_call_issue matches",
            as_int(s, "cycle_call_issue") == call_issue,
            f"{as_int(s, 'cycle_call_issue')} != {call_issue}",
        )
        tr.record(
            f"{name}: cycle_call_commit matches",
            as_int(s, "cycle_call_commit") == call_commit,
            f"{as_int(s, 'cycle_call_commit')} != {call_commit}",
        )
        tr.record(
            f"{name}: internal loads <= load commits",
            as_int(s, "cycle_internal_load_completions")
            <= as_int(s, "cycle_load_commit"),
            f"{as_int(s, 'cycle_internal_load_completions')} > "
            f"{as_int(s, 'cycle_load_commit')}",
        )

        if "internal_load_commit" in s and "external_load_commit" in s:
            tr.record(
                f"{name}: internal+external load commit matches",
                as_int(s, "internal_load_commit")
                + as_int(s, "external_load_commit")
                == load_commit,
                f"{as_int(s, 'internal_load_commit')} + "
                f"{as_int(s, 'external_load_commit')} != {load_commit}",
            )

    if "unissued_mem_req" in s:
        tr.record(
            f"{name}: unissued_mem_req <= mem_bp_wait",
            as_int(s, "unissued_mem_req") <= as_int(s, "mem_bp_wait"),
            f"{as_int(s, 'unissued_mem_req')} > {as_int(s, 'mem_bp_wait')}",
        )

    tr.record(
        f"{name}: lockstep_wait implies sched_blocked",
        not (
            as_int(s, "lockstep_wait") > 0 and as_int(s, "sched_blocked") == 0
        ),
    )
    tr.record(
        f"{name}: call_wait implies sched_blocked",
        not (as_int(s, "call_wait") > 0 and as_int(s, "sched_blocked") == 0),
    )
    tr.record(
        f"{name}: threshold_blocked implies sched_blocked",
        not (
            as_int(s, "threshold_blocked") > 0
            and as_int(s, "sched_blocked") == 0
        ),
    )
    tr.record(
        f"{name}: backpressure subtype implies mem_bp_wait",
        not (
            (as_int(s, "all_ports_stalled") > 0 or as_int(s, "port_retry") > 0)
            and as_int(s, "mem_bp_wait") == 0
        ),
    )
    tr.record(
        f"{name}: debug_stalls <= runtime",
        as_int(s, "debug_stalls") <= runtime,
        f"debug_stalls={as_int(s, 'debug_stalls')} runtime={runtime}",
    )


def main():
    if len(sys.argv) != 2:
        print("usage: test_metrics.py <outdir>", file=sys.stderr)
        sys.exit(1)

    outdir = Path(sys.argv[1])
    log_path = outdir / "debug-trace.txt"
    test_log_path = outdir / "test.log"

    if not log_path.is_file():
        print(f"error: log not found: {log_path}", file=sys.stderr)
        sys.exit(1)

    text = log_path.read_text()
    summaries = []
    for line in text.splitlines():
        parsed = parse_summary_line(line)
        if parsed is not None:
            summaries.append(parsed)

    tr = TestRunner(log_path, test_log_path)

    tr.record(
        "SALAM_SUMMARY lines found",
        len(summaries) > 0,
        f"found={len(summaries)}",
    )
    tr.record(
        "Contains Disjoint Cycle Breakdown", "Disjoint Cycle Breakdown" in text
    )
    tr.record("No legacy Stage 2 text", "Stage 2" not in text)
    tr.record("No Productive Cycles text", "Productive Cycles" not in text)
    tr.record(
        "No legacy Total Active Load Cycles text",
        "Total Active Load Cycles" not in text,
    )
    tr.record(
        "No legacy Total Active Store Cycles text",
        "Total Active Store Cycles" not in text,
    )
    tr.record(
        "No legacy Total Active Compute Cycles text",
        "Total Active Compute Cycles" not in text,
    )

    for s in summaries:
        check_summary(tr, s)

    sys.exit(tr.finish())


if __name__ == "__main__":
    main()
