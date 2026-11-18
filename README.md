# Locality-Aware Task Scheduling on Heterogeneous HPC Architectures

[![Build Status](https://img.shields.io/badge/tests-11%2F11%20tilepro%20%7C%202%2F2%20nova-brightgreen)](#)
[![Architectures](https://img.shields.io/badge/architectures-NUMA%20%7C%20TILEPro64%20%7C%20gem5-blue)](#)
[![Paper](https://img.shields.io/badge/reference-Scientific%20Programming%202015-orange)](https://doi.org/10.1155/2015/981759)

A comprehensive high-performance computing (HPC) research codebase implementing, evaluating, and extending locality-aware task scheduling and data distribution across two distinct architectural paradigms:
1. **Multi-Socket NUMA Systems (AMD Opteron 8-Node Topology)** + The **NOVA / ALLoC** Co-Scheduler.
2. **2D Mesh Manycore Processors (Tilera TILEPro64 8×8 Mesh)** + The **TILEPro64 Analytical Simulator** and Staged **gem5 / Garnet 3.0** Migration.

Based on the research paper:
> **"Locality-Aware Task Scheduling and Data Distribution for OpenMP Programs on NUMA Systems and Manycore Processors"**  
> *Ananya Muddukrishna, Peter A. Jonsson, Mats Brorsson (Scientific Programming, 2015)*

---

## 0. Experimental Measurement Taxonomy

To ensure scientific precision, this repository distinguishes between four separate tiers of metrics:

| Tier | Measurement Type | Source | Description |
| :--- | :--- | :--- | :--- |
| **Tier 1** | **Measured Host Execution Time** | Hardware timers / OS `clock_gettime` | Wall-clock execution cycles measured directly on physical or emulated host CPUs during benchmark execution. |
| **Tier 2** | **Modeled Communication Cost** | Footprint matrices $\sum \text{bytes} \times \text{cost}$ | Algorithmic communication cost derived from task memory distributions and NUMA/Mesh hop distance penalty tables. |
| **Tier 3** | **Analytical Simulation Output** | `tilepro64/` standalone C model | Discrete-event analytical calculation modeling Manhattan mesh hops ($38 + 2 \times \text{hops}$) and idealized cache hit rates. |
| **Tier 4** | **Cycle-Accurate gem5 Output** | `gem5.opt` (Garnet 3.0 + Ruby) | Microarchitectural simulation capturing router contention, buffer backpressure, flit serialization, and MESI coherence transactions. |

---

## Repository Structure

```text
.
├── benchmarks/
│   ├── numa/                   # AMD Opteron NUMA Figure 7 benchmark kernels (Map, Matmul, etc.)
│   └── tilepro64_gem5/         # SE-mode gem5 TILEPro64 benchmarks with m5ops annotations
├── docs/
│   ├── implementation-log.md   # Sprint implementation log and verified architectural findings
│   ├── tilepro64_to_gem5_migration_plan.md # gem5 staged migration specification (v3.0)
│   ├── presentation.html       # Interactive full-screen Reveal.js presentation (5 slides + speaker notes)
│   └── presentation_deck.md    # Master presentation markdown script & examiner Q&A
├── harness/
│   ├── run_baseline_matrix.py  # Strictly runs the 100-run baseline NUMA matrix (5 benches x 4 configs x 5 reps)
│   ├── run_nova_matrix.py      # Runs NOVA evaluation and ablation matrix (NOVA, No-Shadow, Fixed-Alpha)
│   ├── run_matrix.py           # Canonical runner redirecting to verified baseline
│   └── plot_fig7.py            # Generates Figure 7 reproduction chart
├── patches/
│   ├── nova/                   # Standalone C source files for the NOVA / ALLoC scheduler extension
│   │   ├── mir_sched_pol_nova.c # MIR scheduler policy implementation
│   │   ├── nova_coordinator.c  # Double-buffered coordinator with EWMA smoothing and rwlock protection
│   │   ├── nova_dealer.c       # Load-aware composite scoring dealer (locality + queue backlog)
│   │   ├── nova_stealer.c      # Authoritative task-claiming stealer with queue removal
│   │   └── nova_shadow_index.c # Concurrency-safe auxiliary shadow index with entry eviction
│   └── nova-mir-dev.patch      # Patch for the upstream MIR runtime (includes task queue lock & remove)
├── results/
│   ├── numa/                   # Canonical historical 100-run NUMA baseline results
│   │   ├── matrix.csv          # 100 empirical reproduction runs (100% check_ok=1)
│   │   └── fig7_reproduction.png # Two-panel chart (modelled comm cost vs. real cycles)
│   └── nova_evaluation/        # NOVA evaluation and ablation output matrix
├── tests/
│   ├── test_nova_queue_race.c  # Deterministic 2-thief 1-task queue race test suite (6,000 runs)
│   ├── test_nova_concurrency.c # Full multi-threaded stress test (40,000 tasks, dealers/workers/thieves)
│   └── Makefile                # Test automation for NOVA correctness
├── tilepro64/                  # Standalone TILEPro64 Analytical Simulator
│   ├── src/                    # C architecture model, home-cache latency, and scheduler
│   ├── tests/                  # Section 21 validation test suite (11/11 passing)
│   ├── benchmarks/             # Map and Vecmul workloads
│   ├── results/
│   │   ├── raw_results.csv     # 56 experimental runs across WS, LA, Coarse, Fine, Vicinity
│   │   └── tilepro64_results.png # Grouped-bar publication chart
│   └── README.md               # Detailed analytical simulator specification
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
- **N-B (Authoritative Task Claiming & Safe Shadow Index):** Auxiliary ring buffer ($k=8$) allowing thieves to locate locality-friendly tasks, paired with atomic claiming (`taken` CAS) and physical removal from authoritative task queues (`mir_task_queue_lock_and_remove`), completely preventing duplicate concurrent executions.
- **N-C (Adaptive Steal Radius & EWMA):** True exponentially weighted moving average (EWMA, $\beta=0.7$) tracks recent steal dynamics across epochs, dynamically adjusting vicinity radius without bus flooding.
- **Synchronized Control Plane:** Double-buffered coordinator context protected by reader-writer locks (`pthread_rwlock_t`) and atomic monotonic counters.

---

## 2. TILEPro64 Manycore Simulation (`tilepro64/`)

> [!NOTE]
> The TILEPro64 implementation in `tilepro64/` is an **analytical simulator** modeling the 64-tile 8×8 2D mesh network:
- **Latencies:** 10 cycles local L2 access, 38 cycles remote base + 2 cycles per Manhattan mesh hop.
- **Work Dealing & Stealing:** Algorithm 3 locality routing with 64 KB significance threshold and vicinity-restricted stealing.
- **Validation Suite:** `cd tilepro64 && make test` passes **11 out of 11 validation tests**.
- **Results (`tilepro64/results/`):**
  - **Map:** `LA + Coarse` achieves an **80.3% reduction in communication cost** (1.31M vs. 6.67M cycles) and a **39.3% execution speedup** (170.5K vs. 281.1K cycles) over work-stealing.

---

## 3. gem5 / Garnet 3.0 Cycle-Accurate Migration

For cycle-accurate microarchitectural evaluation, a staged migration path is defined in [`docs/tilepro64_to_gem5_migration_plan.md`](docs/tilepro64_to_gem5_migration_plan.md):
- **Topology:** Custom 8×8 mesh (`gem5/configs/topologies/TilePro64_Mesh.py`) with 64 Garnet routers, 64 L1/L2 caches, and 4 perimeter memory controllers.
- **Verification:** Tested with gem5 `v23.0.1.0` (X86_MESI_Two_Level) running 64-core Ruby random tests in Garnet 3.0.
- **Microbenchmarks:** `calibrate_latency.c` for latency curve fitting, and `bench_gem5.c` with m5ops ROI markers.

---

## Quickstart

```bash
# 1. Run NOVA Queue Race and Concurrency Stress Tests
make -f tests/Makefile test

# 2. Run TILEPro64 analytical simulation validation suite
cd tilepro64 && make test

# 3. Run TILEPro64 experiment matrix and regenerate charts
make results

# 4. View presentation
open ../docs/presentation.html
```
