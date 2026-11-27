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
    fig, axes = plt.subplots(1, 3, figsize=(13.5, 4.9))
    for ax, (field, title, ylabel) in zip(axes, panels):
        data = [int(values[p][field]) for p in policies]
        bars = ax.bar(labels, data, color=colors, edgecolor="#222222", linewidth=0.8, width=0.64)
        ax.set_title(title, fontweight="bold", fontsize=13.5)
        ax.set_ylabel(ylabel, fontsize=11.5)
        ax.tick_params(axis="both", labelsize=10.5)
        ax.grid(axis="y", linestyle="--", alpha=0.45)
        ax.set_axisbelow(True)
        ax.set_ylim(0, max(data) * 1.24 if max(data) else 1)
        for bar, value in zip(bars, data):
            ax.annotate(f"{value:,}", (bar.get_x() + bar.get_width() / 2, bar.get_height()),
                        xytext=(0, 4), textcoords="offset points", ha="center", va="bottom", fontsize=11.5, fontweight="bold")
    fig.suptitle("H9 Worst-Case: 32 Transfers Clash on One Center Route", fontweight="bold", fontsize=16)
    fig.text(0.5, 0.02,
             "32 simultaneous 4 KB transfers; 7-hop XY path; 64 B/cycle/link. "
             "Simple XY omits contention; FIFO & priority share finite links.",
             ha="center", fontsize=10.5)
    fig.tight_layout(rect=(0, 0.09, 1, 0.90))
    fig.savefig(OUT_PATH, dpi=180, bbox_inches="tight")
    print(f"Saved H9 worst-case graph: {OUT_PATH}")


if __name__ == "__main__":
    main()
