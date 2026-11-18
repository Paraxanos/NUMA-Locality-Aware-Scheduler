#!/usr/bin/env python3
"""Run the NOVA scheduler evaluation and ablation matrix.

Outputs results to results/nova_evaluation/matrix.csv.
Configurations evaluated:
  - ws+coarse:            Work-stealing baseline
  - la+coarse:            Locality-aware baseline
  - nova+coarse:          Full NOVA (Locality + Dynamic Queue Pressure + Shadow Index)
  - nova+fine:            NOVA with fine-grain page placement
  - nova_noshadow+coarse: NOVA Ablation: Shadow Index disabled (NOVA_DISABLE_SHADOW_INDEX=1)
  - nova_noshadow+fine:   NOVA Ablation: Shadow Index disabled with fine placement

Validates all runs: check_ok == 1, exec_cycles > 0.
"""
import csv
import os
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BUILD = REPO / "build"
RESULTS = REPO / "results" / "nova_evaluation"
RESULTS.mkdir(parents=True, exist_ok=True)

BENCHMARKS = ["map", "matmul", "reduction", "jacobi", "sparselu"]
# (sched_label, mir_sched_flag, dist_label, mir_dist_flag, extra_env)
CONFIGS = [
    ("ws", "ws-de-node", "coarse", "coarse", {}),
    ("la", "numa", "coarse", "coarse", {}),
    ("nova", "nova", "coarse", "coarse", {}),
    ("nova", "nova", "fine", "fine", {}),
    ("nova_noshadow", "nova", "coarse", "coarse", {"NOVA_DISABLE_SHADOW_INDEX": "1"}),
    ("nova_noshadow", "nova", "fine", "fine", {"NOVA_DISABLE_SHADOW_INDEX": "1"}),
]
REPS = 5
WORKERS = 24

FIELDS = [
    "benchmark", "scheduler_policy", "dist_policy", "topology", "run_id",
    "exec_cycles", "comm_cost_total", "comm_tasks_total", "comm_cost_avg",
    "tasks_stolen", "tasks_owned", "check_ok"
]


def run_one(bench, sched_label, sched_flag, dist_label, dist_flag, extra_env, run_id):
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
    env.update(extra_env)

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
    print(f"Running NOVA Evaluation & Ablation Suite ({expected_total} runs: 5 benches x 6 configs x {REPS} reps)...")
    rows = []

    for bench in BENCHMARKS:
        for sched_label, sched_flag, dist_label, dist_flag, extra_env in CONFIGS:
            for run_id in range(1, REPS + 1):
                row = run_one(bench, sched_label, sched_flag, dist_label, dist_flag, extra_env, run_id)
                if row:
                    rows.append(row)
                    print(f"[{len(rows):3d}/{expected_total}] {bench:10s} {sched_label:14s} {dist_label:6s} "
                          f"run {run_id}: cycles={row['exec_cycles']} comm={row['comm_cost_avg']}")

    if len(rows) != expected_total:
        print(f"WARNING: Completed {len(rows)}/{expected_total} runs.", file=sys.stderr)
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
