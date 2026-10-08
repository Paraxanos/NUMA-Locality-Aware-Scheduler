# 06 — Proposed Novelties: ALLoC

**Adaptive Locality–Load Co-Scheduling.** Three coupled scheduler changes plus one stretch mechanism.
All reuse footprint data the runtime already computes; none add architectural knowledge to the
programmer's job. Each maps to hotspots in [`04-hotspots.md`](04-hotspots.md).

---

## N-A — Load-aware work-dealing

**Replaces:** the binary gate + pure `argmin` communication cost in `push_numa()`.

**Mechanism.** Place task `T` on the node minimising a single scalar:

```
score(q) = α · comm_norm(D, q) + (1 − α) · occ_norm(q)
```

- `comm_norm(D, q)` — `mir_mem_node_dist_get_comm_cost(D, q)` normalised to `[0, 1]` across the
  candidate nodes.
- `occ_norm(q)` — `mir_queue_size(q)` normalised the same way.
- `α` — tuned online from an EWMA of the global steal rate: high steal rate ⇒ imbalance ⇒ lower `α`
  (weight load more); low steal rate ⇒ raise `α` (weight locality more).

Keep the `sum(D) > LLC/C` gate — it is a real "don't bother reasoning about tiny tasks" signal.
**Drop** the `StdDev(D) > 0` gate (see D1 — it is broken anyway) in favour of the continuous `α`
blend.

| | |
|---|---|
| **Fixes** | H3, H4 (depends on D1/D2 being addressed) |
| **Cost** | one extra `O(nodes)` read of queue sizes per significant task; `α` update is `O(1)` |
| **Risk** | normalisation across a tiny candidate set can be jumpy — clamp, and fall back to pure locality when all `occ_norm` values are within a small band |
| **Measured by** | placement regret (chosen vs min `comm_cost`); steal rate; time + stall axes |

---

## N-B — Task-level locality-aware stealing

**Replaces:** "take the steal end of the nearest non-empty queue."

**Mechanism.** Once a steal target queue is chosen, inspect up to `k` candidate tasks from its steal
end and take the one minimising `comm_cost(task->dist_by_access_type[READ], thief_node)`. The
distribution vector is already cached on the task, so each check is `O(nodes)` and the whole step is
`O(k · nodes)` with `k` small (4–8). Ties and tasks with no footprint keep FIFO order.

| | |
|---|---|
| **Fixes** | H2; subsumes the "stats-only" `comm_cost` recompute already in `pop_numa()` |
| **Cost** | bounded; only on the (already slow) steal path, not the common local pop |
| **Risk** | peeking into a lock-free Chase–Lev deque mid-structure — may need a small locked side-buffer or a bounded "scan-then-CAS" loop; prototype against `mir_dequeue.c` first |
| **Measured by** | remote/local byte ratio; steal-distance histogram (`num_comm_tasks_stolen_by_diameter[]`); stall axis |

---

## N-C — Adaptive steal radius (per-worker)

**Replaces:** the static vicinity / fixed `cores_per_node · d` thresholds.

**Mechanism.** Each worker holds a radius `r`, initially 1. Track an EWMA of failed steal attempts
over a sliding window:

- sustained failure ⇒ `r++` (capped at `arch->diameter`);
- a successful near steal ⇒ `r--` (floored at 1).

`pop_numa()`'s ring search only visits nodes within `r` hops. The unordered `alt_queue` sweep (D3)
is folded into the **same** radius-limited, distance-ordered walk. Hysteresis (different up/down
window lengths) and a cap on adjustment frequency prevent thrash near a threshold.

| | |
|---|---|
| **Fixes** | H1, D3; makes Figure 11's trade-off self-managing |
| **Cost** | a counter and a comparison per steal attempt |
| **Risk** | oscillation near a threshold — mitigated by hysteresis + rate cap |
| **Measured by** | steal success rate; steal-distance histogram; time + stall axes vs the paper's best hand-picked vicinity |

---

## N-D — Between-phase re-distribution hint (stretch)

**Adds:** a closed loop for iterative kernels.

**Mechanism.** After a barrier, the scheduler exports the realised task→node execution map. A
lightweight `omp_redistribute(ptr)` call (or a next-touch fault handler) re-homes pages that were
consistently accessed from a node other than their home, within a **per-phase migration budget**.
Only armed when a "phase" — a repeated identical sub-DAG — is detected.

| | |
|---|---|
| **Fixes** | H6 |
| **Cost** | page migration is expensive — strictly budgeted, phase-gated |
| **Risk** | scope creep. Ship N-A/B/C first; N-D only if time allows *and* Jacobi/Reduction show clear residual remote traffic after N-A/B/C |
| **Measured by** | remote/local byte ratio across iterations; time axis on Jacobi, Reduction |

---

## Out of primary scope

- **H5 (footprint fidelity).** Managed defensively — footprints declared identically across every
  policy in a comparison. Not a contribution.
- **H7 (a third distribution policy).** A plausible *secondary* contribution if N-A/B/C land early:
  an access-pattern-tagged `coarse-cyclic` policy for SparseLU-like kernels. Flagged, not committed.

## Why these three cohere

N-A, N-B, N-C all live in the scheduler, all consume the same per-task footprint vector, and all
share the steal/deal instrumentation. They can be implemented behind independent `MIR_CONF` flags and
evaluated additively (baseline → +N-A → +N-B → +N-C), which also gives the report a clean ablation.
