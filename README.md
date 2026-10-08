# Locality-Aware Task Scheduling on Heterogeneous HPC Architectures

[![Build Status](https://img.shields.io/badge/tests-11%2F11%20passing-brightgreen)](#)
[![Architectures](https://img.shields.io/badge/architectures-NUMA%20%7C%20TILEPro64-blue)](#)
[![Paper](https://img.shields.io/badge/reference-Scientific%20Programming%202015-orange)](https://doi.org/10.1155/2015/981759)

A comprehensive high-performance computing (HPC) research codebase implementing and evaluating locality-aware task scheduling and data distribution across two distinct architectural paradigms:
1. **Multi-Socket NUMA Systems (AMD Opteron 8-Node Topology)** + The **NOVA / ALLoC** Co-Scheduler.
2. **2D Mesh Manycore Processors (Tilera TILEPro64 8×8 Mesh)** + The **TILEPro64 Locality Simulator**.

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

### The NOVA / ALLoC Solution (`patches/nova/`)
To resolve the SparseLU paradox, we developed the **NOVA** co-scheduler:
- **N-A (Load-Aware Dealing):** Continuous composite score $\text{score}(q) = \alpha \cdot \text{comm\_norm} + (1 - \alpha) \cdot \text{occ\_norm}$ balancing data proximity with queue backlog.
- **N-B (Task-Level Locality Stealing):** Lock-free Shadow Index ring buffer ($k=8$) allowing thieves to peek at waiting tasks and steal those matching their local memory.
- **N-C (Adaptive Steal Radius):** Per-worker EWMA dynamically adjusts search reach to prevent starvation without flooding buses.
- **Zero Lock Contention:** Double-buffered control plane coordinator swaps statistics snapshots every 64 ops.

---

## 2. TILEPro64 Manycore Simulation (`tilepro64/`)

A standalone C simulator modeling the 64-tile 8×8 2D mesh network:
- **Latencies:** 10 cycles local L2 access, 38 cycles remote base + 2 cycles per Manhattan mesh hop.
- **Work Dealing & Stealing:** Algorithm 3 locality routing with 64 KB significance threshold and vicinity-restricted stealing.
- **Test Suite:** `cd tilepro64 && make test` passes **11 out of 11 validation tests**.
- **Results (`tilepro64/results/`):**
  - **Map:** `LA + Coarse` achieves an **80.3% reduction in communication cost** (1.31M vs. 6.67M cycles) and a **39.3% execution speedup** (170.5K vs. 281.1K cycles) over work-stealing.

---

## 3. Academic Presentation & Deliverables

- **Interactive Slide Deck:** Open [docs/presentation.html](docs/presentation.html) in your browser for a full-screen Reveal.js presentation (press **`S`** for presenter notes).
- **Master Slide Script & Q&A:** View [docs/presentation_deck.md](docs/presentation_deck.md) for word-for-word presenter scripts and examiner Q&A answers.

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