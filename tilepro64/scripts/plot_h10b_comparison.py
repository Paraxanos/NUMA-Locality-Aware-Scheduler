#!/usr/bin/env python3
"""Plot H10b stealing-policy results from the full experiment CSV."""
import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parent.parent
CSV_PATH = ROOT / "results" / "raw_results.csv"
OUT_PATH = ROOT / "results" / "h10b_comparison.png"


def main():
    with CSV_PATH.open("r", encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    policies = ("ws", "la", "dp")
    data = {row["scheduler"]: row for row in rows if row["benchmark"] == "h10b"}
    if not all(policy in data for policy in policies):
        raise SystemExit("H10b WS, LA, and DP rows are missing; regenerate results/raw_results.csv with --all")

    labels = ["WS\nunrestricted", "LA\n2-hop", "Dynamic\npriority"]
    colors = ["#4C72B0", "#55A868", "#C44E52"]
    metrics = [
        ("simulated_communication_cost", "Communication cost", "Modeled cycles"),
        ("simulated_execution_time", "Makespan", "Modeled cycles"),
        ("idle_probe_rounds", "Idle search probes", "Tile-probes"),
    ]
    fig, axes = plt.subplots(2, 2, figsize=(11, 7))
    axes = list(axes.flat)
    for ax, (field, title, ylabel) in zip(axes[:3], metrics):
        values = [int(data[p][field]) for p in policies]
        bars = ax.bar(labels, values, color=colors, edgecolor="#222222", linewidth=0.7, width=0.66)
        ax.set_title(title, fontweight="bold")
        ax.set_ylabel(ylabel)
        ax.grid(axis="y", linestyle="--", alpha=0.45)
        ax.set_axisbelow(True)
        ax.set_ylim(0, max(values) * 1.2 if max(values) else 1)
        for bar, value in zip(bars, values):
            ax.annotate(f"{value:,}", (bar.get_x() + bar.get_width() / 2, bar.get_height()),
                        xytext=(0, 4), textcoords="offset points", ha="center", va="bottom", fontsize=9)

    ax = axes[3]
    avg_hops = [int(data[p]["total_steal_distance"]) / max(1, int(data[p]["steal_count"])) for p in policies]
    bars = ax.bar(labels, avg_hops, color=colors, edgecolor="#222222", linewidth=0.7, width=0.66)
    ax.set_title("Average steal distance", fontweight="bold")
    ax.set_ylabel("Mesh hops per steal")
    ax.grid(axis="y", linestyle="--", alpha=0.45)
    ax.set_axisbelow(True)
    ax.set_ylim(0, max(avg_hops) * 1.2)
    for bar, value in zip(bars, avg_hops):
        ax.annotate(f"{value:.2f}", (bar.get_x() + bar.get_width() / 2, bar.get_height()),
                    xytext=(0, 4), textcoords="offset points", ha="center", va="bottom", fontsize=9)

    fig.suptitle("H10b: Corner-Home Work-Stealing Policies", fontsize=15, fontweight="bold")
    fig.text(0.5, 0.015,
             "64 tasks; 4 KB per task homed at tile 63; mixed estimated compute times. "
             "Dynamic priority ages idle workers and expands search radius.",
             ha="center", fontsize=9)
    fig.tight_layout(rect=(0, 0.06, 1, 0.94))
    fig.savefig(OUT_PATH, dpi=180, bbox_inches="tight")
    print(f"Saved H10b comparison graph: {OUT_PATH}")


if __name__ == "__main__":
    main()
