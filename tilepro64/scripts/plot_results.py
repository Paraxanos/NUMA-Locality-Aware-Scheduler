#!/usr/bin/env python3
"""
Generates publication-quality charts from tilepro64/results/raw_results.csv.
Adheres strictly to Sections 28, 29, and 30 of prompt.md.
Uses standard library csv + matplotlib (no pandas dependency required).
"""
import csv
from pathlib import Path
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

REPO_ROOT = Path(__file__).resolve().parent.parent
RESULTS_DIR = REPO_ROOT / "results"
RAW_CSV = RESULTS_DIR / "raw_results.csv"
OUT_PNG = RESULTS_DIR / "tilepro64_results.png"

def main():
    if not RAW_CSV.exists():
        print(f"Error: {RAW_CSV} not found. Run scripts/run_experiments.py first.")
        return 1

    rows = []
    with open(RAW_CSV, "r") as f:
        reader = csv.DictReader(f)
        for r in reader:
            rows.append(r)

    # Filter representative comparison for default vicinity = 4
    # Configurations: WS+Coarse, WS+Fine, LA+Coarse, LA+Fine
    benchmarks = ["map", "vecmul"]
    configs = [
        ("WS + Coarse", "ws", "coarse"),
        ("WS + Fine",   "ws", "fine"),
        ("LA + Coarse", "la", "coarse"),
        ("LA + Fine",   "la", "fine"),
    ]

    comm_data = {b: [] for b in benchmarks}
    time_data = {b: [] for b in benchmarks}

    for b in benchmarks:
        for label, s, d in configs:
            matched = [
                r for r in rows
                if r["benchmark"] == b and r["scheduler"] == s and r["data_distribution"] == d and r["vicinity"] == "4"
            ]
            if matched:
                comm_data[b].append(float(matched[0]["simulated_communication_cost"]))
                time_data[b].append(float(matched[0]["simulated_execution_time"]))
            else:
                comm_data[b].append(0.0)
                time_data[b].append(0.0)

    # Plot 2-panel chart as specified in Section 30 of prompt.md
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5.5))
    x = range(len(benchmarks))
    bar_width = 0.18
    colors = ["#4C72B0", "#55A868", "#C44E52", "#8172B2"]

    for i, (label, _, _) in enumerate(configs):
        offset = (i - 1.5) * bar_width
        vals_comm = [comm_data[b][i] / 1e6 for b in benchmarks] # Mega-cycles
        vals_time = [time_data[b][i] / 1e3 for b in benchmarks] # Kilo-cycles

        ax1.bar([pos + offset for pos in x], vals_comm, width=bar_width, label=label, color=colors[i], edgecolor="black", linewidth=0.6)
        ax2.bar([pos + offset for pos in x], vals_time, width=bar_width, label=label, color=colors[i], edgecolor="black", linewidth=0.6)

    # Panel 1: Communication / Access Cost
    ax1.set_title("Modeled Communication / Access Cost\n(Lower is Better)", fontsize=12, fontweight="bold")
    ax1.set_ylabel("Communication Cost (Million Cycles)", fontsize=11)
    ax1.set_xticks(x)
    ax1.set_xticklabels(["Map (48-64 Tasks)", "Vecmul (Overlapping Tiles)"], fontsize=11)
    ax1.grid(axis="y", linestyle="--", alpha=0.6)
    ax1.legend(title="Configuration", fontsize=9, loc="upper right")

    # Panel 2: Simulated Execution Time
    ax2.set_title("Simulated Execution Time (Makespan)\n(Lower is Better)", fontsize=12, fontweight="bold")
    ax2.set_ylabel("Simulated Execution Time (Thousand Cycles)", fontsize=11)
    ax2.set_xticks(x)
    ax2.set_xticklabels(["Map (48-64 Tasks)", "Vecmul (Overlapping Tiles)"], fontsize=11)
    ax2.grid(axis="y", linestyle="--", alpha=0.6)
    ax2.legend(title="Configuration", fontsize=9, loc="upper right")

    fig.suptitle("TILEPro64 8×8 Mesh Architecture: Locality-Aware vs Work-Stealing Baseline", fontsize=14, fontweight="bold", y=0.98)
    plt.tight_layout()
    plt.subplots_adjust(top=0.88)
    plt.savefig(OUT_PNG, dpi=180)
    print(f"Successfully generated result chart -> {OUT_PNG}")

if __name__ == "__main__":
    main()
