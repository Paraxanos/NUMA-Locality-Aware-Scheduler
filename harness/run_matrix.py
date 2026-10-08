#!/usr/bin/env python3
"""Run the NUMA reproduction config matrix and write results/numa/matrix.csv.
See docs/14-numa-opteron-implementation-plan.md Step 4/5.
"""
import csv
import os
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BUILD = REPO / "build"
RESULTS = REPO / "results" / "numa"
RESULTS.mkdir(parents=True, exist_ok=True)

BENCHMARKS = ["map", "matmul", "reduction", "jacobi", "sparselu"]
# (scheduler_label, MIR -s value, dist_label, MIR -m value)
CONFIGS = [
    ("ws", "ws-de-node", "coarse", "coarse"),
    ("ws", "ws-de-node", "fine", "fine"),
    ("la", "numa", "coarse", "coarse"),
    ("la", "numa", "fine", "fine"),
]
REPS = 5
WORKERS = 24

FIELDS = ["benchmark", "scheduler_policy", "dist_policy", "topology", "run_id",
          "exec_cycles", "comm_cost_total", "comm_tasks_total", "comm_cost_avg",
          "tasks_stolen", "tasks_owned", "check_ok"]


def run_one(bench, sched_label, sched_flag, dist_label, dist_flag, run_id):
    env = os.environ.copy()
    env["MIR_ARCH_NAME"] = "sim_numa8"
    env["MIR_CONF"] = f"-w {WORKERS} -s {sched_flag} -m {dist_flag} --worker-stats"
    env["BENCH_NAME"] = bench
    env["BENCH_SCHED"] = sched_label
    env["BENCH_DIST"] = dist_label
    env["BENCH_TOPO"] = "numa8"
    env["BENCH_RUN_ID"] = str(run_id)
    binary = BUILD / f"bench_{bench}"
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
    line = result.stdout.strip().splitlines()[-1]
    parts = line.split(",")
    if len(parts) != len(FIELDS):
        print(f"MALFORMED OUTPUT: {bench} {sched_label} {dist_label} run {run_id}: {line!r}",
              file=sys.stderr)
        return None
    return dict(zip(FIELDS, parts))


def main():
    rows = []
    for bench in BENCHMARKS:
        for sched_label, sched_flag, dist_label, dist_flag in CONFIGS:
            for run_id in range(1, REPS + 1):
                row = run_one(bench, sched_label, sched_flag, dist_label, dist_flag, run_id)
                if row:
                    rows.append(row)
                    print(",".join(row[f] for f in FIELDS))

    if not rows:
        print("No successful runs; nothing written.", file=sys.stderr)
        sys.exit(1)

    out_path = RESULTS / "matrix.csv"
    with open(out_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    print(f"\nWrote {len(rows)} rows to {out_path}")


if __name__ == "__main__":
    main()
