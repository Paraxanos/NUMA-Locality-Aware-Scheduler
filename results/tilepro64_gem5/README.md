# Provenance & Fidelity: `results/tilepro64_gem5/`

These artifacts are an **analytical projection**, not a gem5 measurement.
Do not cite them as Tier 4 (cycle-accurate) results.

| File | How it was produced | What it contains |
| :--- | :--- | :--- |
| `matrix.csv` | Emitted by `benchmarks/tilepro64_gem5/run_gem5_suite.py` | 40 modelled configurations. `exec_time_ms` = host wall-clock of the native binary. `simulated_cycles`, `l2_hit_ratio`, `steals_est` = closed-form expressions / hardcoded constants in the runner, never measured. |
| `latency_calibration.csv` | Output of native `calibrate_latency` on the generating host | `Measured_Cycles` column is **0 for all 64 rows** — the cycle counter was not readable on that host. `Expected_Cycles` is the closed form `10` (local) or `38 + 2 * hops`. |
| `tilepro64_gem5_evaluation.png` | `plot_evaluation()` in the runner | Visualizes the two files above. Panel A is the closed-form model (not a calibration); Panel C is a hardcoded set of hit-rate constants. |

What *is* verified in gem5: the `TilePro64_Mesh` topology loads and exits cleanly on
Ruby/Garnet smoke tests (16-core 4×4 and 64-core 8×8 meshes), and gem5 v23.0.1.0 builds on the
generating host. No application kernel from `bench_gem5` / `calibrate_latency` has been run
inside gem5, and no m5-op ROI statistics have been collected.

To convert this to a real Tier 4 result:
1. Build the kernels against gem5's `m5` API and run each of the 40 configurations under
   `gem5.opt` (`X86_MESI_Two_Level`) with the `TilePro64_Mesh` topology.
2. Derive `matrix.csv` from `m5out/stats.txt` (see `scripts/parse_gem5_stats.py`).
3. Replace `calibrate_latency`'s measurement path with one where the cycle counter is real
   (inside gem5, `m5` pseudo-ops / `curTick()` or `stats.txt` counters).
4. Update this file and the README Tier 4 row.
