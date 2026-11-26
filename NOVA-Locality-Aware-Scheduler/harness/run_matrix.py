#!/usr/bin/env python3
"""Run the complete NUMA benchmarking matrix including baselines and NOVA co-scheduler.
Outputs to results/numa/matrix.csv.
"""
import csv
import os
import random
import subprocess
import sys
import time
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
    ("nova-a", "nova-a", "coarse", "coarse"),
    ("nova-ab", "nova-ab", "coarse", "coarse"),
    ("nova", "nova", "coarse", "coarse"),
    ("nova", "nova", "fine", "fine"),
]
REPS = 10
WORKERS = 24
SHUFFLE_SEED = 20261123  # fixed seed: shuffled order is reproducible, not just "random"

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
    lines = result.stdout.strip().splitlines()
    if not lines:
        print(f"EMPTY OUTPUT: {bench} {sched_label} {dist_label} run {run_id}", file=sys.stderr)
        return None
    line = lines[-1]
    parts = line.split(",")
    if len(parts) != len(FIELDS):
        print(f"MALFORMED OUTPUT: {bench} {sched_label} {dist_label} run {run_id}: {line!r}",
              file=sys.stderr)
        return None
    return dict(zip(FIELDS, parts))


def main():
    # Build the full job list up front and shuffle it (fixed seed -> reproducible order, logged
    # below) instead of the old fixed benchmark->config->rep nested-loop order. Fairness concern
    # found during validation: the old order always ran `ws` first and `nova`/`nova+fine` last for
    # every single benchmark, so any session-level drift (thermal, OS scheduler state, background
    # load) would systematically bias later-run configs rather than average out across reps.
    jobs = []
    for bench in BENCHMARKS:
        for sched_label, sched_flag, dist_label, dist_flag in CONFIGS:
            for run_id in range(1, REPS + 1):
                jobs.append((bench, sched_label, sched_flag, dist_label, dist_flag, run_id))
    rng = random.Random(SHUFFLE_SEED)
    rng.shuffle(jobs)

    total_runs = len(jobs)
    print(f"Starting Benchmark Probe: {total_runs} total runs across {len(BENCHMARKS)} benchmarks "
          f"(shuffled order, seed={SHUFFLE_SEED})...")
    start_time = time.time()
    rows = []
    completed = 0

    for bench, sched_label, sched_flag, dist_label, dist_flag, run_id in jobs:
        row = run_one(bench, sched_label, sched_flag, dist_label, dist_flag, run_id)
        completed += 1
        if row:
            rows.append(row)
            if completed % 10 == 0 or completed == total_runs:
                elapsed = time.time() - start_time
                print(f"[{completed}/{total_runs}] ({elapsed:.1f}s) {bench} {sched_label}+{dist_label} #{run_id}: "
                      f"cycles={row['exec_cycles']} comm={row['comm_cost_avg']} ok={row['check_ok']}")

    if not rows:
        print("No successful runs; nothing written.", file=sys.stderr)
        sys.exit(1)

    out_path = RESULTS / "matrix.csv"
    with open(out_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    print(f"\nWrote {len(rows)}/{total_runs} rows to {out_path} in {time.time() - start_time:.1f}s")


if __name__ == "__main__":
    main()
