# 04 — Hotspots H1–H7

Conceptual bottlenecks in the approach. Each names what the method does, why it costs performance,
the evidence, and the headroom we expect. Referenced by the novelties in
[`06-novelties-alloc.md`](06-novelties-alloc.md).

---

## H1 — Vicinity / steal reach is static and hand-tuned

**What.** The vicinity size (manycore) is chosen once, globally, before the run. On NUMA the steal
thresholds are the fixed formula `size > cores_per_node · d`, also non-adaptive.

**Why it costs.** Figure 11 *is* the proof — small reach starves threads (poor load balance), large
reach drags in 4–6× remote latency, and the optimum moves with the workload (Map wants small, Vecmul
wants large). A single static value is wrong for part of every run and for most workloads.

- **Evidence:** Figure 11; hard-coded `low_limit` in `pop_numa()` step 4.
- **Headroom:** High — the paper's largest unaddressed knob.

---

## H2 — Victim selection is queue-level, not task-level

**What.** The scheduler chooses a victim *queue* by NUMA distance, then takes whatever task is at its
steal end. It never asks whether a *different* task in that same queue has data closer to the thief.

**Why it costs.** A queue on a distant node can still hold a task whose footprint is local to the
stealing thread — footprints and node distance are independent. The current code already computes
`comm_cost` on steal, but only to log it.

- **Evidence:** `mir_queue_pop()` / `stealWSDeque()` take the queue end; `comm_cost` recomputed "for
  stats" in `pop_numa()`.
- **Headroom:** Medium–high on irregular DAGs (SparseLU, Health, Sort).

---

## H3 — Placement optimises locality and ignores load

**What.** `push_numa()` places a significant-footprint task on the `argmin` communication-cost node,
with no reference to how long that node's queue already is.

**Why it costs.** The paper itself states *"performance benefits from load-balancing often trump
those from locality,"* then builds a dealer that is locality-only and leans on stealing to undo the
imbalance — wasting the cycles spent dealing. A skewed footprint distribution (common: one hot input
array) funnels most tasks to one or two nodes.

- **Evidence:** `push_numa()` never reads `mir_queue_size`; paper §4.1.
- **Headroom:** High when footprints are skewed toward few nodes.

---

## H4 — Binary threshold cliffs decide who gets locality treatment

**What.** Two hard gates — `sum(D) > LLC/C` and `StdDev(D) > 0` — split tasks into
"locality-scheduled" vs "dumped on the local queue."

**Why it costs.** A footprint one byte under `LLC/C` gets no locality consideration at all; a
trivially imperfect spread (`sd` just above 0) triggers the full `O(N²)` path. The cut-offs are
scale-free and unvalidated — the source literally says `FIXME: Refine this!` on the `sd == 0.0` line.

- **Evidence:** `is_data_dist_significant()`; the `FIXME` comment.
- **Headroom:** Medium — mostly removes pathological cases and cuts scheduling overhead.

---

## H5 — The footprint estimate is fragile, and the paper knows it

**What.** Footprint comes from the `depend` clause (paper) or a hand-written `mir_data_footprint_t`
(MIR). Incomplete clauses underestimate; the `exists a` fast path in Algorithm 3 needs a
*non-standard* intensity clause; `row_sz` only expresses square blocks.

**Why it costs.** Every downstream decision (place, steal, significance gate) is only as good as `D`.
A wrong `D` silently produces wrong placement with no feedback that anything went wrong.

- **Evidence:** paper §4 ("the estimate is fragile when programmers specify an incomplete depend
  clause"); `row_sz` `FIXME` in `mir_task.h`.
- **Headroom:** Medium — but also a *threat to our own results*: footprints must be declared
  consistently across every policy we compare, or we measure our declarations rather than the
  policies.

---

## H6 — No feedback loop between distribution and scheduling

**What.** Distribution is fixed at `omp_malloc` time. If it is wrong for the access pattern, the
scheduler detects "nothing to exploit" and degrades to plain work-stealing. Nothing migrates a hot
mis-placed page; nothing re-distributes between phases.

**Why it costs.** Iterative solvers (Jacobi, Reduction, any stencil) run the same task→data mapping
thousands of times — a one-time re-home after iteration 1 would amortise to nearly free, but the
method has no mechanism to express it. Page migration exists on TILEPro only as an explicitly
"high cost" route.

- **Evidence:** distribution set only in `mir_mem_pol_config()`; no migration path on the NUMA side.
- **Headroom:** High on iterative kernels; zero on single-pass kernels.

---

## H7 — `fine` / `coarse` is a two-item menu with a known blind spot

**What.** Only unit-wise round-robin and per-allocation round-robin exist. No block-cyclic, no
access-pattern-aware, no "follow the scheduler's task→node map" distribution.

**Why it costs.** SparseLU is explicitly called out as needing *"a scheme more advanced than fine and
coarse"* — its blocks have irregular, data-dependent sharing that neither policy matches, so it
regresses under the heuristic's own recommendation.

- **Evidence:** paper §6, SparseLU discussion; Figures 7 & 9.
- **Headroom:** Medium — a third policy, but a *secondary* contribution for us (the scheduler is the
  primary target).

---

## Summary

| ID | One line | Fixed by |
|---|---|---|
| H1 | static vicinity / steal reach | N-C |
| H2 | queue-level, not task-level, stealing | N-B |
| H3 | placement ignores load | N-A |
| H4 | binary threshold cliffs | N-A |
| H5 | fragile footprint estimate | (managed defensively; possible secondary work) |
| H6 | no distribution↔scheduling feedback loop | N-D (stretch) |
| H7 | `fine`/`coarse` blind spot (SparseLU) | (possible secondary contribution) |
