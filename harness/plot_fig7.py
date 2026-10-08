#!/usr/bin/env python3
"""Reproduce a Figure-7-style chart from results/numa/matrix.csv.
See docs/14-numa-opteron-implementation-plan.md Step 5/6.

Two panels, not one: comm_cost_avg (the modelled per-task locality cost) and exec_cycles (real
wall-clock-ish execution time). They diverge for Reduction/Jacobi/SparseLU -- la+coarse can look
"best" on comm_cost alone (perfect locality minimizes per-byte access cost) while actually being
worse on exec_cycles (that same concentration starves parallelism). Reporting comm_cost alone
would misrepresent those three benchmarks' real story; this is the paper's own point about a
locality-only cost model being incomplete, showing up directly in which metric moves.
"""
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd

REPO = Path(__file__).resolve().parent.parent
RESULTS = REPO / "results" / "numa"


def normalized_pivot(agg, value_col):
    baseline = (
        agg[(agg.scheduler_policy == "ws") & (agg.dist_policy == "coarse")]
        .set_index("benchmark")[value_col]
    )
    col = f"normalized_{value_col}"
    agg[col] = agg.apply(lambda r: r[value_col] / baseline[r.benchmark], axis=1)
    agg["config"] = agg.scheduler_policy + "+" + agg.dist_policy
    return agg.pivot(index="benchmark", columns="config", values=col)


def main():
    df = pd.read_csv(RESULTS / "matrix.csv")
    for col in ["exec_cycles", "comm_cost_avg"]:
        df[col] = pd.to_numeric(df[col], errors="coerce")

    agg = (
        df.groupby(["benchmark", "scheduler_policy", "dist_policy"])
        .agg(
            comm_cost_avg=("comm_cost_avg", "mean"),
            exec_cycles=("exec_cycles", "mean"),
            n=("run_id", "count"),
        )
        .reset_index()
    )

    commcost_pivot = normalized_pivot(agg.copy(), "comm_cost_avg")
    cycles_pivot = normalized_pivot(agg.copy(), "exec_cycles")

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 5))
    commcost_pivot.plot(kind="bar", ax=ax1, legend=False)
    ax1.set_ylabel("Normalized comm_cost (vs. ws+coarse)")
    ax1.set_title("Modelled communication cost")
    ax1.axhline(1.0, color="gray", linewidth=0.8, linestyle="--")
    ax1.tick_params(axis="x", rotation=0)

    cycles_pivot.plot(kind="bar", ax=ax2, legend=True)
    ax2.set_ylabel("Normalized exec_cycles (vs. ws+coarse)")
    ax2.set_title("Real execution time")
    ax2.axhline(1.0, color="gray", linewidth=0.8, linestyle="--")
    ax2.tick_params(axis="x", rotation=0)
    ax2.legend(loc="upper left", bbox_to_anchor=(1.0, 1.0))

    fig.suptitle("NUMA reproduction: modelled cost vs. real execution time")
    plt.tight_layout()

    out = RESULTS / "fig7_reproduction.png"
    plt.savefig(out, dpi=150)
    print(f"Wrote {out}\n")
    print("=== comm_cost_avg, normalized to ws+coarse ===")
    print(commcost_pivot.to_string())
    print("\n=== exec_cycles, normalized to ws+coarse ===")
    print(cycles_pivot.to_string())


if __name__ == "__main__":
    main()
