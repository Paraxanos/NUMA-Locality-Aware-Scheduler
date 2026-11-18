#!/usr/bin/env python3
"""Reproduce a Figure-7-style chart from results/numa/matrix.csv.
See docs/14-numa-opteron-implementation-plan.md Step 5/6.

Two panels: comm_cost_avg (the modelled per-task locality cost) and exec_cycles (real
wall-clock execution time). They diverge for Reduction/Jacobi/SparseLU -- la+coarse can look
"best" on comm_cost alone (perfect locality minimizes per-byte access cost) while actually being
worse on exec_cycles (that same concentration starves parallelism). Reporting comm_cost alone
would misrepresent those three benchmarks' real story; this is the paper's own point about a
locality-only cost model being incomplete, showing up directly in which metric moves.
"""
import csv
from collections import defaultdict
from pathlib import Path
import numpy as np

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

REPO = Path(__file__).resolve().parent.parent
RESULTS = REPO / "results" / "numa"


def main():
    matrix_path = RESULTS / "matrix.csv"
    if not matrix_path.exists():
        print(f"Error: {matrix_path} not found.")
        return

    with open(matrix_path, "r") as f:
        rows = list(csv.DictReader(f))

    # Group by benchmark, config
    data = defaultdict(lambda: defaultdict(list))
    for r in rows:
        b = r["benchmark"]
        cfg = f"{r['scheduler_policy']}+{r['dist_policy']}"
        try:
            comm = float(r["comm_cost_avg"])
            cycles = float(r["exec_cycles"])
            data[b][cfg].append({"comm": comm, "cycles": cycles})
        except (ValueError, KeyError):
            continue

    benchmarks = ["map", "matmul", "reduction", "jacobi", "sparselu"]
    configs = ["la+coarse", "la+fine", "ws+coarse", "ws+fine"]

    norm_comm = {cfg: [] for cfg in configs}
    norm_cycles = {cfg: [] for cfg in configs}

    for b in benchmarks:
        ws_coarse_comm = np.mean([x["comm"] for x in data[b]["ws+coarse"]]) if data[b]["ws+coarse"] else 1.0
        ws_coarse_cyc = np.mean([x["cycles"] for x in data[b]["ws+coarse"]]) if data[b]["ws+coarse"] else 1.0
        for cfg in configs:
            m_comm = np.mean([x["comm"] for x in data[b][cfg]]) if data[b][cfg] else 0.0
            m_cyc = np.mean([x["cycles"] for x in data[b][cfg]]) if data[b][cfg] else 0.0
            norm_comm[cfg].append(m_comm / ws_coarse_comm)
            norm_cycles[cfg].append(m_cyc / ws_coarse_cyc)

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 5))
    x = np.arange(len(benchmarks))
    width = 0.2

    for i, cfg in enumerate(configs):
        offset = (i - 1.5) * width
        ax1.bar(x + offset, norm_comm[cfg], width, label=cfg)
        ax2.bar(x + offset, norm_cycles[cfg], width, label=cfg)

    ax1.set_xticks(x)
    ax1.set_xticklabels(benchmarks)
    ax1.set_ylabel("Normalized comm_cost (vs. ws+coarse)")
    ax1.set_title("Modelled communication cost")
    ax1.axhline(1.0, color="gray", linewidth=0.8, linestyle="--")

    ax2.set_xticks(x)
    ax2.set_xticklabels(benchmarks)
    ax2.set_ylabel("Normalized exec_cycles (vs. ws+coarse)")
    ax2.set_title("Real execution time")
    ax2.axhline(1.0, color="gray", linewidth=0.8, linestyle="--")
    ax2.legend(loc="upper left", bbox_to_anchor=(1.0, 1.0))

    fig.suptitle("NUMA reproduction: modelled cost vs. real execution time")
    plt.tight_layout()

    out = RESULTS / "fig7_reproduction.png"
    plt.savefig(out, dpi=150)
    print(f"Wrote {out}\n")

    print("=== comm_cost_avg, normalized to ws+coarse ===")
    header = f"{'benchmark':<12}" + "".join(f"{cfg:<15}" for cfg in configs)
    print(header)
    for idx, b in enumerate(benchmarks):
        row_str = f"{b:<12}" + "".join(f"{norm_comm[cfg][idx]:<15.4f}" for cfg in configs)
        print(row_str)

    print("\n=== exec_cycles, normalized to ws+coarse ===")
    print(header)
    for idx, b in enumerate(benchmarks):
        row_str = f"{b:<12}" + "".join(f"{norm_cycles[cfg][idx]:<15.4f}" for cfg in configs)
        print(row_str)


if __name__ == "__main__":
    main()
