# Locality-Aware Task Scheduling on Heterogeneous HPC Architectures

[![Build Status](https://img.shields.io/badge/tests-15%2F15%20passing-brightgreen)](#)
[![MIR Probe](https://img.shields.io/badge/MIR%20probe-400%2F400%20shuffled-brightgreen)](#)
[![Architectures](https://img.shields.io/badge/architectures-NUMA%20%7C%20TILEPro64-blue)](#)
[![Paper](https://img.shields.io/badge/reference-Scientific%20Programming%202015-orange)](https://doi.org/10.1155/2015/981759)

A comprehensive high-performance computing (HPC) research codebase implementing and evaluating locality-aware task scheduling and data distribution across two distinct architectural paradigms:
1. **Multi-Socket NUMA Systems (`NOVA-Locality-Aware-Scheduler/` + `results/numa/`)** — **NOVA / ALLoC** Co-Scheduler on MIR HAL-Sim (AMD Opteron 8-Node Topology).
2. **2D Mesh Manycore Processors (`tilepro64/`)** — Analytical 64-tile 8×8 Mesh Simulator resolving hotspots **H8, H9, H10a, and H10b**.

Based on the research paper:
> **"Locality-Aware Task Scheduling and Data Distribution for OpenMP Programs on NUMA Systems and Manycore Processors"**  
> *Ananya Muddukrishna, Peter A. Jonsson, Mats Brorsson (Scientific Programming, 2015)*

---

## Repository Structure

```text
.
├── benchmarks/
│   └── numa/                   # AMD Opteron NUMA Figure 7 benchmark kernels (Map, Matmul, etc.)
├── build/                      # Precompiled benchmark binaries
├── docs/
│   ├── presentation.html       # Interactive full-screen Reveal.js presentation (5 slides + speaker notes)
│   └── presentation_deck.md    # Master presentation markdown script & examiner Q&A
├── harness/
│   ├── run_matrix.py           # Automated test harness for 100-run Opteron benchmark matrix
│   └── plot_fig7.py            # Generates Figure 7 reproduction chart
├── patches/
│   ├── nova/                   # Standalone C source files for the NOVA / ALLoC scheduler extension
│   └── nova-mir-dev.patch      # Patch for the upstream MIR runtime
├── results/
│   └── numa/
│       ├── matrix.csv          # 100 empirical reproduction runs (100% check_ok=1)
│       └── fig7_reproduction.png # Two-panel chart (modelled comm cost vs. real cycles)
├── scripts/
│   └── generate_pptx.py        # Presentation builder and artifact validator
├── tilepro64/                  # Standalone TILEPro64 Manycore Simulator
│   ├── src/                    # C architecture model, home-cache latency, and scheduler
│   ├── tests/                  # Section 21 validation test suite (11/11 passing)
│   ├── benchmarks/             # Map and Vecmul workloads
│   ├── results/
│   │   ├── raw_results.csv     # 56 experimental runs across WS, LA, Coarse, Fine, Vicinity
│   │   └── tilepro64_results.png # Grouped-bar publication chart
│   ├── scripts/                # Experiment runner & visualization scripts
│   ├── Makefile                # Build, test, and results automation
│   └── README.md               # Detailed TILEPro64 specification & documentation
└── vendor/
    └── mir-dev/                # Submodule pointing to upstream MIR runtime (anamud/mir-dev)
```

---

## 1. AMD Opteron NUMA Reproduction & Results

### The Baseline Reproduction (`results/numa/`)
We reproduced the paper's Figure 7 across 5 computational kernels (Map, Matmul, Reduction, Jacobi, SparseLU) over 4 scheduler and distribution configurations (`ws+coarse`, `ws+fine`, `la+coarse`, `la+fine`) with 5 repetitions (100 total runs) on a simulated 8-node AMD Opteron topology:

- **Map:** Communication cost drops **45.1%** (0.549×); execution cycles speed up by **19.0%** (0.810×).
- **Matmul:** Communication cost drops **22.5%** (0.775×) with execution cycle parity.
- **The SparseLU Paradox:** While modeled communication cost drops **26.7%** (0.733×), real execution time is **15.1% SLOWER** (1.151× cycles) under locality-aware scheduling compared to simple work-stealing!
  - *Root Cause:* In irregular task DAGs, locality-only dealing funnels tasks onto an already overloaded queue, leaving other cores idle.

### The NOVA / ALLoC Solution (`NOVA-Locality-Aware-Scheduler/` & `patches/nova/`)
To resolve H1–H4 and MIR defects D1–D3 (including the SparseLU imbalance), we implemented the **NOVA** co-scheduler (`nova-a`, `nova-ab`, `nova`) inside MIR using a Control/Data Plane split:
- **N-A (Load-Aware Dealing):** Composite score $\mathrm{score}(q) = \alpha \cdot \mathrm{comm}_{\mathrm{norm}} + (1 - \alpha) \cdot \mathrm{occ}_{\mathrm{norm}}$ balancing proximity with queue backlog (rebalancing SparseLU Node 0 from `187.7/16.3` → `112.6/91.4` owned/stolen).
- **N-B (Task-Level Locality Stealing):** $K=8$ Shadow Index ring buffer (with seqlock generation counters & fences) letting thieves steal locally cheap tasks—slashing Reduction comm cost **−41.3%** and Jacobi **−42.7%**.
- **N-C (Adaptive Steal Radius):** Per-worker EWMA dynamically scales search reach while folding in distance-ordered stealing (D3).
- **400-Run Shuffled Probe & Honest Trade-Off:** Across 400 shuffled runs (10 reps), modeled communication cost drops **−16.7% to −44.3%** across all 5 benchmarks, while single-socket HAL-Sim exposes NOVA's real CPU bookkeeping cost (`+1.4%` to `+67.5%`) since host execution has no cross-socket latency feedback. Full details in [`NOVA-Locality-Aware-Scheduler/README.md`](NOVA-Locality-Aware-Scheduler/README.md).

---

## 2. TILEPro64 Manycore Simulation (`tilepro64/`)

An analytical C simulator modeling the 64-tile 8×8 2D mesh network (`10` cycles local L2, `38 + 2·hops` remote):
- **Validation Suite:** `cd tilepro64 && make test` passes **15 out of 15 validation checks**.
- **Baseline (`Map`):** `LA + Coarse` cuts modeled communication cost **−80.3%** (`6.67M → 1.31M`) and speeds up makespan **39.3%** (`281.1K → 170.5K`) with **0 steals**.
- **Hotspots H8–H10b Resolved (`tilepro64/results/`):**
  - **H8 (Adaptive Migration `AM`):** Break-even chunk re-homing cuts comm cost **−82%** and total work **−12.6%** on corner-trapped reuse traces.
  - **H9 (Bisection Contention):** 112-link XY arbitration proves priority scheduling cuts queue wait (`1,528 → 48`), while no-contention baselines miss a **5.4×** worst-case bottleneck.
  - **H10a (2D Stencil Placement):** Coordinate-preserving `2d_block` vs `1d_striped` slashes comm cost **−51.0%**, center-bisection traffic **−93.8% (16×)**, and makespan **2.83×** with 0 steals.
  - **H10b (14-Hop Corner Drag):** Idle-aged ETA stealing (`dp`) cuts restrictive 2-hop LA makespan by **64%** (`133.2K → 47.7K`) while reducing steal distance vs WS (`5.61` vs `7.11` hops).

---

## 3. Academic Presentation & Deliverables

- **Print-Ready 7-Slide Presentation PDF:** [`output/HPC_Locality_Scheduling_Presentation.pdf`](output/HPC_Locality_Scheduling_Presentation.pdf) (built from [`docs/presentation_print.html`](docs/presentation_print.html)).
- **Interactive Slide Deck:** Open [`docs/presentation.html`](docs/presentation.html) in your browser (`S` for speaker notes).

---

## Quickstart

```bash
# 1. Run TILEPro64 validation suite
cd tilepro64
make test

# 2. Run TILEPro64 experiment matrix and regenerate charts
make results

# 3. View presentation
open ../docs/presentation.html
```
