# TILEPro64 Manycore Locality-Aware Task Scheduler & Architecture Model

A standalone C-based architectural model and locality-aware task scheduling simulator faithfully reproducing the **TILEPro64 manycore processor** portion of the research paper:

> **"Locality-Aware Task Scheduling and Data Distribution for OpenMP Programs on NUMA Systems and Manycore Processors"**  
> *Ananya Muddukrishna, Peter A. Jonsson, Mats Brorsson (Scientific Programming, 2015)*

---

## 1. Scope & Target Architecture
- **Target Architecture:** TILEPro64 manycore processor exclusively (8 × 8 2D mesh network of 64 tiles).
- **Excluded by Design:** NUMA multi-socket systems, AMD Opteron experiments, `libnuma`, physical node allocation, and NUMA-specific Algorithms 1 & 2 are strictly excluded to isolate manycore architectural mechanics.

---

## 2. Architectural Representation (8 × 8 2D Mesh)
The simulated processor comprises 64 identical tiles arranged as an 8 × 8 grid:
- **Tile Coordinates:** Tile $i$ occupies coordinate $(x, y) = (i \pmod 8, \lfloor i / 8 \rfloor)$ where $x, y \in [0, 7]$.
- **Interconnect:** 2D mesh network with Manhattan hop distance:
  $$\text{hops}(i, j) = |x_i - x_j| + |y_i - y_j| \quad (\text{maximum diameter } = 14 \text{ hops})$$
- **Cache Hierarchy:**
  - Private L1 cache per core (16 KB).
  - Distributed shared L2 cache partitioned into 64 banks (64 KB per tile bank, 4 MB total aggregate L2).
- **Task Queues:** One dedicated task queue per tile core.

---

## 3. Home-Cache Model & Access Latencies
Data cache lines are homed in specific tile L2 banks:
- **Task Data Distribution Vector ($D[64]$):** Represents the footprint volume (in bytes/cache lines) homed at each tile $i$:
  $$D = [D_0, D_1, \dots, D_{63}]$$
- **Access Latency Model (Cycles):** Calibrated directly from the paper and runtime architecture specifications:
  - Local L2 access: **10 cycles**.
  - Remote L2 access: **38 cycles base + 2 cycles per mesh hop**.
  $$\text{latency}(i, j) = \begin{cases} 10, & \text{if } i = j \\ 38 + 2 \times (|x_i - x_j| + |y_i - y_j|), & \text{if } i \ne j \end{cases}$$

---

## 4. Data Distribution Policies
1. **Fine Distribution:**
   - Cache lines are striped round-robin across all 64 tile home caches.
   - Each tile holds approximately $1/64$ of the total allocation.
2. **Coarse Distribution:**
   - Memory chunks or arrays are mapped contiguously to specific home tiles (e.g., chunk $k$ homed on tile $k \pmod{64}$).

---

## 5. Locality-Aware Scheduling Algorithms

### 5.1 Locality-Aware Work Dealing (Algorithm 3 in Paper)
When a task is created:
1. **Significance Gate:** Evaluates whether task footprint $\sum D_i > \text{LLC}/C$ ($64 \text{ KB}$). If footprint is small, the task is kept on the creator tile's local queue.
2. **Locality Cost Evaluation:** For significant tasks, the dealer calculates communication/access cost for executing on candidate tile $X \in [0..63]$:
   $$\text{cost}(X) = \sum_{i=0}^{63} \frac{D[i]}{\text{line\_size}} \times \text{latency}(X, i)$$
3. **Queue Placement:** Routes the task to $\operatorname{argmin}_{X} \text{cost}(X)$. Under coarse distribution, this directly assigns tasks to the core homing the data.

### 5.2 Vicinity-Based Work Finding / Stealing
When a worker's local queue is empty:
1. It queries candidate remote queues strictly within configured **vicinity radius $d$** ($\text{hops} \le d$).
2. It walks concentric Manhattan distance rings ($d = 1, 2, \dots, \text{vicinity}$) to steal work from the closest eligible neighbor.
3. Queues beyond the configured vicinity are excluded from search.
4. **Baseline Work-Stealing (WS):** Unrestricted search across all 63 remote tiles without distance filtering.

---

## 6. Build & Test Instructions

### 6.1 Compilation
```bash
cd tilepro64
make clean && make
```
Generates executables `bin/tilepro64` and `bin/test_suite`.

### 6.2 Running Section 21 Validation Suite
```bash
make test
```
Verifies all 11 correctness points (64 tiles, 8x8 mesh, queue creation, $D[64]$ accuracy, latency formula, work dealing, local execution, vicinity-bounded stealing, dependency gating, and single execution).

---

## 7. Running Experiments & Generating Visualizations
```bash
make results
```
This automated workflow:
1. Runs the full experiment matrix across **Map** and **Vecmul**, comparing **WS** vs **LA**, **Coarse** vs **Fine**, and vicinity radii $d \in [1, 14]$.
2. Emits raw data to `results/raw_results.csv`.
3. Programmatically generates the grouped-bar chart `results/tilepro64_results.png`.

---

## 8. Headline Empirical Results

### Communication Cost (Million Cycles) & Simulated Makespan (Thousand Cycles)
| Benchmark | Configuration | Simulated Comm Cost | Simulated Exec Time | Steals | Local Executions |
|---|---|---|---|---|---|
| **Map** | `LA + Coarse` | **1.31 M** | **170.5 K** | **0** | **64** |
| **Map** | `LA + Fine` | 6.30 M | 255.6 K | 63 | 1 |
| **Map** | `WS + Coarse` | 6.67 M | 281.1 K | 63 | 1 |
| **Map** | `WS + Fine` | 6.30 M | 255.6 K | 63 | 1 |
| **Vecmul** | `LA + Coarse` | **1.09 M** | **198.8 K** | **0** | **64** |
| **Vecmul** | `LA + Fine` | 4.72 M | 254.7 K | 63 | 1 |
| **Vecmul** | `WS + Coarse` | 4.89 M | 258.4 K | 63 | 1 |
| **Vecmul** | `WS + Fine` | 4.72 M | 254.7 K | 63 | 1 |

### Key Observations
1. **Dramatic Locality Gain:** `LA + Coarse` achieves an **80.3% reduction in communication cost** and **39.3% reduction in simulated execution time** on Map by placing tasks directly onto their home-cache tile.
2. **Work-Stealing Overhead:** `WS` places all tasks on creator Tile 0, forcing 63 worker cores to steal across the 2D mesh, creating high remote memory traffic and execution delays.

---

## 9. Paper Mechanisms vs Simulation Assumptions
- **Exact Paper Mechanisms:** 8x8 2D mesh topology, 10-cycle local / 38-base remote + 2-hop Manhattan latency, 64 KB LLC/C significance threshold, Algorithm 3 work dealing, vicinity search rings.
- **Simulation Parameters:** Host CPU cycle scaling and discrete execution round stepping for deterministic simulation reproducibility.
