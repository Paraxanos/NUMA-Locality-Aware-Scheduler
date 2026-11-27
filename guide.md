# Complete Beginner-Friendly Study & Debate Guide (`guide.md`)
**Everything Explained From Zero (Assuming Only Basic C / C++ / OpenMP)**

This is your **single source of truth** for tomorrow's presentation and debate. Every acronym, research term, C function, benchmark number, and competitor weakness is explained in plain English from scratch.

---

## 1. Zero-Jargon Glossary (Read This First!)

If you hear any of these words tomorrow, here is what they actually mean in plain C/OpenMP terms:

### Hardware & Memory Basics
* **Core vs. Socket (Node):**
  * A **core** is one CPU worker that runs one thread at a time.
  * A **socket (or NUMA node)** is a physical chip holding a group of cores (e.g., 6 cores) plugged into its *own* local sticks of RAM.
  * An **AMD Opteron 8-Node NUMA server** has 8 nodes connected by wires (called **HyperTransport**). Any core can read RAM from any node, *but*:
    * Reading **your own node's RAM** takes **~40 CPU cycles** (fast).
    * Reading **another node's RAM across the wires** takes **240–340 CPU cycles** (**6× to 8.5× slower**).
* **NUMA (Non-Uniform Memory Access):** Memory access speed depends on *where* the data physically sits in RAM relative to *which* CPU core is reading it.
* **Cache Hierarchy (`L1`, `L2`, `L3` / `LLC`):**
  * Tiny, ultra-fast memory inside the CPU chip itself.
  * **LLC (Last-Level Cache):** The biggest, final cache inside the processor before going out to slow RAM (e.g., `L2` on TILEPro64, `L3` on Opteron).
* **Cache Line (`64 bytes`):** CPUs do not fetch 1 `int` (4 bytes) at a time from RAM—they fetch a whole **64-byte chunk** (`16` floats/ints at once), called a cache line.
* **OS Page (`4 KB = 4096 bytes`):** The operating system hands out physical RAM in **4 KB chunks** called pages.
* **First-Touch Policy (`standard` allocation):** When you call `malloc()` in C, the OS does **not** pick which NUMA node's RAM holds the array yet. Whichever CPU thread **writes to a 4 KB page first** gets that page placed on its own node's RAM.

### OpenMP, MIR & Runtime Terms
* **OpenMP Task (`#pragma omp task`):** You put a pragma above a C function call so any idle thread in a thread pool can execute that chunk of work later.
* **`depend(in: A)` clause:** An OpenMP tag telling the runtime *"Before running this task, know that it reads array `A`."*
* **MIR (`Massive Infrastructure for Runtime`):**
  * It is **NOT** a general CS acronym—it is simply the **name of the custom C OpenMP runtime library** written by the authors of the 2015 research paper (located in `vendor/mir-dev/` in our repo).
  * Normally GCC uses `libgomp` to run `#pragma omp task`. The paper's authors replaced `libgomp` with **MIR** because MIR tracks where arrays live in RAM and schedules tasks accordingly.
* **HAL-Sim (`Hardware Abstraction Layer Simulator`):**
  * Our lab machines do not have 8 physical AMD Opteron sockets (they only have 1 socket).
  * So MIR uses **HAL-Sim** (`arch_sim_numa8`), a software model inside MIR that *tags* each `malloc` chunk with a fake node ID (`0..7`) and multiplies remote accesses by the real Opteron distance numbers (**10** local, **16** 1-hop, **22** 2-hop) to **calculate** modeled communication cost—even though the CPU itself runs on 1 physical socket.
* **Footprint Vector (`D`):**
  * Literally just a C integer array of size `N` (where `N = 8` nodes on NUMA, or `N = 64` tiles on TILEPro64):
    ```c
    uint64_t D[64]; // D[i] = how many bytes this task will read from node/tile i
    ```
  * Example: If Task 5 reads a `128 KB` array stored entirely on Tile 42, then `D[42] = 131072` and every other `D[i] = 0`. That whole array `D` is called the **footprint vector**.
* **"Garbage In, Garbage Out":**
  * A common engineering phrase meaning: if you feed bad/incomplete input to a program, even the smartest algorithm gives wrong output. Here: if a programmer forgets an array inside `depend(...)`, the footprint vector `D` is wrong, so the scheduler places the task on the wrong CPU.

### Scheduling & Data Distribution Terms
* **Work-Stealing (`WS`):** Default OpenMP behavior. Every CPU core has its own task queue. If Core 3 finishes early and its queue is empty, it randomly picks another busy core (say Core 0) and **steals** a task from the back of Core 0's queue—even if that task's data lives far away on Node 7!
* **Work-Dealing (`LA` = Locality-Aware):** Instead of letting the creator core keep all tasks and waiting for stealing, the creator core **deals (pushes)** a new task directly into the queue of whichever node/tile holds most of the task's data (`argmin(communication_cost)`).
* **Vicinity / Steal Reach:** How far away (in mesh hops or NUMA hops) an empty worker is allowed to look when stealing a task. `Vicinity = 2` means "only steal from neighbors within 2 hops."
* **`coarse` vs. `fine` Data Distribution:**
  * **`coarse` distribution:** Put an *entire* allocated array on **one single node/tile** (next allocation goes to the next node/tile round-robin). Best when each task owns one isolated array.
  * **`fine` distribution:** Chop every array into small pieces (64-byte cache lines on mesh, 4 KB pages on NUMA) and spread them **round-robin across all nodes/tiles** like dealing a deck of cards. Avoids overloading 1 memory bank, but kills pure local access.
* **Makespan:** Total execution time from when the first task starts until the **very last task finishes** (the slowest core determines makespan!).
* **Starvation:** Cores sitting completely idle (`0` tasks) because all tasks got piled into another core's queue.

### Benchmark Kernels (The 5–6 Programs Tested)
1. **Map:** 48–64 completely independent tasks; each task reads/updates its own vector (`out[i] = in[i] * const`). Zero sharing between tasks.
2. **Vecmul:** Vector multiplication (`C[i] = A[i] * B[i]`). Two adjacent chunks share boundaries.
3. **Matmul:** Matrix multiplication (`C = A × B`) split into square sub-blocks. Many tasks reuse the same rows of `A` and columns of `B`.
4. **Reduction:** Combines array chunks down to a single sum (tree structure—lots of cross-task sharing).
5. **Jacobi (2D Stencil):** A 2D grid where each cell updates itself using its **4 neighbors (North, South, East, West)** across repeated iterations. Those neighbor border rows/columns shared between adjacent tasks are called **halos**.
6. **SparseLU:** Matrix factorization (`A = L × U`) broken into sub-blocks. Tasks have **irregular dependencies** (you cannot compute row blocks until the pivot block finishes) and skewed data reuse (`lu0 → fwd/bdiv → bmod`). **This is the hardest benchmark.**

### Bugs & Concurrency Terms
* **Race Condition / Data Race:** Two threads read/modify the same variable or queue at the exact same time without proper locking, causing crashes, lost tasks, or duplicate work.
* **Use-After-Free:** Thread A frees (`free(t)`) a task pointer while Thread B is still about to read `t->data`. Guaranteed crash/corruption.
* **Torn Read:** Thread A reads a pair `(task_ptr, footprint_ptr)` while Thread B is halfway through overwriting that slot—so Thread A gets the *new* task pointer paired with the *old* footprint!
* **Seqlock:** A lightweight version-counter trick: read the counter before reading data, read it again after; if the counter changed (or is odd), someone wrote mid-read, so retry!
* **ASan (`AddressSanitizer`):** Compiler flag (`-fsanitize=address`) that automatically catches use-after-free, buffer overflows, and invalid memory reads at runtime.

---

## 2. The Main Problem Statement (What Everyone Is Trying to Solve)

**Base Paper:** *Muddukrishna, Jonsson, & Brorsson (Scientific Programming, 2015)*

1. **The Core Problem:**
   In multi-socket NUMA systems (Opteron) and 2D mesh processors (TILEPro64), fetching data from a remote memory node/tile costs **4×–8.5× more cycles** than local memory.
   * **Standard Work-Stealing (`ws`)** balances CPU cores well, *but* ignores data locations—causing massive interconnect stalls.
   * **The Paper's Proposal (`la`, Locality-Aware)** places each task where its data lives (`argmin(comm_cost)`), and limits stealing to a fixed radius.
2. **Why the Paper Isn't Enough (The SparseLU Paradox):**
   * When we reproduced the paper's experiments on MIR (`results/numa/`), `la+coarse` cut communication cost by **45.1%** and sped up **Map by 19.0%**.
   * **HOWEVER, on irregular `SparseLU`, `la+coarse` was 15.1% SLOWER than simple work-stealing!**
   * *Why?* Because `la` placed 187.7 tasks onto Node 0 (where pivot blocks lived) and left 7 other nodes with almost nothing (`16.3` steals total). **23 cores starved waiting on Node 0.**
3. **Our Team's Two-Part Solution:**
   * **Track 1 — NUMA / MIR (`NOVA` / ALLoC):** Fixes load blindness, queue-tail stealing, static steal reach, and 3 MIR bugs across **400 shuffled experiments**.
   * **Track 2 — Manycore Mesh (`tilepro64/`):** Full C model of the 64-tile 8×8 mesh resolving **all 4 mesh-specific bottlenecks (`H8`, `H9`, `H10a`, `H10b`)** with **15/15 unit tests passing**.

---

## 3. All 10 Hotspots (H1–H10b) + 3 Bugs (D1–D3) — Deep Dive on YOUR Part (`H5, H6, H7`)

### YOUR CORE PART: Hotspots H5, H6, and H7

> **Why H5, H6, and H7 matter:** While NOVA (`N-A`, `N-B`, `N-C`) fixes scheduling decisions (`H1–H4`), **H5, H6, and H7 are flaws in how memory data itself is measured, placed, and updated.** Most rival teams overlook or cheat on these—giving you massive debate leverage!

#### 1) Hotspot H5 — Fragile Footprint Estimation (`D` vector)
* **What happens in code:**
  To build `D[node]`, the runtime reads OpenMP `depend(in: A, B)` clauses (or `rt_dep_t` / `mir_data_footprint_t`).
* **Why it is fragile:**
  1. If a programmer forgets to list an array in `depend(...)` (or uses non-square blocks since MIR's `row_sz` `FIXME` only expresses square blocks), `D` undercounts bytes.
  2. There is **zero error warning**—the scheduler silently sends tasks to the wrong CPU (**garbage in, garbage out**).
  3. Worse: if an array shrinks below the significance threshold (`sum(D) <= LLC / cores`), the locality scheduler **shuts itself off completely** and dumps tasks locally!
* **How WE handled H5:**
  We managed H5 defensively: every policy we compare (`ws`, `la`, `nova-a`, `nova-ab`, `nova`) receives the **exact same complete footprint declarations** and `512×512` blocks (`> LLC/C`), so we never cheat the comparison.
* **How Rival `Siddharth-k7` FAILED on H5 (Killer Debate Point!):**
  Look at `Siddharth-k7/README.md` lines 331–333:
  > *"Algorithm 1's threshold `LLC / cores` is 64 KiB here... SparseLU's tasks (8 to 12 KiB) are below it, so they always stay in the creating node's queue"*
  **Because they shrank SparseLU so much (`32×32` blocks), `sum(D)` fell below `64 KiB`, completely bypassing their own locality-aware dealer (`deal_work`)!** On top of that, they had to alter SparseLU's input matrix diagonal (`+ 2*NB*BS`) to avoid dividing by zero (`NaN`).

#### 2) Hotspot H6 — No Feedback Loop Between Data Distribution & Scheduling
* **What happens in code:**
  In C/OpenMP, you choose `coarse` or `fine` **once** when calling `omp_malloc` / `mir_mem_pol_config()`. After allocation, **pages stay frozen on that node forever**.
* **Why it hurts:**
  Iterative programs (`Jacobi`, `Reduction`) repeat the exact same task-to-data pattern for hundreds of iterations. If a page started on the wrong node in Iteration 1, every subsequent iteration pays 240–340 remote cycles—there is no runtime feedback loop to migrate hot mis-placed pages!
* **How WE handled H6 (Honesty on MIR + Proof on TILEPro64):**
  * On **MIR (NUMA)**, dynamic OS page migration (`N-D`) was kept as an unbuilt stretch goal—and we state that honestly instead of faking it.
  * On **TILEPro64 (`tilepro64/`)**, **we DID implement and test dynamic home migration as Hotspot H8 (`SCHED_POLICY_AM`)**:
    * Before migrating a 64 KB chunk to an accessing tile, `scheduler_adapt_data_placement()` checks:
      1. Does `expected_future_savings > migration_transfer_cost`? (Break-even test!)
      2. Does the chunk fit in the destination tile's `64 KB` L2 bank?
    * On the `map_h8` stress trace, adaptive migration cuts communication cost by **−82%** (`2.13M → 384K` cycles) and total modeled work by **−12.6%** (`11.73M → 10.25M` cycles).
* **How Rival `Siddharth-k7` FAILED on H6 (Killer Debate Point!):**
  Siddharth claims *"adaptive data distribution"* (`OMP_DATA_DISTRIBUTION=adaptive`), **but their `README.md` (lines 177–183) admits:**
  * *"Pages are never moved."*
  * It requires a **2-pass offline profiling trick**: run the entire program once (`RT_PROFILE_OUT`), save a `profiles/<bench>.prof` file, and run it a *second* time (`RT_PROFILE_IN`)! That is offline static tuning, not a live feedback loop—and even with an offline profile, `la-adaptive` STILL runs slower than hand-picked `la-coarse` on Jacobi (`1.44× vs 1.39×`) and Reduction (`1.10× vs 1.01×`).

#### 3) Hotspot H7 — Rigid Two-Item Memory Menu (`coarse` vs. `fine` Blind Spot)
* **What happens in code:**
  The base paper only gives programmers two choices: `coarse` (whole array on 1 node/tile) or `fine` (interleave every 64 B line / 4 KB page round-robin across *all* nodes/tiles).
* **Why it hurts:**
  * On **SparseLU**, tasks share matrix sub-blocks irregularly across steps (`coarse` overloads Node 0, `fine` ruins local cache hits)—the paper itself admits SparseLU needs *"a scheme more advanced than fine and coarse."*
  * On **2D Grid / Stencil workloads (`Jacobi`)**, `fine` (`1d_striped`) chops rows/columns in 1D across all 64 tiles. That destroys 2D neighbor geometry—reading your North/South/East/West neighbors forces traffic straight across the middle of the chip!
* **How WE solved H7 for 2D Grids (Hotspot H10a in `tilepro64/`):**
  * We added **`DATA_DIST_2D_BLOCK`** (`distribute_2d_stencil_blocked` in `tilepro64/src/data_distribution.c`), which maps an 8×8 grid of 2D stencil tasks `(tx, ty)` directly onto physical mesh tiles `(tx, ty)` so each task holds its `64 KB` block locally and all 4 neighbor halo borders (`16 KB` each) live **exactly 1 hop away**!
  * **Measured Simulator Improvement (`LA + 2d_block` vs `LA + 1d_striped` over 256 tasks):**
    * Remote access cost drops **−51.0%** (`24.74M → 12.12M` cycles)
    * Center-bisection link traffic drops **−93.8% (16× reduction!)** (`524,288 → 32,768` link-cycles)
    * Steals drop from **248 → 0** (`256/256` local executions)
    * Critical-path makespan speeds up **2.83× (−64.7%)** (`1,492,992 → 526,848` cycles).

---

### Quick Summary of the Other Hotspots (So Nobody Stumps You)

#### NUMA Scheduler Hotspots (`H1–H4`) & MIR Bugs (`D1–D3`)
* **H1 (Static Steal Reach):** Vicinity radius is fixed forever before the run. *(Fixed by NOVA **N-C**: each worker's steal radius `1..diameter` automatically grows/shrinks using an EWMA of steal success/failure.)*
* **H2 (Queue-Level, Not Task-Level Stealing):** When stealing, workers blindly pop the tail of a victim queue without checking if another task inside that queue has data on the thief's node. *(Fixed by NOVA **N-B**: a lock-free **Shadow Index** of `K=8` entries lets thieves peek inside victim queues and steal the cheapest task for themselves—slashing Reduction comm cost **−41.3%** and Jacobi **−42.7%**!)*
* **H3 (Load-Blind Placement):** `push_numa()` checks only communication cost, ignoring how many tasks are already waiting in that queue (causing the SparseLU Node 0 pile-up). *(Fixed by NOVA **N-A**: `score(node) = α · cost_norm + (1 - α) · occupancy_norm`, balancing Node 0 from `187.7 owned / 16.3 stolen` to `112.6 / 91.4`.)*
* **H4 (Binary Threshold Cliffs):** Hard cutoffs (`sum(D) > LLC/C` and `StdDev(D) > 0`) flip locality on/off abruptly. *(Replaced by N-A's smooth score.)*
* **D1 (Upstream MIR Variance Bug):** In `vendor/mir-dev/src/mir_mem_pol.c`, `sum_dsq = (diff * diff)` overwrote instead of adding `+=`! The `StdDev` gate was literally computing variance from only the last node!
* **D2 (Upstream MIR `O(workers)` Scan):** `push_numa()` looped over every worker instead of every NUMA node.
* **D3 (Upstream MIR Unordered Steal Sweep):** Idle threads could steal from a 2-hop node before checking a 1-hop neighbor. *(Fixed in N-C's hop-ordered loop.)*

#### TILEPro64 Manycore Mesh Hotspots (`H8–H10b`) — *All 4 Implemented & Tested (15/15 Pass)*
* **H8 (Static Home-Cache Trapping):** Data locked forever at far Tile 63 under heavy Tile 0 reuse. **Fix (`AM`):** Break-even adaptive migration cuts comm cost **−82%** and total work **−12.6%**.
* **H9 (Center-Bisection Link Contention):** XY routing sends cross-mesh traffic through center links (`x=3↔4`, `y=3↔4`). Ignoring link queueing makes simple models over-optimistic by **5.4×** (`448` vs `2,432` cycles on worst-case shared route). **Fix (`h9`):** 112-link XY contention simulator; priority arbitration cuts regular queue wait from **1,528 → 48** cycles.
* **H10a (2D Coarse/Fine Mismatch):** 1D line striping breaks 2D stencils. **Fix (`2d_block`):** Cuts comm **−51.0%**, center bisection **16×**, and makespan **2.83×**.
* **H10b (14-Hop Corner Drag):** When 64 tasks home at corner Tile 63, short 2-hop `LA` starves distant workers (`133,160` cyc, `638` probes), while global `WS` steals across the whole chip (`7.11` avg hops). **Fix (`dp`):** Dynamic Priority stealing ages idle workers (expanding search `1→14` hops) and steals longest-remaining-time (`ETA`) tasks first—cutting `LA` makespan by **−64%** (`47,680` cyc) with fewer steals/hops (`5.61`) than `WS`.

---

## 4. How NOVA Works & Why Our Results Beat Competitors

### What Our NOVA Co-Scheduler Actually Changed Inside MIR
Instead of locking queues on every push/pop, NOVA splits work into two paths:
1. **Fast Data Plane (`O(1)` lock-free reads):** Every worker reads a single pre-published atomic snapshot pointer (`alloc_context_t`).
2. **Cold Control Plane (`Coordinator` every 64 ops):** One elected worker updates queue occupancies, steal-rate averages, blend weight `α`, and worker steal radii, then swaps the snapshot pointer atomically.
3. **3 Real Memory/Concurrency Bugs Caught Using `gdb` + ASan (`-fsanitize=address`):**
   * *Use-After-Free:* Fixed dealer order so we append to the Shadow Index **before** pushing into the live queue.
   * *Torn Read:* Added a seqlock generation counter + memory fences so peeking `(task, footprint)` never mixes old/new pointers.
   * *NULL Stats Bug:* Upstream MIR only allocated worker stats when policy name was literally `"numa"`, crashing NOVA until extended.
   * *Result:* Native ASan stress-test pass rate jumped from **~0% → ~97%** across 400 runs!

### Headline MIR Results (400 Shuffled Runs, Normalized to `ws+coarse`)
| Kernel | Base `la` Comm | Full `nova` Comm | Honest `exec_cycles` (1-Socket HAL-Sim) |
|---|---:|---:|---|
| **Map** | 0.547 (`−45.3%`) | **0.557 (`−44.3%`)** | `1.014` (`+1.4%` bookkeeping overhead) |
| **Matmul** | 0.773 (`−22.7%`) | **0.811 (`−18.9%`)** | `1.085` (`+8.5%`) |
| **Reduction** | 0.948 (`−5.2%`) | **0.587 (`−41.3%`)** | `1.072` (`+7.2%`) |
| **Jacobi** | 0.953 (`−4.7%`) | **0.573 (`−42.7%`)** | `1.675` (`+67.5%`) |
| **SparseLU** | 0.734 (`−26.6%`) | **0.833 (`−16.7%`)** | `1.267` (`+26.7%`, queues balanced `112.6/91.4`!) |

> **Why `exec_cycles` is > 1.00 on our MIR testbed (Crucial Debate Shield!):**  
> HAL-Sim runs on a single-socket computer where reading "remote" memory takes the exact same physical time as "local" memory. Therefore, `exec_cycles` measures **only NOVA's extra scheduler bookkeeping cost**, with **zero penalty** when `ws` makes remote reads! The real multi-socket memory-latency payoff is modeled in communication cost (**up to −44.3%**) and requires genuine multi-socket hardware to repay the bookkeeping cycles. Saying this openly makes our science bulletproof!

### Why We Beat `Siddharth-k7/HPC_PROJ` & `Hemanth-G48/hpc-project`
1. **Siddharth Skipped 50% of the Problem Statement:** Their `README.md` literally states *"The TILEPro64 part (Algorithm 3) is skipped because that chip no longer exists."* (And replaced it with an unrelated CUDA GPU transfer demo.) **We solved the entire TILEPro64 mesh (`H8–H10b`, `15/15` tests).**
2. **Siddharth's Own "Improvements" Slowed Benchmarks Down:** In their own `IMPROVEMENTS_RESULTS.md`, adding their cost-aware stealing (`la-coarse-cost`) makes **Map +52.0% slower** (`0.87× → 1.32×`), **Vecmul +38.3% slower** (`1.03× → 1.42×`), and `la-adaptive-cost` hits **`1.50×` on Vecmul** while exploding SparseLU miss latency from `654 → 1,130` cycles! Why? Because `find_work_cost()` in `numa_rt.c` takes a `spin_lock(&q->lock)` on victim queues **276 times on Map just to peek `q->tail` and reject it!** Our Shadow Index peeks lock-free.
3. **Siddharth Never Touched MIR/OpenMP:** They wrote a 1,042-line custom toy `rt_task` wrapper, missed MIR bugs `D1–D3`, still pop `q->tail` blindly (`H2`), still deal load-blind (`H3`), and bypassed their own `64 KiB` threshold on SparseLU (`H5`)!
4. **Hemanth-G48 (`hpc-project`) Built No Scheduler At All:** Just 4 cores running 1 `map` C loop with `pthreads` and hardcoded memory offsets (`7` total runs)—no dealing, no stealing, no Matmul/Jacobi/SparseLU, no TILEPro64, and no novel extension.

---

## 5. `tilepro64/` Code Walkthrough (Know Every File & Key Function!)

All files live in `tilepro64/`. Build and test anytime with:
```bash
make -C tilepro64 test      # Compiles and runs all 15 unit checks
make -C tilepro64 results   # Runs all 125 configs -> raw_results.csv + charts
```

### Module-by-Module Map (Simple C Explanation)

#### 1. `src/tilepro64.h` & `src/tilepro64.c` — The 64-Tile Mesh
* Defines `g_tilepro64`, an array of 64 `tile_t` structs (`id = 0..63`).
* Each tile has coordinates `x = id % 8`, `y = id / 8`, one `64 KB` L2 home-cache bank, one `16 KB` L1 cache, and one dedicated `task_queue_t* queue`.
* **`tile_distance(id1, id2)`**: Returns Manhattan distance `|x1 - x2| + |y1 - y2|` (`0` to `14` hops).

#### 2. `src/home_cache.h` & `src/home_cache.c` — Footprint Vector `D[64]`
* Defines `data_dist_t`:
  ```c
  typedef struct {
      uint64_t bytes_per_tile[64]; // D[0..63]
      uint64_t total_bytes;
  } data_dist_t;
  ```
* Helper `data_dist_add(&dist, tile_id, bytes)` records how many bytes live in `tile_id`'s L2 bank.

#### 3. `src/latency.h` & `src/latency.c` — Cycle Cost Table
* `latency_model_init(10, 38, 2)` precomputes `g_latency_model.matrix[64][64]`:
  * Same tile (`i == j`): **`10` cycles**
  * Different tiles (`i != j`): **`38 + 2 * tile_distance(i, j)` cycles** (max `66` cycles).

#### 4. `src/data_distribution.h` & `src/data_distribution.c` — Data Placement (`H7`, `H10a`)
* `distribute_fine(&dist, bytes)`: Divides `bytes` equally across all 64 tiles.
* `distribute_coarse(&dist, home_tile, bytes)`: Puts all `bytes` on `home_tile`.
* `distribute_2d_stencil_1d_striped(...)` (**H10a baseline**): Stripes interior + 4 halo borders linearly across all 64 tiles (`(tile_y*8 + tile_x + i) % 64`).
* `distribute_2d_stencil_blocked(...)` (**H10a fix**): Keeps the `64 KB` center block on `(tile_x, tile_y)` and puts North, South, East, West `16 KB` halo borders on adjacent tiles `(x, y±1)` and `(x±1, y)` (**1 hop away**).

#### 5. `src/queue.h` & `src/queue.c` — Circular Per-Tile Task Queue
* Ring buffer (`MAX_QUEUE_CAPACITY = 1024`) supporting:
  * `queue_push(q, task)` (adds to `tail`)
  * `queue_pop_local(q)` (owner pops from `head`, FIFO)
  * `queue_steal(q)` (thief steals from `tail`)
  * `queue_remove_at(q, idx)` (used by `DP` in H10b to pluck the best-scoring task out of the middle of a queue).

#### 6. `src/scheduler.h` & `src/scheduler.c` — The Brains (`WS`, `LA`, `AM`, `DP`)
* **`scheduler_calc_comm_cost(dist, target_tile)`**: Loops `home = 0..63`; for each non-zero `bytes_per_tile[home]`, computes `ceil(bytes / 64) * latency_get(target_tile, home)` (cache lines × hop latency).
* **`scheduler_deal_task(task, creator_tile)`** *(Paper Algorithm 3)*:
  * Checks significance gate: `if (policy != SCHED_POLICY_WS && task->data_dist.total_bytes > 64 KB)` → checks all 64 tiles and pushes `task` onto the tile with **minimum `scheduler_calc_comm_cost`**.
  * Otherwise (or under `WS`), pushes onto `creator_tile`'s queue.
* **`scheduler_find_task(worker_tile)`**:
  * First tries `queue_pop_local` on own queue.
  * If empty and `LA`/`AM`: loops `d = 1 .. vicinity` (closest neighbors first) and calls `queue_steal` on the first non-empty queue within `d` hops.
  * If `WS`: round-robins across all 63 other tiles with zero distance limit.
* **`scheduler_adapt_data_placement()`** *(H8 — `AM`)*:
  * Calculates `expected_savings = savings_per_use * accesses * remaining_uses` vs `transfer_cost`.
  * Moves all bytes to `access_tile` **only if** savings > transfer cost **and** `total_bytes <= 64 KB` bank size.
* **`scheduler_find_task_dynamic()`** *(H10b — `DP`)*:
  * Expands search radius `1 + idle_rounds * 3` (capped at `14`).
  * Loops inside victim queues (`0 .. q->count - 1`), computing `ETA = compute_cycles + comm_cost`, boosting long critical-path tasks late in the batch, subtracting steal latency, and adding an `idle_rounds` aging bonus so starved workers eventually steal far tasks.
* **`scheduler_execute_task(worker_tile, task)`**:
  * Adds `task->compute_cycles + communication_cost` to `g_scheduler.tile_busy_cycles[worker_tile]` and updates `max_tile_cycles` (makespan).

#### 7. Benchmarks (`src/benchmark_*.c`) & Validation (`tests/test_suite.c`)
* **`benchmark_map.c`**:
  * `run_benchmark_map`: 64 tasks sharing two 32 KB chunks (`map`). *(Note: 32 KB < 64 KB gate, whereas `map_h8` and `vecmul` trigger >64 KB locality dealing.)*
  * `run_benchmark_map_h8`: 2 chunks at Tiles 63 & 62 reused 32× by Tile 0 (`AM` cuts comm **−82%**).
  * `run_benchmark_h9` & `run_benchmark_h9_worst`: Models 112 XY mesh links (`64 B/cycle` bandwidth) with 32 cross-bisection 4 KB transfers (`Simple XY` vs `FIFO` vs `Priority`).
* **`benchmark_vecmul.c`**:
  * `run_benchmark_vecmul`: 64 tasks, `96 KB` multi-tile chunks (`> 64 KB` gate).
  * `run_benchmark_h10b`: 64 tasks with `4 KB` data homed at corner Tile 63 (`8` tasks @ `30,000` cycles, `56` @ `5,000` cycles), comparing `WS`, `LA (2-hop)`, and `DP`.
* **`benchmark_h10a.c`**:
  * `run_benchmark_h10a`: 4 sweeps of an 8×8 Jacobi stencil (`256` tasks total, `64 KB` block + `4 × 16 KB` halos), tracking both `comm_cost` and 112-link XY bisection congestion (`1d_striped` vs `2d_block`).
* **`tests/test_suite.c`**: **15 automated tests** verifying every building block (64 tiles, 14-hop corner distance, `D[64]` math, `10 / 38+2h` latency, Algorithm 3 dealing, local execution, vicinity filtering, task dependencies, exact-once execution, H8 migration break-even, H9 priority arbitration, H10b ETA selection, and H10a 2D stencil blocking).

---

## 6. 5-Sentence Cheat Sheet to Glance Right Before Tomorrow's Debate

1. **"What was your role?"** → *"I handled the H5/H6/H7 data-model hotspot audit and built/extended the TILEPro64 8×8 manycore C simulator (`tilepro64/`), resolving H8 (migration), H9 (bisection queueing), H10a (2D stencil blocking), and H10b (dynamic corner stealing) with 15/15 validation tests."*
2. **"How do you explain H5, H6, H7?"** → *"H5: `depend` footprints fail silently when incomplete ('garbage in, garbage out'); H6: `omp_malloc` freezes page placement forever with no runtime feedback loop; H7: `coarse` vs `fine` fails irregular graphs and 2D grids—which we solved via 2D blocking in H10a."*
3. **"Why is our work better than Siddharth-k7's?"** → *"They completely skipped the TILEPro64 half of the Problem Statement, never modified real MIR (missing bugs D1–D3), let SparseLU fall below their 64 KiB threshold so their dealer skipped it, used a 2-pass offline `.prof` file for 'adaptive' distribution, and slowed Map down by +52% by spinlocking victim tails 276 times—whereas NOVA peeks lock-free via Shadow Index and cuts comm up to −44.3%."*
4. **"What if they point out NOVA's `exec_cycles` is higher on MIR?"** → *"Our MIR testbed HAL-Sim runs on a single physical socket without cross-socket RAM stall penalties—so `exec_cycles` shows pure scheduling bookkeeping overhead while communication cost drops −16.7% to −44.3% and SparseLU queues rebalance (`187.7→112.6`). Multi-socket hardware is where those memory savings repay the overhead."*
5. **"What if they ask about a function in `tilepro64/`?"** → *"`scheduler_deal_task()` checks the `64 KB` threshold and places tasks at minimum `scheduler_calc_comm_cost()`, `scheduler_find_task()` walks Manhattan rings up to `vicinity` (`tile_distance()`), `scheduler_adapt_data_placement()` handles H8 break-even migration, and `run_benchmark_h10a()` proves 2D blocking cuts center-bisection link traffic by 16×."*
