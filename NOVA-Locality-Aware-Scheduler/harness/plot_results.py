#!/usr/bin/env python3
"""Plot benchmark comparison: Baselines (ws, la) vs NOVA Co-Scheduler.
Generates two-panel Figure 7-style chart and ablation analysis chart.
"""
import os
import sys
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
RESULTS_DIR = REPO / "results" / "numa"
CSV_PATH = RESULTS_DIR / "matrix.csv"

if not CSV_PATH.exists():
    print(f"Error: {CSV_PATH} not found!")
    sys.exit(1)

df = pd.read_csv(CSV_PATH)
df['exec_cycles'] = pd.to_numeric(df['exec_cycles'])
df['comm_cost_avg'] = pd.to_numeric(df['comm_cost_avg'])
df['tasks_stolen'] = pd.to_numeric(df['tasks_stolen'])
df['tasks_owned'] = pd.to_numeric(df['tasks_owned'])

benchmarks = ["map", "matmul", "reduction", "jacobi", "sparselu"]
bench_titles = {
    "map": "Map (Independent)",
    "matmul": "Matmul (Blocked)",
    "reduction": "Reduction (Shared Slice)",
    "jacobi": "Jacobi (2D Stencil)",
    "sparselu": "SparseLU (Dataflow Reuse)"
}

# Group and compute means
grouped = df.groupby(['benchmark', 'scheduler_policy', 'dist_policy']).agg({
    'exec_cycles': ['mean', 'std'],
    'comm_cost_avg': ['mean', 'std'],
    'tasks_stolen': ['mean', 'std'],
    'tasks_owned': ['mean', 'std']
}).reset_index()

grouped.columns = ['benchmark', 'scheduler', 'dist', 
                   'cycles_mean', 'cycles_std', 
                   'comm_mean', 'comm_std',
                   'stolen_mean', 'stolen_std',
                   'owned_mean', 'owned_std']

# Compute normalization against ws+coarse
norm_data = []
for b in benchmarks:
    ws_coarse = grouped[(grouped['benchmark'] == b) & 
                        (grouped['scheduler'] == 'ws') & 
                        (grouped['dist'] == 'coarse')]
    base_cycles = ws_coarse['cycles_mean'].values[0]
    base_comm = ws_coarse['comm_mean'].values[0]

    for idx, row in grouped[grouped['benchmark'] == b].iterrows():
        norm_cycles = row['cycles_mean'] / base_cycles
        norm_comm = row['comm_mean'] / base_comm
        norm_data.append({
            'benchmark': b,
            'scheduler': row['scheduler'],
            'dist': row['dist'],
            'config': f"{row['scheduler']}+{row['dist']}",
            'cycles_mean': row['cycles_mean'],
            'cycles_norm': norm_cycles,
            'cycles_std': row['cycles_std'],
            'comm_mean': row['comm_mean'],
            'comm_norm': norm_comm,
            'comm_std': row['comm_std'],
            'stolen_mean': row['stolen_mean'],
            'owned_mean': row['owned_mean']
        })

norm_df = pd.DataFrame(norm_data)
summary_path = RESULTS_DIR / 'summary.csv'
norm_df.to_csv(summary_path, index=False)
print(f"Summary metrics saved to {summary_path}")

# ----------------- PLOT 1: Figure 7 Comparison (WS, LA, NOVA) -----------------
main_configs = [
    ('ws', 'coarse', 'ws+coarse', '#888888'),
    ('ws', 'fine', 'ws+fine', '#bbbbbb'),
    ('la', 'coarse', 'la+coarse', '#d95f02'),
    ('la', 'fine', 'la+fine', '#fdbf6f'),
    ('nova', 'coarse', 'nova+coarse', '#1b9e77'),
    ('nova', 'fine', 'nova+fine', '#7570b3'),
]

plt.style.use('seaborn-v0_8-whitegrid' if 'seaborn-v0_8-whitegrid' in plt.style.available else 'default')
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 6))

x = np.arange(len(benchmarks))
width = 0.13
offsets = np.linspace(-width * 2.5, width * 2.5, len(main_configs))

for i, (sched, dist, label, color) in enumerate(main_configs):
    comm_vals = []
    cycle_vals = []
    for b in benchmarks:
        match = norm_df[(norm_df['benchmark'] == b) & (norm_df['scheduler'] == sched) & (norm_df['dist'] == dist)]
        if not match.empty:
            comm_vals.append(match['comm_norm'].values[0])
            cycle_vals.append(match['cycles_norm'].values[0])
        else:
            comm_vals.append(0)
            cycle_vals.append(0)
    
    ax1.bar(x + offsets[i], comm_vals, width, label=label, color=color, edgecolor='black', linewidth=0.5)
    ax2.bar(x + offsets[i], cycle_vals, width, label=label, color=color, edgecolor='black', linewidth=0.5)

ax1.set_title("Modelled Communication Cost (Normalized to ws+coarse)", fontsize=13, fontweight='bold', pad=12)
ax1.set_ylabel("Normalized Communication Cost (lower is better)", fontsize=11)
ax1.set_xticks(x)
ax1.set_xticklabels([bench_titles[b] for b in benchmarks], rotation=15, ha='right', fontsize=10)
ax1.axhline(1.0, color='gray', linestyle='--', linewidth=1, alpha=0.7)
ax1.legend(loc='upper right', frameon=True, fontsize=9)
ax1.grid(True, linestyle=':', alpha=0.6)

ax2.set_title("Real Execution Cycles (Normalized to ws+coarse)", fontsize=13, fontweight='bold', pad=12)
ax2.set_ylabel("Normalized Execution Cycles (lower is better)", fontsize=11)
ax2.set_xticks(x)
ax2.set_xticklabels([bench_titles[b] for b in benchmarks], rotation=15, ha='right', fontsize=10)
ax2.axhline(1.0, color='gray', linestyle='--', linewidth=1, alpha=0.7)
ax2.legend(loc='upper right', frameon=True, fontsize=9)
ax2.grid(True, linestyle=':', alpha=0.6)

plt.tight_layout()
fig_out = RESULTS_DIR / 'fig7_nova_comparison.png'
plt.savefig(fig_out, dpi=300)
plt.close()
print(f"Figure saved to {fig_out}")

# ----------------- PLOT 2: NOVA Additive Ablation Study -----------------
ablation_configs = [
    ('ws', 'coarse', 'Baseline (ws)', '#999999'),
    ('la', 'coarse', 'Base Locality (la)', '#d95f02'),
    ('nova-a', 'coarse', '+N-A (Load Dealer)', '#386cb0'),
    ('nova-ab', 'coarse', '+N-A+N-B (+Shadow Steal)', '#a6cee3'),
    ('nova', 'coarse', '+N-A+N-B+N-C (Full NOVA)', '#1b9e77'),
]

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 5.5))
offsets_abl = np.linspace(-width * 2, width * 2, len(ablation_configs))

for i, (sched, dist, label, color) in enumerate(ablation_configs):
    comm_vals = []
    cycle_vals = []
    for b in benchmarks:
        match = norm_df[(norm_df['benchmark'] == b) & (norm_df['scheduler'] == sched) & (norm_df['dist'] == dist)]
        if not match.empty:
            comm_vals.append(match['comm_norm'].values[0])
            cycle_vals.append(match['cycles_norm'].values[0])
        else:
            comm_vals.append(0)
            cycle_vals.append(0)
    
    ax1.bar(x + offsets_abl[i], comm_vals, width, label=label, color=color, edgecolor='black', linewidth=0.5)
    ax2.bar(x + offsets_abl[i], cycle_vals, width, label=label, color=color, edgecolor='black', linewidth=0.5)

ax1.set_title("NOVA Additive Ablation: Communication Cost", fontsize=12, fontweight='bold', pad=10)
ax1.set_ylabel("Normalized Comm Cost", fontsize=10)
ax1.set_xticks(x)
ax1.set_xticklabels([b.upper() for b in benchmarks], fontsize=10)
ax1.axhline(1.0, color='gray', linestyle='--', linewidth=0.8)
ax1.legend(loc='upper right', frameon=True, fontsize=9)
ax1.grid(True, linestyle=':', alpha=0.6)

ax2.set_title("NOVA Additive Ablation: Execution Cycles", fontsize=12, fontweight='bold', pad=10)
ax2.set_ylabel("Normalized Execution Cycles", fontsize=10)
ax2.set_xticks(x)
ax2.set_xticklabels([b.upper() for b in benchmarks], fontsize=10)
ax2.axhline(1.0, color='gray', linestyle='--', linewidth=0.8)
ax2.legend(loc='upper right', frameon=True, fontsize=9)
ax2.grid(True, linestyle=':', alpha=0.6)

plt.tight_layout()
abl_out = RESULTS_DIR / 'nova_ablation_study.png'
plt.savefig(abl_out, dpi=300)
plt.close()
print(f"Ablation figure saved to {abl_out}")
