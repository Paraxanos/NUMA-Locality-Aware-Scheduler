# NUMA/Opteron reproduction — results

Results from reproducing the locality-aware task scheduling paper's Figure 7 benchmark set (Map,
Matmul, Reduction, Jacobi, SparseLU) on a simulated 8-node NUMA topology
(`arch_sim_numa8`, calibrated from the paper's own machine) layered on top of the unmodified MIR
runtime and scheduler.

## Files

- **`matrix.csv`** — the full result set. 100 rows: 5 benchmarks × 4 scheduler/distribution
  configurations × 5 repetitions. Every row passed an independent algorithmic correctness check
  (`check_ok=1`), not just "didn't crash."
- **`fig7_reproduction.png`** — two-panel chart generated from `matrix.csv` by `harness/plot_fig7.py`:
  modelled communication cost (left) and real execution time (right), both normalized per benchmark
  to that benchmark's `ws+coarse` baseline.

## `matrix.csv` schema

```
benchmark, scheduler_policy, dist_policy, topology, run_id,
exec_cycles, comm_cost_total, comm_tasks_total, comm_cost_avg,
tasks_stolen, tasks_owned, check_ok
```

- `scheduler_policy`: `ws` (work-stealing baseline) or `la` (locality-aware)
- `dist_policy`: `coarse` or `fine`
- `comm_cost_avg`: modelled per-task memory-access cost — real `libnuma`-style distance units, run
  through the scheduler's own cost function, but on *simulated* placement (no real multi-socket
  hardware was available for this reproduction)
- `exec_cycles`: real wall-clock-ish cycle count around the benchmark's parallel region only

## Why two metrics

There's no real NUMA hardware behind this run — development happened on a single-socket host, with
an `arch` plugin making the scheduler believe it's placing data across 8 simulated nodes. That means
`comm_cost` (modelled) and `exec_cycles` (real) measure genuinely different things, and for 3 of the
5 benchmarks they disagree: `la+coarse` can look best on `comm_cost` (perfect locality minimizes
per-byte cost) while being *worst* on `exec_cycles` (that same concentration starves parallelism
elsewhere) — which is itself a reproduction of the paper's own point that a locality-only cost model
is incomplete, not a bug in the data.

## Headline results (`comm_cost_avg`, normalized to `ws+coarse`)

| config | Map | Matmul | Reduction | Jacobi | SparseLU |
|---|---|---|---|---|---|
| `la+coarse` | **0.549** | **0.775** | 0.956 | 0.935 | **0.733** |
| `la+fine` | 1.003 | 1.001 | 1.000 | 0.984 | 0.997 |
| `ws+coarse` | 1.0 | 1.0 | 1.0 | 1.0 | 1.0 |
| `ws+fine` | 1.003 | 1.001 | 1.000 | 0.984 | 0.997 |

`exec_cycles`, same normalization:

| config | Map | Matmul | Reduction | Jacobi | SparseLU |
|---|---|---|---|---|---|
| `la+coarse` | 0.810 | 0.994 | 1.030 | ~1.0 (noisy) | **1.151** |
| `la+fine` | 0.836 | 0.965 | 0.997 | ~1.0 (noisy) | 1.015 |
| `ws+coarse` | 1.0 | 1.0 | 1.0 | 1.0 | 1.0 |
| `ws+fine` | 0.918 | 0.974 | 0.972 | ~1.0 (noisy) | 1.011 |

Map and Matmul match the paper's reported direction and rough magnitude closely. Reduction and
Jacobi reproduce the paper's qualitative lessons but at a much smaller magnitude than the paper's own
numbers (plausibly because the paper's effect needs real multi-socket memory-latency pressure this
host can't produce). SparseLU reproduces the paper's own stated counter-example — locality-aware
scheduling with the *recommended* policy is measurably worse in real execution time, even though it
looks best on the modelled metric.

Regenerate this chart from the CSV with `python3 harness/plot_fig7.py` (from the repo root), or
re-run the full matrix with `python3 harness/run_matrix.py`.
