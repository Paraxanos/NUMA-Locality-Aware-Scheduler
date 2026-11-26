# TILEPro64 HPC Architecture and Locality Improvements

## Executive summary

This project is a C-based **architectural simulator**, not a physical TILEPro64 machine emulator or a benchmark run on TILEPro64 hardware. It represents a 64-tile, 8 × 8 mesh, estimates data access cost from a tile-to-home distance model, and simulates task queues and selected locality policies.

The main high-performance-computing (HPC) problem studied is **data locality**: a task may run on one tile while the L2 bank holding its data is on another. The simulator compares work stealing (WS) and locality-aware scheduling (LA), and adds two experiments:

- **H8 adaptive home migration (AM):** move a cache-footprint model toward a repeatedly used tile when estimated future access savings exceed migration cost.
- **H9 center-bisection contention:** route transfers over mesh links and arbitrate competing transfers using FIFO or synthetic criticality priority.

In the current result CSV, the H8 stress case shows lower modeled communication cost and total modeled work after migration. H9 priority greatly reduces the model's aggregate transfer waiting metric, but slightly increases network makespan. These are results under explicit simulator assumptions, not measured TILEPro64 hardware results.

## 1. HPC concepts needed to understand this model

### Parallelism and manycore systems

HPC programs divide work among multiple processing elements so independent work can execute concurrently. TILEPro64 is represented here as 64 tiles. More cores help only when the program exposes enough parallel work and the memory system and interconnect can supply data efficiently.

### Task parallelism and scheduling

A **task** is a unit of computation with a cost and a data footprint. A scheduler chooses where a task is queued and which worker executes it. The decision affects load balance, queue overhead, data locality, and total completion time.

- **Work stealing (WS):** idle workers search for tasks on other workers' queues. This can balance work, but a stolen task may execute far from its data.
- **Locality-aware work dealing (LA):** when a task is created, estimate the data-access cost on candidate tiles and queue it on a low-cost tile.
- **Vicinity search:** constrain stealing to tiles within a Manhattan-distance radius, preferring nearby queues.

### Memory hierarchy, cache locality, and home caches

Processors use several memory levels because small, nearby storage is faster than larger, farther storage. In this model, each tile has private L1 and a bank of distributed shared L2. A cache line has a **home tile** whose L2 bank is treated as its authoritative location. A request to another tile's home is remote and incurs a modeled penalty.

**Spatial locality** means nearby data is accessed close together; **temporal locality** means recently accessed data is reused. Both can help caches. A task scheduled near the home of its frequently used data can reduce remote traffic. Conversely, fixed homes can create a home-cache trap when the workload's access location changes or differs from the initial mapping.

### Data placement and granularity

- **Coarse placement:** a whole chunk is mapped to one home tile. It preserves chunk structure and may make placement decisions effective, but a poor home can make every access remote.
- **Fine placement:** a footprint is split across many homes. It spreads demand, but can scatter a stencil or matrix neighborhood across the mesh.
- **Chunking / blocking:** subdivide arrays so each task works on a manageable region. Good chunks can fit in cache and retain neighborhood locality. This project has coarse/fine footprint models, but does not implement real matrix tiling or cache-line mapping.

### Interconnect topology and communication cost

Tiles are connected as a 2D mesh. The shortest-path distance used by the ordinary memory model is Manhattan distance:

\[d((x_1,y_1),(x_2,y_2))=|x_1-x_2|+|y_1-y_2|.\]

The mesh diameter is 14 hops. More hops generally mean more latency and consume more network resources. A distance-only latency model cannot represent two transfers contending for the same link; H9 adds an experimental link-queue model for that missing effect.

### Bandwidth, contention, and queueing

**Bandwidth** is the amount of data a link can carry per unit time. **Contention** occurs when multiple transfers need limited shared link capacity. Transfers wait in queues, increasing their latency and potentially the application's makespan. A priority queue can decide which transfer gets service first; it does not increase link bandwidth. It may help urgent traffic while delaying lower-priority traffic.

### Load balance, latency, throughput, and makespan

- **Latency:** time for one operation or transfer to complete.
- **Throughput:** amount of work or data completed per unit time.
- **Makespan:** elapsed simulated time until all work is done; often determined by the busiest worker or critical path.
- **Load balance:** how evenly useful work is distributed across workers.

Lower aggregate communication cost does not always mean lower makespan. A policy may reduce total traffic but concentrate work on one tile or delay an important transfer. H9 demonstrates that distinction.

### Dependencies and critical paths

Tasks may depend on earlier tasks. A dependent task is not ready until its predecessors complete. The longest chain of dependent work bounds parallel speedup. The task API tracks dependencies, although the current benchmark drivers mostly run independent tasks and do not construct a full dependency graph.

### Migration and break-even analysis

Moving data costs time and consumes bandwidth. It is worthwhile only when expected future savings exceed the migration cost:

\[\text{expected future access savings} > \text{migration cost}.\]

The AM model estimates this from access latency, data size, expected reuse, and a modeled transfer rate. Real systems also require coherence, dirty-line handling, eviction, replication rules, and capacity accounting; those are not fully simulated here.

### Experimental method and reproducibility

An experiment should compare policies using the same workload and report the assumptions and metrics. The project emits CSV output and a plot. Useful metrics include communication cost, migration cost, bytes moved, queue wait, busiest-link service, and makespan. Results are deterministic for the model inputs, but should not be called measured hardware performance.

## 2. TILEPro64 architecture represented in the code

`src/tilepro64.h` defines 64 tiles in an 8 × 8 coordinate grid, 16 KB private L1 per tile, 64 KB per L2 bank, and the 14-hop mesh diameter. `src/tilepro64.c` initializes tile coordinates, cache sizes, and one task queue per tile.

`src/latency.h` and `src/latency.c` build a table of modeled access latencies:

- local home: 10 cycles;
- remote home: 38 base cycles plus 2 cycles per Manhattan hop.

These constants are the project's configured model values. They are not calibrated by the current H9 bandwidth experiment, and this report does not independently verify them against TILEPro64 silicon.

`src/home_cache.h` represents a footprint as `data_dist_t`, a 64-entry vector of bytes homed at each tile plus a total byte count. This is a compact analytic representation; it does not store addresses or cache lines.

## 3. Source code guide

| File | Role in the simulator |
|---|---|
| `src/tilepro64.h`, `src/tilepro64.c` | Tile grid, tile properties, mesh distance, queue initialization/cleanup. |
| `src/queue.h`, `src/queue.c` | Fixed-capacity per-tile task queue; local pop and tail stealing. |
| `src/task.h`, `src/task.c` | Task state, compute cycles, data footprint, dependency counters, execution metadata. |
| `src/home_cache.h`, `src/home_cache.c` | Per-tile footprint vector and home-byte accounting. |
| `src/data_distribution.h`, `src/data_distribution.c` | Fine, coarse, and multi-chunk footprint initialization. |
| `src/latency.h`, `src/latency.c` | Local/remote latency constants and distance-based latency matrix. |
| `src/scheduler.h`, `src/scheduler.c` | WS/LA/AM policy, task placement, stealing, access-cost accounting, migration break-even logic, scheduler metrics. |
| `src/benchmark_map.c` | Map workload, dedicated H8 stress trace, and currently also the H9 link simulator. |
| `src/benchmark_vecmul.c` | Vecmul-style task/data footprint benchmark. |
| `src/benchmark.h`, `src/main.c` | Result structure, command-line options, CSV formatting, full experiment matrix. |
| `tests/test_suite.c` | Thirteen checks for topology, data placement, latency, scheduling, dependencies, migration, and H9 policy arbitration. |
| `scripts/run_experiments.py` | Runs `--all` and writes the CSV. |
| `scripts/plot_results.py` | Reads CSV and writes the comparison chart. |
| `Makefile` | Build, test, experiment, and cleanup targets. |

### Scheduler behavior

`scheduler_calc_comm_cost` counts cache lines (64 bytes) in each home and multiplies by the modeled latency from the candidate execution tile. `scheduler_deal_task` keeps small tasks with the creator; for larger tasks, LA/AM choose the candidate with lowest estimated communication cost. WS queues tasks at the creator. `scheduler_find_task` checks the local queue first, then either searches all queues (WS) or walks Manhattan-distance rings up to the configured vicinity (LA/AM). `scheduler_execute_task` records the executed tile, communication cost, task completion, and worker busy cycles.

### Adaptive migration code

`scheduler_adapt_data_placement` estimates the benefit of consolidating the represented footprint at an access tile. It computes avoided remote access latency, compares estimated future savings with a transfer cost, checks that the object fits in the destination bank, then changes the footprint vector and increments migration metrics. `MIGRATION_BYTES_PER_CYCLE` is set to 16 in this code path. The H8 test case has two 32 KB objects, so each destination fits each object, but the simulator does not maintain complete bank occupancy across arbitrary objects or model eviction/coherence.

## 4. Implemented improvement experiments

### WS and LA

WS is the locality-agnostic baseline. LA makes queue placement use the modeled footprint and latency. Work stealing supports load balance when local queues empty; LA bounds the search by vicinity to reduce expensive far searches. The current benchmark drivers and queue order create specific, deterministic task traces, so results depend on those traces.

### H8: static home-cache trapping and adaptive migration

`run_benchmark_map_h8` in `src/benchmark_map.c` constructs two 32 KB chunks. Coarse placement homes them at tiles 63 and 62; fine placement distributes them across tiles. Each chunk is read 32 times by tile 0, for 64 modeled accesses total. AM attempts migration after the first access, using the remaining-use estimate and destination-capacity check. Static LA leaves the homes fixed.

Current CSV rows at vicinity 4:

| H8 placement | Access cost | Migration cost | Total modeled work | Bytes migrated |
|---|---:|---:|---:|---:|
| Static coarse | 2,129,920 cycles | 0 | 11,729,920 cycles | 0 |
| AM coarse | 384,000 cycles | 266,240 cycles | 10,250,240 cycles | 65,536 |
| Static fine | 1,689,600 cycles | 0 | 11,289,600 cycles | 0 |
| AM fine | 370,240 cycles | 210,560 cycles | 10,180,800 cycles | 64,512 |

Under this stress trace, coarse AM lowers modeled access cost by about 82% and total work by about 13%. The setup is intentionally favorable to migration: data starts far away, the same tile repeatedly accesses it, and tasks are serialized on tile 0. It is evidence about this model and trace, not an expected general speedup.

### H9: center-bisection link arbitration

The H9 experiment is implemented in `src/benchmark_map.c`. It creates 32 4 KB transfers from the left half to the right half, each crossing the vertical center bisection. It constructs deterministic XY paths over 112 **undirected physical neighbor links**. Each modeled link serves at most 64 bytes per cycle; a 4 KB transfer therefore occupies a link for 64 cycles before moving to its next hop. Arbitration is performed each cycle. FIFO uses transfer ID order; priority uses a synthetic priority value assigned by the trace.

| Policy | Aggregate queued transfer cycles | Makespan | Busiest-link service |
|---|---:|---:|---:|
| FIFO | 1,528 | 448 cycles | 256 cycles |
| Priority | 48 | 451 cycles | 256 cycles |

Priority reduces the queue-wait metric by about 97%, but makespan is 3 cycles higher in this trace. The result illustrates a queueing tradeoff, not a universal benefit. The 64 bytes/cycle value is a configurable assumption, not a measured TILEPro64 link bandwidth. The model also simplifies routing, buffering, flow control, packetization, and full-duplex behavior.

## 5. Reading the result files

`results/raw_results.csv` is the source of current simulated metrics. Its columns include communication cost, migration cost and bytes, total modeled work, makespan, H9 queue delay, busiest-link service, task counts, steals, and local/remote executions. H9 currently stores its aggregate queue-wait value in both `simulated_communication_cost` and `congestion_queue_cycles`; those two columns are duplicates for H9 rows. The network makespan is reported in `simulated_execution_time`.

`results/tilepro64_results.png` is generated from selected rows by `scripts/plot_results.py`. It is a visualization of simulator outputs, not a hardware performance chart.

The README contains older headline benchmark values and says the validation suite has 11 checks. Those sections predate H8/H9 changes; use the current CSV and the 13-check suite as the up-to-date record. In particular, the ordinary Map results differ from the older headline table.

## 6. Build and run

From the `tilepro64` directory:

```sh
make
make test
make results
```

Run an individual experiment:

```sh
bin/tilepro64 --benchmark map --scheduler la --dist coarse --vicinity 4 --csv
bin/tilepro64 --benchmark map_h8 --scheduler am --dist coarse --csv
bin/tilepro64 --benchmark h9 --scheduler fifo --csv
bin/tilepro64 --benchmark h9 --scheduler priority --csv
```

On Windows, the built executable may be named `bin/tilepro64.exe`; invoke that path if the extension is required by the shell.

## 7. Scope and limitations

The code is useful for exploring scheduling and locality hypotheses, but it is not a cycle-accurate TILEPro64 simulator. It does not model actual instruction execution, L1/L2 replacement, cache coherence protocol, memory controllers, DRAM, network routers, link-level full-duplex bandwidth, packet flow control, interrupts, OS scheduling, or measured TILEPro64 hardware.

The migration model has a fixed rate and estimated reuse. The H8 case is a hand-defined stress trace. The H9 trace assigns synthetic priorities and uses simplified per-cycle arbitration. Consequently:

1. Treat cycle counts and bandwidth as modeled units under stated assumptions.
2. Do not claim physical hardware speedups from these outputs.
3. Validate promising policies with real access traces and hardware counters or a validated architecture simulator.
4. Sweep migration bandwidth, reuse count, and H9 link capacity to determine whether conclusions are robust to assumptions.

## 8. HPC terms glossary

| Term | Meaning in this report |
|---|---|
| Tile | One compute core and its associated modeled cache resources in the 8 × 8 array. |
| Home cache | L2 bank treated as the location for a data line or chunk. |
| Footprint | Amount and distribution of data a task is modeled to access. |
| Locality | How close computation is to the data it uses. |
| Work stealing | An idle worker takes work from another worker's queue. |
| Vicinity | Maximum Manhattan distance used to search for stealable work. |
| Migration | Moving the modeled home of a chunk to another tile. |
| Bisection | A cut dividing the mesh into two halves; cross-cut traffic is limited by links crossing the cut. |
| Contention | Multiple transfers compete for the same finite link service. |
| Queue delay | Time a transfer waits for a link to serve it. |
| Bandwidth | Bytes served per cycle in the model; H9 currently assumes 64 bytes/cycle/link. |
| Makespan | Simulated time until the last modeled task or network transfer completes. |
| Critical path | Longest dependency chain that bounds completion time. |
| Throughput | Completed work or data per unit time. |
| Load balance | Distribution of work across tiles. |
| Break-even | Point where accumulated access savings become greater than migration cost. |
