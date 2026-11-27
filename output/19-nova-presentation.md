# 19 — NOVA: Complete Presentation Source Document

A single, self-contained compilation of the NOVA/ALLoC novelties and the final MIR benchmarking
results, organized for slide-making. Each `##` section below is sized to become one or two slides;
bullets are kept short on purpose, with the supporting detail underneath for speaker notes. Full
technical depth (code excerpts, bug forensics, every caveat) lives in
[`18-nova-gaps-novelties-study-guide.md`](18-nova-gaps-novelties-study-guide.md) — this document is
the distilled, presentable version of that one.

## Suggested slide breakdown

1. Title / project framing
2. The base paper & what it got right
3. The problem — 7 conceptual gaps (H1–H7)
4. The problem — 3 implementation bugs (D1–D3)
5. ALLoC: the three-part answer (N-A/B/C overview)
6. NOVA architecture — control plane vs. data plane
7. N-A — Load-Aware Dealing
8. N-B — Task-Level Locality Stealing (Shadow Index)
9. N-C — Adaptive Steal Radius
10. What we deliberately didn't build (scope honesty)
11. Engineering rigor — 3 real concurrency bugs found & fixed
12. Methodology — why gem5 was abandoned, and the harness bug we caught ourselves
13. Results — communication cost (the win)
14. Results — execution cycles (the honest cost)
15. Case study — SparseLU's queue imbalance, told completely
16. The one-slide conclusion
17. Caveats & future work

---

## 1. Project framing

- **Base paper:** Muddukrishna, Jonsson, Brorsson, *"Locality-Aware Task Scheduling and Data
  Distribution for OpenMP Programs on NUMA Systems and Manycore Processors,"* Scientific Programming,
  2015.
- **Goal:** reproduce its results on its own runtime (MIR), then design and build a scheduler that
  **beats it on its own two metrics** — normalized critical-path time, dispatch-stall cycles.
- **Our contribution:** **NOVA** (NUMA-Optimized, Vicinity-Adaptive co-scheduler) — the systems design
  that implements **ALLoC** (Adaptive Locality–Load Co-Scheduling), a three-part extension to the
  paper's own scheduler.
- **Scale of what was built:** ~10 new C files (the `nova/` subsystem), 3 new scheduler policies
  registered as an ablation ladder, a 400-run verified benchmark matrix, and two full study-guide
  documents.

---

## 2. What the base paper already does well

- Adds a **locality-aware scheduler** (`push_numa`/`pop_numa`) on top of plain work-stealing:
  places each task on the NUMA node that minimizes a communication-cost estimate, and steals by
  NUMA distance when a worker runs dry.
- Backed by a real **data distribution** layer (`fine` = page-interleaved, `coarse` = whole-allocation
  round robin) decided once, at allocation time.
- The paper's own figures show real wins over plain stealing — this is a working, validated idea, not
  a strawman. NOVA's job is to find and fix what it leaves on the table, not to replace it.

---

## 3. The problem, part 1 — seven conceptual gaps (H1–H7)

| # | Gap | One-line why it costs |
|---|---|---|
| H1 | Steal reach is **static**, hand-tuned | Paper's own Fig. 11: optimal reach is workload-dependent — one fixed value is wrong most of the time |
| H2 | Stealing is **queue-level**, not task-level | A distant queue can still hold a task that's locally cheap for the thief — never checked |
| H3 | Placement **ignores load** | Paper itself says load-balancing often matters more than locality, then builds a locality-only dealer anyway |
| H4 | **Binary threshold cliffs** gate locality treatment | One byte under a cutoff = zero locality consideration; the cutoffs are unvalidated |
| H5 | Footprint estimate is **fragile** | Garbage in, garbage out, silently — out of scope for us, managed defensively |
| H6 | **No feedback loop** between distribution and scheduling | A hot mis-placed page never gets re-homed, even across thousands of identical iterations |
| H7 | `fine`/`coarse` is a **two-item menu** | Paper itself admits SparseLU needs something more advanced — out of scope for us |

**NOVA targets H1, H2, H3, H4 directly.** H5/H7 were scoped out from day one; H6 was a stretch goal
(N-D) that was ultimately not built — see slide 10.

---

## 4. The problem, part 2 — three implementation bugs (D1–D3)

Found in the specific MIR snapshot (2018) we built on — correctness bugs, independent of the design
critique above:

- **D1 — variance computed from one node only.** `sum_dsq = (diff*diff)` instead of `+=` in the
  stddev accumulator — the "is this distribution significant" gate was effectively reading noise.
  A real bug, and directly relevant since N-A replaces this exact gate.
- **D2 — `push_numa()` scans workers, not nodes.** `O(workers)` where `O(nodes)` suffices; fragile to
  CPU affinity layout.
- **D3 — the remote steal sweep is unordered.** A thread can steal from the *most distant* node before
  checking a *nearer* one — directly undercuts the scheduler's own locality intent.

All three were fixed as part of building NOVA (D1/D2 as shared infrastructure, D3 folded into N-C).

---

## 5. ALLoC — the three-part answer

**Adaptive Locality–Load Co-Scheduling.** Three coupled, independently-switchable scheduler changes,
each reusing data the runtime already computes:

| | Replaces | Targets |
|---|---|---|
| **N-A** — Load-aware dealing | blind `argmin(comm_cost)` placement | H3, H4 |
| **N-B** — Task-level locality stealing | "take whatever's at the queue's steal end" | H2 |
| **N-C** — Adaptive steal radius | static, hand-tuned vicinity size | H1, D3 |

Implemented as an **additive ablation ladder** — `nova-a` (N-A alone) → `nova-ab` (+N-B) → `nova`
(+N-C, the full contribution) — so every benchmark result can show exactly which component produced
which effect, not just a before/after.

---

## 6. NOVA's architecture — why a "control plane" exists

**The hard problem:** N-A needs a live read of every node's queue size on *every task dealt*; N-C
needs a live read of every worker's steal radius on *every pop attempt*. Done naively, that's an
`O(nodes)` lock-free read on the hot path of every single scheduling decision across 24 workers —
risking making the "smarter" scheduler slower purely from bookkeeping.

**The fix — control-plane / data-plane split:**
- **Data plane** (hot path): every push/pop reads **one pre-published snapshot** via a single atomic
  pointer load. `O(1)`, no lock, regardless of node count.
- **Control plane** (cold path): a **Coordinator** recomputes the real `O(nodes)` stats — occupancy,
  steal-success/failure EWMAs, blended `α`, per-worker radius — once every **64 operations**, via a
  CAS-elected single writer, then publishes a fresh snapshot with one atomic pointer swap.

**The other key piece — the Shadow Index:** a small (`K=8`), lock-free, append-only ring per task
queue, caching recent tasks' footprints so a thief can *peek* at candidates without touching the real
queue's locks. This is what makes task-level stealing (N-B) possible without a global lock on every
steal attempt.

---

## 7. N-A — Load-Aware Dealing

**Mechanism:**
```
score(node) = α · cost_norm(node) + (1 − α) · occupancy_norm(node)
```
- Place the task on the node with the **lowest** blended score — not just the closest one.
- `α` is tuned online from a global steal-rate EWMA: high failure rate → trust load more; low
  failure rate → trust locality more.
- Replaces H3's load-blind placement *and* H4's broken `StdDev` gate with one continuous signal.

**Cost:** one extra `O(nodes)` read per significant task; `α` update is `O(1)`.

---

## 8. N-B — Task-Level Locality Stealing

**Mechanism:**
- Victim *queue* still chosen by distance (unchanged from the base paper).
- But instead of blindly taking the oldest task, **peek up to 8 cached candidates** (via the Shadow
  Index) and take whichever one minimizes communication cost **to the thief**, not the original
  placer.
- Bounded `O(K·nodes)` scan, only on the (already slower) steal path — never touches the common local
  pop.

**This is the single largest comm-cost effect measured in the whole study** (see slide 13) — Reduction
and Jacobi barely improve under the base paper's own scheduler alone, and jump dramatically once N-B
is added.

---

## 9. N-C — Adaptive Steal Radius

**Mechanism:**
- Each worker's steal radius starts at 1 hop and adapts from a per-node EWMA of steal
  success/failure: sustained failure → radius grows (capped at network diameter); high success rate →
  radius shrinks back down.
- The old *unordered* remote-queue sweep (D3) is folded into the **same** radius-bounded,
  distance-ordered walk — nearer queues are always checked before farther ones, at every hop.
- Replaces one static, hand-picked number (the paper's own Figure 11 trade-off) with a self-managing
  signal.

---

## 10. What we deliberately didn't build

Honesty matters for credibility — this was a scoped decision, not a shortfall discovered too late:

- **N-D (between-phase re-distribution, fixing H6)** — never implemented. Distribution is still fixed
  forever at allocation time in this codebase; nothing re-homes a hot page between loop iterations.
- **The Heuristic Feedback Log** (a planned smoothing layer over the raw EWMA control loop) — never
  implemented. The `α`/radius controllers react directly to raw EWMAs with simple hysteresis; there's
  no meta-level tracking of whether a past adjustment actually helped.
- Both were explicit stretch goals in the original design docs, contingent on the core three landing
  first — not things we claimed and failed to deliver.

---

## 11. Engineering rigor — three real concurrency bugs, found and fixed

Validated through **actual native stress-testing (gdb + AddressSanitizer)**, not code review alone —
each bug is a genuine, non-obvious concurrency hazard a read-through plausibly would have missed:

1. **Use-after-free** — the dealer appended a task to the Shadow Index *after* pushing it to the real
   queue; another worker could steal, run, and free it in between. Fixed by reordering: append
   *before* push, always safe.
2. **Torn read** — the Shadow Index's lock-free peek read a task pointer and its footprint pointer as
   two separate, unsynchronized reads; a concurrent write could hand back a mismatched pair. Fixed
   with a seqlock-style generation-counter check plus explicit memory fences.
3. **The dominant bug** — a pre-existing MIR quirk only allocated a stats array for a scheduler
   literally named `"numa"`, silently leaving it `NULL` for every NOVA variant — a NULL-pointer read
   on nearly every steal. This one bug caused the overwhelming majority of all crashes seen in
   development. Fixed by extending the allocation gate.

**Result:** native stress-test pass rate went from **~0% to ~97%** across the full benchmark ×
ablation × distribution cross-product once all three were fixed.

---

## 12. Methodology — the two pivots that happened along the way

- **gem5 full-system simulation was attempted and abandoned.** The course mandated it for real
  measured NUMA latency instead of a modeled estimate — but a full run took **61 hours without
  finishing** on available hardware, and repeated matrix launches kept getting killed by memory
  pressure. Reverted to the native HAL-Sim testbed already validated for the base-paper reproduction.
- **We found and fixed a bug in our own benchmarking harness before trusting any number.**
  An initial 200-run probe's `summary.csv` didn't actually match its own raw `matrix.csv` — off by
  36.5% on the one baseline every Map-benchmark percentage was normalized against. The harness also
  always ran configs in the same fixed order (`ws` first, `nova` last, every time) — a textbook
  ordering confound. Both fixed: **400 runs, 10 reps, shuffled job order (fixed seed)**, and
  `summary.csv` independently re-verified to match `matrix.csv` to the 4th decimal place before any
  number below was trusted.

---

## 13. Results — communication cost (the win)

Normalized to `ws+coarse` (baseline work-stealing). **Lower is better.**

| Benchmark | `ws` baseline | `la` (base paper) | `nova-a` | `nova-ab` | **`nova` (full)** |
|---|:---:|:---:|:---:|:---:|:---:|
| Map | 1.000 | 0.547 | 0.549 | 0.547 | **0.557 (−44.3%)** |
| Matmul | 1.000 | 0.773 | 0.773 | 0.772 | **0.811 (−18.9%)** |
| Reduction | 1.000 | 0.948 | 0.958 | **0.593** | **0.587 (−41.3%)** |
| Jacobi | 1.000 | 0.953 | 0.949 | **0.583** | **0.573 (−42.7%)** |
| SparseLU | 1.000 | 0.734 | 0.737 | 0.718 | **0.833 (−16.7%)** |

**Key talking point:** Reduction and Jacobi barely move under the base paper's scheduler alone
(−5%, −5%) but jump to **−41%, −43%** the moment N-B (task-level stealing) is added — the single
largest effect measured anywhere in this study, and exactly the mechanism H2 predicted.

---

## 14. Results — execution cycles (the honest cost)

Same normalization, same benchmarks. **Lower is better. This is real, measured wall-clock time — not
modeled.**

| Benchmark | `ws` baseline | `la` | `nova-a` | `nova-ab` | **`nova` (full)** |
|---|:---:|:---:|:---:|:---:|:---:|
| Map | 1.000 | 1.057 | 1.073 | 1.064 | **1.014 (+1.4%)** |
| Matmul | 1.000 | 0.985 | 1.063 | 1.033 | **1.085 (+8.5%)** |
| Reduction | 1.000 | 1.018 | 1.045 | 1.036 | **1.072 (+7.2%)** |
| Jacobi | 1.000 | 1.108 | 1.133 | 1.096 | **1.675 (+67.5%)** |
| SparseLU | 1.000 | 1.192 | 1.224 | 1.168 | **1.267 (+26.7%)** |

**Key talking point — say this out loud, don't bury it:** full NOVA is **slower in real exec_cycles
than baseline `ws` for every single benchmark.** This is reproducible across 10 shuffled-order reps,
not noise. Why: this testbed has **no real multi-socket memory latency** — exec_cycles here measures
*only* NOVA's own bookkeeping overhead (EWMA tracking, Shadow Index, Coordinator refresh, extra queue
scans). The comm-cost win on slide 13 is a *modeled* proxy for memory latency that never feeds back
into wall-clock time in this environment.

---

## 15. Case study — SparseLU's queue imbalance, told completely

A complete example of "the mechanism works, the wall clock doesn't care (here)":

- Base paper's scheduler (`la+coarse`) genuinely starves the machine: **187.7 tasks owned on node 0,
  only 16.3 stolen**, out of 204 — seven of eight nodes mostly idle.
- Full NOVA rebalances this to a healthy **112.6 owned / 91.4 stolen** — N-A's load-aware dealer is
  doing exactly what it was designed to do.
- Communication cost drops **16.7%** below baseline.
- **Exec_cycles is still 26.7% slower than baseline.** The queues are balanced; the benchmark does not
  run faster. The mechanism-level claim is true and verified. The performance-level claim does not
  follow from it — not on this testbed.

---

## 16. The one-slide conclusion

> NOVA's communication-cost wins are real, substantial, and reproduce cleanly across every benchmark
> and every ablation level — N-B's task-level stealing in particular delivers exactly the large,
> mechanism-explainable improvement the design predicted. But on a single-socket testbed with no real
> NUMA memory latency, every one of those wins is purchased with a measured, reproducible execution-time
> cost that **this benchmark alone cannot prove is worth paying** — because the thing that would make
> it worth paying (real memory latency actually hurting the baseline) is exactly what this testbed
> cannot simulate. NOVA demonstrably buys locality at a demonstrable price. Whether real multi-socket
> hardware pays that price back is a precise, stated open question — not a claim we're making.

---

## 17. Caveats & future work

- **No real multi-socket NUMA hardware validation yet** — the single biggest open item. Everything on
  slides 13–15 would need to be re-run on genuine hardware (or a completed gem5 run) to settle the
  conclusion on slide 16.
- **H6 and the Heuristic Feedback Log remain unimplemented** (slide 10) — real, acknowledged scope
  gaps, not bugs.
- **H5 (footprint fidelity) and H7 (a third distribution policy)** were never in scope as
  contributions.
- **10 reps is better than the original 5, but still not a formal confidence interval** — treat
  percentages as point estimates, not exact figures.
- **Absence of a fourth concurrency bug is not provable** — only that native stress testing at the
  real worker count reached ~97% pass rate before any matrix run was trusted.

---

## Appendix — source documents for deeper detail

| Need | Document |
|---|---|
| Full technical deep-dive (code, bug forensics, every number) | `docs/18-nova-gaps-novelties-study-guide.md` |
| Conceptual hotspots H1–H7 in full | `docs/04-hotspots.md` |
| Implementation defects D1–D3 in full | `docs/05-implementation-defects.md` |
| ALLoC novelty specs | `docs/06-novelties-alloc.md` |
| NOVA systems architecture spec | `docs/10-nova-architecture.md` |
| Raw + aggregated benchmark data | `NOVA-Locality-Aware-Scheduler/results/numa/{matrix,summary}.csv` |
| Result charts (PNG) | `NOVA-Locality-Aware-Scheduler/results/numa/fig7_nova_comparison.png`, `nova_ablation_study.png` |
