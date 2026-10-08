# Master Project Presentation Deck: Locality-Aware Task Scheduling on Heterogeneous HPC Architectures

**Project Title:** Locality-Aware Task Scheduling and Data Distribution on NUMA Systems & Manycore Processors  
**Team Project:** HPC & Runtime Systems Architecture  
**Reference Paper:** Muddukrishna, Jonsson, & Brorsson (Scientific Programming, 2015)  
**Deliverables Covered:** Full 8-Node AMD Opteron NUMA Reproduction + 64-Tile TILEPro64 2D Mesh Simulation + NOVA/ALLoC Scheduler Architecture

---

## Slide 1: Title & Executive Overview

### Visual Layout
```
┌────────────────────────────────────────────────────────────────────────┐
│  LOCALITY-AWARE TASK SCHEDULING ON MODERN HPC ARCHITECTURES             │
│  Dual-Architecture Empirical Study: NUMA Systems vs. Manycore Mesh    │
│                                                                        │
│  [AMD Opteron 8-Node NUMA]          vs.      [Tilera TILEPro64 Mesh]   │
│   Hierarchical Interconnect                   8×8 2D Grid Network      │
│   4×–6× Remote Memory Stalls                  Manhattan Hop Latency    │
└────────────────────────────────────────────────────────────────────────┘
```

### Key Talking Points
- **The Core Problem in Modern HPC:** As processor core counts scale, memory access is no longer uniform. Fetching data from distant RAM sockets or mesh tiles costs **4× to 6× more latency** than local cache access.
- **Why Naive Parallelism Fails:** Traditional work-stealing schedulers distribute tasks blindly for load balance, dragging cache lines across interconnects and saturating buses with stall cycles.
- **Dual Contributions of Our Project:**
  1. **NUMA Opteron Baseline Reproduction & NOVA Scheduler:** Reproduced the 2015 paper's Figure 7 across 5 benchmarks (100 runs); demonstrated that locality-only scheduling causes a **15.1% slowdown on SparseLU**, and designed the **NOVA** co-scheduler to fix it.
  2. **Standalone TILEPro64 Manycore Simulator:** Built an exact 64-tile 8×8 mesh software simulator with home-cache latency models, demonstrating an **80.3% communication cost reduction** and **39.3% execution speedup** on memory-bound Map workloads.

### Speaker Notes (Word-for-Word Script)
> "Good morning, everyone. In modern High-Performance Computing, computation is fast, but communication is expensive. On multi-socket servers and manycore processors, accessing memory on another node or tile takes 4 to 6 times longer than local memory. Our project reproduces and advances state-of-the-art locality-aware runtime scheduling. We examine two distinct architectural paradigms: multi-socket NUMA servers and 2D mesh manycores. We present complete empirical reproductions of both architectures from Muddukrishna et al.'s seminal 2015 work, and present our NOVA scheduler architecture that addresses the fundamental trade-off between memory locality and core load balance."

---

## Slide 2: Architectural Paradigms: NUMA vs. Manycore Mesh

### Comparative Architecture Table
| Architectural Dimension | 8-Node AMD Opteron (NUMA) | 64-Tile TILEPro64 (Manycore) |
|---|---|---|
| **Topology** | 8 NUMA nodes (HyperTransport interconnect) | 8 × 8 2D mesh grid network |
| **Cores per Unit** | 3–6 cores per node (24–48 total cores) | 1 core per tile (64 total cores) |
| **Memory Model** | Discrete per-node physical DRAM banks | Distributed shared L2 cache partitioned into 64 banks |
| **Local Latency** | ~40 cycles (local node RAM) | 10 cycles (local tile L2 cache bank) |
| **Remote Latency** | 240 – 340 cycles (inter-socket bus) | 38 cycles base + 2 cycles per Manhattan mesh hop |
| **Diameter** | 3 hops | 14 hops ($|x_1 - x_2| + |y_1 - y_2|$) |

### Architectural Insights
- **NUMA Systems:** Discrete memory controllers per socket. Memory distance is determined by interconnect hops between sockets.
- **Manycore Processors:** Every tile contains a slice of the shared L2 cache called its **Home Cache**. Data lines are assigned to a home cache via **Fine** (cache-line striped) or **Coarse** (per-buffer block) distribution.

### Speaker Notes
> "To understand locality scheduling, we must understand the hardware models. On the left is the AMD Opteron NUMA architecture: cores share local DRAM on a socket, and remote requests travel over HyperTransport links costing up to 340 cycles. On the right is the TILEPro64 manycore: 64 independent tiles arranged in an 8-by-8 mesh. Each tile has its own L1 and a 64-kilobyte slice of the shared L2 cache called its 'home cache.' Remote accesses travel through the mesh network with latency scaling directly with Manhattan distance: 38 cycles base plus 2 cycles for every hop."

---

## Slide 3: Locality-Aware Task Scheduling Mechanisms

### Visual Workflow
```
[Task Created with Data Footprint]
                 │
                 ▼
    [Significance Gate Check]
    Footprint > LLC / Cores (64 KB)?
       ├── NO  ──> [Local Execution on Creator Core]
       └── YES ──> [Algorithm 3: Locality Work Dealing]
                         │
                         ▼
             Evaluate Candidate Tiles:
             cost(X) = Σ (D[i] / line_size) × latency(X, i)
                         │
                         ▼
             Route to argmin(cost) Node/Tile Queue
                         │
                         ▼
    [Worker Idle? Vicinity-Bounded Stealing]
    Search concentric Manhattan rings d ∈ [1, Vicinity Radius]
    Steal ONLY from eligible nearby queues to protect locality!
```

### Key Algorithmic Components
1. **Footprint Vector ($D$):** Captures data distribution across all nodes or home caches ($D[0..63]$).
2. **Significance Gating:** Prevents scheduling overhead on tiny tasks that fit entirely in L1/L2 cache.
3. **Locality-Aware Work Dealing:** Places tasks directly on the node or tile homing the majority of the data.
4. **Vicinity-Bounded Stealing:** Restricts idle workers to search only within a fixed hop radius, preventing them from stealing distant tasks that would ruin cache locality.

### Speaker Notes
> "How does the scheduler exploit this topology? When an OpenMP task is created, the runtime examines its memory footprint vector D. If the task is small, it executes locally. But if its footprint exceeds cache capacity, the locality dealer evaluates the access cost across all 64 tiles and routes the task directly to the tile that minimizes latency. Furthermore, when a worker core runs out of work, it doesn't steal randomly. It uses a vicinity search: checking only neighboring queues within a fixed Manhattan radius. This prevents threads from pulling remote tasks that cause catastrophic bus congestion."

---

## Slide 4: Empirical Results & The "SparseLU Paradox"

### 1. AMD Opteron 8-Node NUMA Reproduction (`results/numa/matrix.csv`)
- **100 Benchmark Runs** (Map, Matmul, Reduction, Jacobi, SparseLU × 4 configs × 5 reps, 100% correctness `check_ok=1`):
  - **Map:** Communication cost down **45.1%** (0.549×); execution cycles down **19.0%** (0.810×).
  - **Matmul:** Communication cost down **22.5%** (0.775×); execution time parity.
  - **The SparseLU Paradox:** While modeled communication cost looked **26.7% better (0.733×)**, real execution time was **15.1% SLOWER (1.151× cycles)** than plain work-stealing!
  - **Root Cause:** In irregular DAGs, locality-only dealing funnels tasks onto one overloaded queue, leaving other cores idle.

### 2. TILEPro64 Manycore Simulation (`tilepro64/results/raw_results.csv`)
- **56 Experimental Configurations** on 8×8 Mesh:
  - **Map:** `LA + Coarse` achieves **1.31M cycles communication cost vs. 6.67M for Work-Stealing** (**80.3% reduction**).
  - **Simulated Execution Time:** Drops from **281K cycles to 170K cycles** (**39.3% speedup**).
  - **Steal Elimination:** 0 steals required under `LA + Coarse` versus 63 steals under Work-Stealing.

### Speaker Notes
> "Here are our key empirical findings. On regular workloads like Map, locality scheduling is a clear winner: communication costs drop by 45% on NUMA and 80% on TILEPro64, resulting in a 19% to 39% execution time speedup. However, on irregular workloads like SparseLU, we uncovered a fascinating result: the 'SparseLU Paradox.' Under locality-aware scheduling, SparseLU's communication cost drops by 26%, yet its real execution time is 15% SLOWER than naive work-stealing. Why? Because the dealer placed tasks purely for locality, ignoring queue length and piling all work onto one core while 23 other cores sat idle. This proves that locality without load balance is fundamentally flawed."

---

## Slide 5: The NOVA Solution & Key Conclusions

### The NOVA / ALLoC Innovation
To fix the SparseLU paradox, we developed the **NOVA** architecture:
1. **N-A: Load-Aware Work Dealing:**
   $$\text{score}(q) = \alpha \cdot \text{comm\_norm}(D, q) + (1 - \alpha) \cdot \text{occ\_norm}(q)$$
   Dynamically blends communication cost with queue backlog to prevent queue clumping.
2. **N-B: Task-Level Locality Stealing (Shadow Index):**
   Idle cores peek lock-free at $k=8$ tasks in victim queues and steal the task matching their local memory.
3. **N-C: Adaptive Vicinity Radius:**
   Workers dynamically expand or shrink their search radius based on an EWMA of steal success rates.
4. **Zero-Contention Control Plane:**
   Double-buffered statistics coordinator swaps pointers every 64 ops, keeping the hot path lock-free.

### Summary of Project Outcomes
- **Dual Reproduction:** Successfully reproduced both the Opteron NUMA baseline and the 64-tile TILEPro64 manycore mesh simulator.
- **Automated Toolchains:** Automated Python test harnesses and publication-grade visualization scripts for both architectures.
- **Empirical Contribution:** Identified the exact queue-imbalance pathology of locality-only scheduling and delivered the complete systems specification for the NOVA co-scheduler.

### Speaker Notes
> "In conclusion, we addressed the core flaw of locality schedulers by designing NOVA: an adaptive co-scheduler that balances data proximity against queue backlog. NOVA replaces static blind placement with an alpha-blended score, introduces a lock-free Shadow Index for smart stealing, and makes the vicinity radius self-adjusting. Across both NUMA servers and manycore processors, our reproductions and architectures demonstrate that co-scheduling memory locality and core load is essential for achieving scalable HPC performance. Thank you, and we welcome your questions."

---

## Anticipated Examiner / Professor Q&A

**Q1: Why did you build a standalone TILEPro64 simulator instead of running on real hardware?**  
*Answer:* TILEPro64 processors were discontinued specialty hardware. A software model that accurately implements the 8×8 mesh, 2D Manhattan latency matrix (10 cycles local, 38+2·hop remote), and home-cache distribution allows deterministic, reproducible evaluation of the paper's scheduling algorithms without hardware drift.

**Q2: Why does work-stealing have 63 steals in Map while LA+Coarse has 0 steals?**  
*Answer:* In work-stealing, the initial thread (Tile 0) creates and enqueues all 64 tasks onto its own queue. The other 63 worker cores start with empty queues and must each perform a remote steal across the mesh network. Under `LA + Coarse`, the work dealer uses Algorithm 3 to immediately place each task on the tile that homes its data, distributing work evenly from the start with zero steal overhead.

**Q3: How does NOVA prevent synchronization overhead on the critical path?**  
*Answer:* NOVA decouples the fast data plane from the slow control plane. Workers make dealing and stealing decisions in $O(\text{nodes})$ time without locks. Global queue snapshots and $\alpha$ tuning occur once every 64 operations in a double-buffer, swapped via an atomic pointer CAS, guaranteeing zero lock contention on the hot execution path.
