#!/usr/bin/env python3
"""Plot H9 worst-case path-clash results from raw_results.csv."""
import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parent.parent
CSV_PATH = ROOT / "results" / "raw_results.csv"
OUT_PATH = ROOT / "results" / "h9_worst_comparison.png"


def main():
    with CSV_PATH.open("r", encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    policies = ("simple", "fifo", "priority")
    values = {row["scheduler"]: row for row in rows if row["benchmark"] == "h9_worst"}
    if not all(policy in values for policy in policies):
        raise SystemExit("H9 worst-case rows are missing; regenerate results/raw_results.csv with bin/tilepro64 --all")

    labels = ["Simple XY\n(no contention)", "FIFO\n(shared links)", "Priority\n(shared links)"]
    colors = ["#55A868", "#4C72B0", "#DD8452"]
    panels = [
        ("congestion_queue_cycles", "Aggregate queue wait", "Transfer-cycles"),
        ("simulated_execution_time", "Network makespan", "Cycles"),
        ("max_link_busy_cycles", "Busiest-link service", "Cycles"),
    ]
    fig, axes = plt.subplots(1, 3, figsize=(14, 5))
    for ax, (field, title, ylabel) in zip(axes, panels):
        data = [int(values[p][field]) for p in policies]
        bars = ax.bar(labels, data, color=colors, edgecolor="#222222", linewidth=0.7, width=0.68)
        ax.set_title(title, fontweight="bold", fontsize=12)
        ax.set_ylabel(ylabel)
        ax.grid(axis="y", linestyle="--", alpha=0.45)
        ax.set_axisbelow(True)
        ax.set_ylim(0, max(data) * 1.22 if max(data) else 1)
        for bar, value in zip(bars, data):
            ax.annotate(f"{value:,}", (bar.get_x() + bar.get_width() / 2, bar.get_height()),
                        xytext=(0, 4), textcoords="offset points", ha="center", va="bottom", fontsize=10)
    fig.suptitle("H9 Worst-Case: 32 Transfers Clash on One Center Route", fontweight="bold", fontsize=15)
    fig.text(0.5, 0.02,
             "32 simultaneous 4 KB transfers; seven-hop XY path; assumed 64 B/cycle/link. "
             "Simple XY omits contention; FIFO and priority share finite links.",
             ha="center", fontsize=9)
    fig.tight_layout(rect=(0, 0.09, 1, 0.90))
    fig.savefig(OUT_PATH, dpi=180, bbox_inches="tight")
    print(f"Saved H9 worst-case graph: {OUT_PATH}")


if __name__ == "__main__":
    main()
