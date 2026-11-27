#!/usr/bin/env python3
"""Plot H10a 2D stencil placement results (1D-striped vs 2D-blocked) from raw_results.csv."""
import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parent.parent
CSV_PATH = ROOT / "results" / "raw_results.csv"
OUT_PATH = ROOT / "results" / "h10a_comparison.png"


def main():
    with CSV_PATH.open("r", encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    keys = [
        ("ws", "1d_striped", "WS\n1D-striped"),
        ("ws", "2d_block", "WS\n2D-blocked"),
        ("la", "1d_striped", "LA\n1D-striped"),
        ("la", "2d_block", "LA\n2D-blocked"),
    ]
    data = {(r["scheduler"], r["data_distribution"]): r for r in rows if r["benchmark"] == "h10a"}
    if not all((s, d) in data for s, d, _ in keys):
        raise SystemExit("H10a rows are missing; regenerate results/raw_results.csv with --all")

    labels = [lbl for _, _, lbl in keys]
    colors = ["#4C72B0", "#8172B2", "#C44E52", "#55A868"]
    metrics = [
        ("simulated_communication_cost", "Remote data access cost", "Modeled cycles"),
        ("congestion_queue_cycles", "Center-bisection link traffic", "Link-cycles"),
        ("simulated_execution_time", "Makespan (critical path)", "Modeled cycles"),
    ]
    fig, axes = plt.subplots(1, 3, figsize=(13.5, 4.9))
    for ax, (field, title, ylabel) in zip(axes, metrics):
        values = [int(data[(s, d)][field]) for s, d, _ in keys]
        bars = ax.bar(labels, values, color=colors, edgecolor="#222222", linewidth=0.8, width=0.62)
        ax.set_title(title, fontsize=13.5, fontweight="bold")
        ax.set_ylabel(ylabel, fontsize=11.5)
        ax.tick_params(axis="both", labelsize=10.5)
        ax.grid(axis="y", linestyle="--", alpha=0.45)
        ax.set_axisbelow(True)
        ax.set_ylim(0, max(values) * 1.24 if max(values) else 1)
        for bar, value in zip(bars, values):
            ax.annotate(f"{value:,}", (bar.get_x() + bar.get_width() / 2, bar.get_height()),
                        xytext=(0, 4), textcoords="offset points", ha="center", va="bottom", fontsize=10.5, fontweight="bold")

    fig.suptitle("H10a: 2D Stencil Placement — 1D Striping vs 2D Blocking", fontsize=16, fontweight="bold")
    fig.text(0.5, 0.015,
             "8x8 Jacobi stencil (4 sweeps = 256 tasks; 64 KB block + 4x16 KB N/S/E/W halos). "
             "2D blocking preserves 1-hop neighbor locality and slashes bisection traffic.",
             ha="center", fontsize=10.5)
    fig.tight_layout(rect=(0, 0.06, 1, 0.92))
    fig.savefig(OUT_PATH, dpi=180, bbox_inches="tight")
    print(f"Saved H10a comparison graph: {OUT_PATH}")


if __name__ == "__main__":
    main()
