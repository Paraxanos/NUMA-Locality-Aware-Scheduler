#!/usr/bin/env python3
"""Plot the H9 FIFO versus priority results recorded in raw_results.csv."""
import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parent.parent
CSV_PATH = ROOT / "results" / "raw_results.csv"
OUT_PATH = ROOT / "results" / "h9_comparison.png"


def main():
    with CSV_PATH.open("r", encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    h9 = {row["scheduler"]: row for row in rows if row["benchmark"] == "h9"}
    required = ("simple", "fifo", "priority")
    if not all(policy in h9 for policy in required):
        raise SystemExit("H9 FIFO and priority rows are missing from results/raw_results.csv")

    labels = ["Simple XY", "FIFO", "Priority"]
    colors = ["#55A868", "#4C72B0", "#DD8452"]
    metrics = [
        ("congestion_queue_cycles", "Aggregate queue wait", "Transfer-cycles"),
        ("simulated_execution_time", "Network makespan", "Cycles"),
        ("max_link_busy_cycles", "Busiest-link service", "Cycles"),
    ]

    fig, axes = plt.subplots(1, 3, figsize=(13.5, 4.9))
    for ax, (field, title, ylabel) in zip(axes, metrics):
        values = [int(h9[p][field]) for p in required]
        bars = ax.bar(labels, values, color=colors, edgecolor="#222222", linewidth=0.8, width=0.60)
        ax.set_title(title, fontsize=13.5, fontweight="bold")
        ax.set_ylabel(ylabel, fontsize=11.5)
        ax.tick_params(axis="both", labelsize=11)
        ax.grid(axis="y", linestyle="--", alpha=0.45)
        ax.set_axisbelow(True)
        upper = max(values) * 1.24 if max(values) else 1
        ax.set_ylim(0, upper)
        for bar, value in zip(bars, values):
            ax.annotate(f"{value:,}", (bar.get_x() + bar.get_width() / 2, bar.get_height()),
                        xytext=(0, 4), textcoords="offset points", ha="center", va="bottom", fontsize=11.5, fontweight="bold")

    fig.suptitle("H9 Mesh Traffic: FIFO vs Priority Arbitration", fontsize=16, fontweight="bold")
    fig.text(0.5, 0.015,
             "Custom simulator; 32 × 4 KB center-crossing transfers; assumed 64 B/cycle/link. "
             "Simple XY omits shared-link contention.",
             ha="center", fontsize=10.5)
    fig.tight_layout(rect=(0, 0.08, 1, 0.90))
    fig.savefig(OUT_PATH, dpi=180, bbox_inches="tight")
    print(f"Saved H9 comparison graph: {OUT_PATH}")


if __name__ == "__main__":
    main()
