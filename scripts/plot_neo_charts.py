#!/usr/bin/env python3
"""
scripts/plot_neo_charts.py
Generates high-aesthetic, Swiss neo-brutalist charts matching reference.pdf design language.
Updates:
  1. results/numa/fig7_reproduction.png
  2. tilepro64/results/tilepro64_results.png
"""

from pathlib import Path
import csv
from collections import defaultdict
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

REPO = Path(__file__).resolve().parent.parent

# Set global matplotlib style
plt.rcParams['font.family'] = 'sans-serif'
plt.rcParams['font.sans-serif'] = ['DejaVu Sans', 'Arial', 'Helvetica']
plt.rcParams['axes.edgecolor'] = '#111827'
plt.rcParams['axes.linewidth'] = 1.6
plt.rcParams['xtick.color'] = '#111827'
plt.rcParams['ytick.color'] = '#111827'

COLOR_PAPER = '#F4F0EA'
COLOR_INK = '#111827'
COLOR_MUTED = '#64748B'
COLOR_GRID = '#E2DDD4'

# Cohesive Palette matching PPT
PALETTE = {
    'ws+coarse': '#1E293B',    # Dark Ink Slate (Baseline)
    'ws+fine':   '#94A3B8',    # Cool Slate Gray
    'la+fine':   '#818CF8',    # Lavender / Soft Blue
    'la+coarse': '#FFD147',    # Canary Yellow (Hero Winner / Highlight)
}

def plot_numa():
    numa_csv = REPO / "results" / "numa" / "matrix.csv"
    if not numa_csv.exists():
        print(f"Error: {numa_csv} not found")
        return

    # Parse matrix.csv with standard csv
    rows = []
    with open(numa_csv, "r") as f:
        reader = csv.DictReader(f)
        for r in reader:
            rows.append(r)

    # Aggregate means
    grouped = defaultdict(lambda: {'comm': [], 'cycles': []})
    for r in rows:
        key = (r['benchmark'], r['scheduler_policy'], r['dist_policy'])
        try:
            grouped[key]['comm'].append(float(r['comm_cost_avg']))
            grouped[key]['cycles'].append(float(r['exec_cycles']))
        except ValueError:
            pass

    means = {}
    for key, vals in grouped.items():
        means[key] = {
            'comm': sum(vals['comm']) / len(vals['comm']),
            'cycles': sum(vals['cycles']) / len(vals['cycles']),
        }

    benchmarks = ["jacobi", "map", "matmul", "reduction", "sparselu"]
    configs = ["ws+coarse", "ws+fine", "la+fine", "la+coarse"]
    labels = ["WS + Coarse (Base)", "WS + Fine", "LA + Fine", "LA + Coarse (Dealer)"]

    # Compute normalized values vs ws+coarse
    norm_comm = {b: {} for b in benchmarks}
    norm_cycles = {b: {} for b in benchmarks}

    for b in benchmarks:
        base_comm = means[(b, 'ws', 'coarse')]['comm']
        base_cycles = means[(b, 'ws', 'coarse')]['cycles']
        for cfg in configs:
            s, d = cfg.split('+')
            norm_comm[b][cfg] = means[(b, s, d)]['comm'] / base_comm
            norm_cycles[b][cfg] = means[(b, s, d)]['cycles'] / base_cycles

    benchmarks = ["jacobi", "map", "matmul", "reduction", "sparselu"]
    configs = ["ws+coarse", "ws+fine", "la+fine", "la+coarse"]
    labels = ["WS + Coarse (Base)", "WS + Fine", "LA + Fine", "LA + Coarse (Dealer)"]

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5.8), facecolor=COLOR_PAPER)

    x = range(len(benchmarks))
    bar_w = 0.18

    for ax, p_data, title, y_label in [
        (ax1, norm_comm, "Modeled Communication Cost", "Normalized Cost (vs. WS+Coarse)"),
        (ax2, norm_cycles, "Real Execution Time (Cycles)", "Normalized Execution Time (vs. WS+Coarse)")
    ]:
        ax.set_facecolor("#FFFFFF")
        ax.grid(axis='y', color=COLOR_GRID, linestyle='--', linewidth=1.0, zorder=0)

        for i, (cfg, lbl) in enumerate(zip(configs, labels)):
            offset = (i - 1.5) * bar_w
            vals = [p_data[b][cfg] for b in benchmarks]
            bars = ax.bar(
                [pos + offset for pos in x], vals,
                width=bar_w, label=lbl, color=PALETTE[cfg],
                edgecolor=COLOR_INK, linewidth=1.5, zorder=3
            )

        ax.axhline(1.0, color=COLOR_MUTED, linestyle='--', linewidth=1.2, zorder=4)
        ax.set_xticks(x)
        ax.set_xticklabels([b.upper() for b in benchmarks], fontsize=11, fontweight='bold', color=COLOR_INK)
        ax.set_ylabel(y_label, fontsize=11, fontweight='bold', color=COLOR_INK, labelpad=8)
        ax.set_title(title, fontsize=13, fontweight='bold', color=COLOR_INK, pad=12)
        ax.tick_params(colors=COLOR_INK, labelsize=10)
        ax.set_ylim(0, 1.25)

    # Callout on Map in ax1 (-45.1% comm cost)
    ax1.annotate(
        "-45.1% Comm",
        xy=(1 + 1.5 * bar_w, norm_comm['map']['la+coarse']),
        xytext=(1 + 0.5 * bar_w, 0.75),
        arrowprops=dict(facecolor=COLOR_INK, shrink=0.08, width=1.5, headwidth=6),
        fontsize=9, fontweight='bold', color='#047857',
        bbox=dict(boxstyle='round,pad=0.3', facecolor='#DCFCE7', edgecolor=COLOR_INK, linewidth=1.2)
    )

    # Callout on SparseLU in ax2 (The Paradox: +15.1% slowdown)
    ax2.annotate(
        "THE PARADOX\n+15.1% SLOWER",
        xy=(4 + 1.5 * bar_w, norm_cycles['sparselu']['la+coarse']),
        xytext=(3.4, 1.15),
        arrowprops=dict(facecolor='#DC2626', shrink=0.08, width=1.8, headwidth=7),
        fontsize=9, fontweight='bold', color='#DC2626',
        bbox=dict(boxstyle='round,pad=0.4', facecolor='#FEE2E2', edgecolor=COLOR_INK, linewidth=1.4)
    )

    # Clean neo-brutalist legend
    handles, lbls = ax1.get_legend_handles_labels()
    fig.legend(
        handles, lbls, loc='upper center', ncol=4,
        bbox_to_anchor=(0.5, 0.99), frameon=True,
        facecolor='#FFFFFF', edgecolor=COLOR_INK, framealpha=1.0,
        fontsize=10.5, fancybox=True
    )

    plt.suptitle("AMD Opteron 8-Node NUMA Reproduction (100 Runs, 100% Passed)",
                 fontsize=14, fontweight='bold', color=COLOR_INK, y=1.04)
    plt.tight_layout()
    plt.subplots_adjust(top=0.88, bottom=0.12)

    out_file = REPO / "results" / "numa" / "fig7_reproduction.png"
    plt.savefig(out_file, dpi=200, bbox_inches='tight')
    print(f"Generated aesthetic NUMA chart: {out_file}")

def plot_tilepro64():
    raw_csv = REPO / "tilepro64" / "results" / "raw_results.csv"
    if not raw_csv.exists():
        print(f"Error: {raw_csv} not found")
        return

    rows = []
    with open(raw_csv, "r") as f:
        reader = csv.DictReader(f)
        for r in reader:
            rows.append(r)

    benchmarks = ["map", "vecmul"]
    configs = [
        ("ws+coarse", "WS + Coarse (Base)", "ws", "coarse"),
        ("ws+fine",   "WS + Fine",   "ws", "fine"),
        ("la+fine",   "LA + Fine",   "la", "fine"),
        ("la+coarse", "LA + Coarse (Dealer)", "la", "coarse"),
    ]

    comm_data = {b: [] for b in benchmarks}
    time_data = {b: [] for b in benchmarks}

    for b in benchmarks:
        for cfg_id, label, s, d in configs:
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

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5.8), facecolor=COLOR_PAPER)

    x = range(len(benchmarks))
    bar_w = 0.18

    for ax, data_map, title, y_label, scale, unit in [
        (ax1, comm_data, "Modeled Communication / Access Cost", "Million Cycles (Lower is Better)", 1e6, "M"),
        (ax2, time_data, "Simulated Execution Time (Makespan)", "Thousand Cycles (Lower is Better)", 1e3, "K")
    ]:
        ax.set_facecolor("#FFFFFF")
        ax.grid(axis='y', color=COLOR_GRID, linestyle='--', linewidth=1.0, zorder=0)

        for i, (cfg_id, lbl, _, _) in enumerate(configs):
            offset = (i - 1.5) * bar_w
            vals = [data_map[b][i] / scale for b in benchmarks]
            ax.bar(
                [pos + offset for pos in x], vals,
                width=bar_w, label=lbl, color=PALETTE[cfg_id],
                edgecolor=COLOR_INK, linewidth=1.5, zorder=3
            )

        ax.set_xticks(x)
        ax.set_xticklabels(["MAP\n(48-64 Tasks)", "VECMUL\n(Overlapping Tiles)"], fontsize=11, fontweight='bold', color=COLOR_INK)
        ax.set_ylabel(y_label, fontsize=11, fontweight='bold', color=COLOR_INK, labelpad=8)
        ax.set_title(title, fontsize=13, fontweight='bold', color=COLOR_INK, pad=12)
        ax.tick_params(colors=COLOR_INK, labelsize=10)

    # Callouts on Map LA+Coarse
    # Panel 1: -80.3% Comm Cost
    ax1.annotate(
        "-80.3% Comm Drop\n(1.31M vs 6.67M)",
        xy=(0 + 1.5 * bar_w, comm_data['map'][3] / 1e6),
        xytext=(0 + 0.3 * bar_w, 3.8),
        arrowprops=dict(facecolor=COLOR_INK, shrink=0.08, width=1.5, headwidth=6),
        fontsize=9, fontweight='bold', color='#047857',
        bbox=dict(boxstyle='round,pad=0.3', facecolor='#DCFCE7', edgecolor=COLOR_INK, linewidth=1.2)
    )

    # Panel 2: 39.3% Speedup
    ax2.annotate(
        "39.3% Speedup\n(0 Steals vs 63)",
        xy=(0 + 1.5 * bar_w, time_data['map'][3] / 1e3),
        xytext=(0 + 0.3 * bar_w, 360),
        arrowprops=dict(facecolor=COLOR_INK, shrink=0.08, width=1.5, headwidth=6),
        fontsize=9, fontweight='bold', color='#047857',
        bbox=dict(boxstyle='round,pad=0.3', facecolor='#DCFCE7', edgecolor=COLOR_INK, linewidth=1.2)
    )

    # Legend
    handles, lbls = ax1.get_legend_handles_labels()
    fig.legend(
        handles, lbls, loc='upper center', ncol=4,
        bbox_to_anchor=(0.5, 0.99), frameon=True,
        facecolor='#FFFFFF', edgecolor=COLOR_INK, framealpha=1.0,
        fontsize=10.5, fancybox=True
    )

    plt.suptitle("TILEPro64 8×8 Mesh Architecture Simulation (64 Cores)",
                 fontsize=14, fontweight='bold', color=COLOR_INK, y=1.04)
    plt.tight_layout()
    plt.subplots_adjust(top=0.88, bottom=0.12)

    out_file = REPO / "tilepro64" / "results" / "tilepro64_results.png"
    plt.savefig(out_file, dpi=200, bbox_inches='tight')
    print(f"Generated aesthetic TILEPro64 chart: {out_file}")

if __name__ == "__main__":
    plot_numa()
    plot_tilepro64()
