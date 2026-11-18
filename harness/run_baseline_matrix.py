#!/usr/bin/env python3
"""Run the baseline NUMA reproduction config matrix (100 runs) and write results/baseline_figure7/matrix.csv.

Evaluates 5 benchmarks x 4 baseline policies x 5 repetitions = 100 runs.
Canonical baseline policies:
  - ws+coarse: Work-stealing with coarse-grain page placement
  - ws+fine:   Work-stealing with fine-grain page placement
  - la+coarse: Locality-aware scheduling with coarse placement
  - la+fine:   Locality-aware scheduling with fine placement

Validates every output row for check_ok == 1 and non-zero cycles.
Rejects any corrupted, malformed, or timed-out run.
"""
import csv
import os
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BUILD = REPO / "build"
RESULTS = REPO / "results" / "baseline_figure7"
RESULTS.mkdir(parents=True, exist_ok=True)

BENCHMARKS = ["map", "matmul", "reduction", "jacobi", "sparselu"]
# Exactly 4 canonical baseline configs from Figure 7
CONFIGS = [
    ("ws", "ws-de-node", "coarse", "coarse"),
    ("ws", "ws-de-node", "fine", "fine"),
    ("la", "numa", "coarse", "coarse"),
    ("la", "numa", "fine", "fine"),
]
REPS = 5
WORKERS = 24

FIELDS = [
    "benchmark", "scheduler_policy", "dist_policy", "topology", "run_id",
    "exec_cycles", "comm_cost_total", "comm_tasks_total", "comm_cost_avg",
    "tasks_stolen", "tasks_owned", "check_ok"
]


def run_one(bench, sched_label, sched_flag, dist_label, dist_flag, run_id):
    binary = BUILD / f"bench_{bench}"
    if not binary.exists():
        print(f"ERROR: Benchmark binary {binary} not found. Build MIR benchmarks first.", file=sys.stderr)
        return None

    env = os.environ.copy()
    env["MIR_ARCH_NAME"] = "sim_numa8"
    env["MIR_CONF"] = f"-w {WORKERS} -s {sched_flag} -m {dist_flag} --worker-stats"
    env["BENCH_NAME"] = bench
    env["BENCH_SCHED"] = sched_label
    env["BENCH_DIST"] = dist_label
    env["BENCH_TOPO"] = "numa8"
    env["BENCH_RUN_ID"] = str(run_id)

    try:
        result = subprocess.run([str(binary)], env=env, capture_output=True,
                                 text=True, timeout=120)
    except subprocess.TimeoutExpired:
        print(f"TIMEOUT: {bench} {sched_label} {dist_label} run {run_id}", file=sys.stderr)
        return None

    if result.returncode != 0:
        print(f"FAIL: {bench} {sched_label} {dist_label} run {run_id}: rc={result.returncode}",
              file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        return None

    lines = [l.strip() for l in result.stdout.strip().splitlines() if l.strip()]
    if not lines:
        print(f"EMPTY OUTPUT: {bench} {sched_label} {dist_label} run {run_id}", file=sys.stderr)
        return None

    line = lines[-1]
    parts = line.split(",")
    if len(parts) != len(FIELDS):
        print(f"MALFORMED OUTPUT: {bench} {sched_label} {dist_label} run {run_id}: {line!r}",
              file=sys.stderr)
        return None

    row = dict(zip(FIELDS, parts))

    # Rigorous validation
    if row.get("check_ok") != "1":
        print(f"CHECK FAILED (check_ok={row.get('check_ok')}): {bench} {sched_label} run {run_id}", file=sys.stderr)
        return None

    try:
        cycles = int(row["exec_cycles"])
        if cycles <= 0:
            print(f"INVALID CYCLES ({cycles}): {bench} {sched_label} run {run_id}", file=sys.stderr)
            return None
    except ValueError:
        print(f"NON-NUMERIC CYCLES: {row['exec_cycles']}", file=sys.stderr)
        return None

    return row


def main():
    expected_total = len(BENCHMARKS) * len(CONFIGS) * REPS
    print(f"Running Baseline NUMA Matrix Suite ({expected_total} runs: 5 benches x 4 configs x {REPS} reps)...")
    rows = []

    for bench in BENCHMARKS:
        for sched_label, sched_flag, dist_label, dist_flag in CONFIGS:
            for run_id in range(1, REPS + 1):
                row = run_one(bench, sched_label, sched_flag, dist_label, dist_flag, run_id)
                if row:
                    rows.append(row)
                    print(f"[{len(rows):3d}/{expected_total}] {bench:10s} {sched_label:4s} {dist_label:6s} "
                          f"run {run_id}: cycles={row['exec_cycles']} comm={row['comm_cost_avg']}")

    if len(rows) != expected_total:
        print(f"WARNING: Completed {len(rows)}/{expected_total} runs. Some runs failed or were rejected.", file=sys.stderr)
        if not rows:
            sys.exit(1)

    out_path = RESULTS / "matrix.csv"
    with open(out_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    print(f"\nSuccessfully wrote {len(rows)} validated rows to {out_path}")


if __name__ == "__main__":
    main()
