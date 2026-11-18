# Implementation Log: HPC Locality & Scheduling Project

## 1. Initial State & Baseline Audit

### 1.1 Git Environment & Repository State
- **Branch:** `feat/tilepro64-simulation-and-presentation`
- **Head Commit:** `b373ebb`
- **Submodule Status:**
  - `vendor/mir-dev` is checked out at commit `9f44c60ba90bbf0cde635c86f3bf86774364bb61` (clean detached HEAD).
  - `patches/nova-mir-dev.patch` has been brought up to date and verified with `git apply`.
- **Repository Hygiene Actions:**
  - Untracked 9,009 historical files (`.venv/` virtual environment and `build/bench_*` binary artifacts) from the git index via `git rm -r --cached .venv build`.
  - Updated `.gitignore` to explicitly ignore `.venv/`, `venv/`, `build/`, and `gem5/`.

### 1.2 Baseline Component Audits
1. **TILEPro64 Standalone C Model (`tilepro64/`):**
   - Built cleanly via `make clean && make`.
   - Test suite `bin/test_suite` executed: **11/11 tests passed**.
   - Verified benchmarks: `bin/tilepro64 --benchmark map --scheduler la+coarse` and `ws+coarse` run and output expected metrics.
   - **Critical Classification:** The TILEPro64 simulator in `tilepro64/` is an **analytical simulator** that models Manhattan distance latencies ($38 + 2 \times \text{hops}$) and algorithmic cache access penalties. It is **not** a cycle-accurate hardware simulation. Both `README.md` and `tilepro64/README.md` have been updated with this explicit notice.

2. **gem5 Architecture & Build (`gem5/`):**
   - Cloned gem5 `v23.0.1.0`.
   - Toolchains: Homebrew SCons 4.11.1, Python 3.12+, Protobuf 36.2, Boost 1.92.
   - Darwin / SCons 4.x compatibility patches applied:
     - `gem5/SConstruct` and SConsopts files: fixed positional argument deprecations in `CheckLibWithHeader` (changed `'zlibVersion();'` to `call='zlibVersion();'`).
     - `gem5/src/base/time.cc`: replaced undeclared `clock_nanosleep` on macOS with `nanosleep(&ts, NULL)`.
     - `gem5/src/proto/SConsopts`: adjusted pkg-config flags to include required Abseil dependencies (`-labsl_*`).
     - `gem5/configs/ruby/Ruby.py`: fixed Python 3.12+ `exec/eval` local scope issue by using `importlib.import_module()`.
   - Build artifact: `gem5/build/X86_MESI_Two_Level/gem5.opt` compiled and linked successfully.

---

## 2. Verified Architectural & Correctness Findings

### 2.1 NOVA Task Stealing & Queue Consistency Gaps (Resolved)
- **The Shadow Index Stealing Hole:**
  Previously, `nova_stealer.c` only set `task->taken = 1` without removing the task from the victim's authoritative queue (`mir_task_queue_t`). When the victim's worker called `mir_task_queue_pop()`, it popped and executed the exact same task, causing duplicate concurrent execution.
- **Queue Type & API Mismatches:**
  `mir_sched_pol_nova.c` created queues of type `mir_task_queue_t`, while `nova-mir-dev.patch` previously defined helpers on `mir_queue_t` and `nova_stealer.c` called mismatched function names.
- **Resolution Implemented:**
  - Implemented `mir_task_queue_lock_and_remove(struct mir_task_queue_t* queue, struct mir_task_t* task)` in `mir_task_queue.c` and `.h`.
    Acquires `deq_lock` then `enq_lock` (preventing deadlock), searches the circular buffer, atomically claims the task via `__sync_bool_compare_and_swap(&task->taken, 0, 1)`, and shifts subsequent elements to physically extract the task from the queue.
  - Implemented `mir_task_queue_pop_end(struct mir_task_queue_t* queue)` for fallback tail stealing.
  - Enhanced `mir_task_queue_pop()` with an atomic CAS check on `cand->taken` to automatically skip and discard tasks already claimed by remote thieves.
  - Updated `patches/nova-mir-dev.patch` to cleanly provide these extensions to `vendor/mir-dev`.

### 2.2 Safe Shadow Index & Invalidation (Resolved)
- Protected `shadow_index_t` with `pthread_mutex_t lock`.
- Added `nova_shadow_invalidate()` to purge claimed tasks from the shadow index, preventing dangling pointers and use-after-free.
- Evicted already-taken tasks during peeks.
- Added runtime ablation support via `NOVA_DISABLE_SHADOW_INDEX=1` environment variable and `nova_shadow_set_disabled()`.

### 2.3 Coordinator Concurrency & Synchronization (Resolved)
- Replaced raw non-atomic increments on the published snapshot with atomic `atomic_fetch_add` operations on dedicated coordinator monotonic counters (`steal_ok_count`, `steal_fail_count`).
- Added reader-writer lock (`pthread_rwlock_t buf_lock`) to protect the double-buffered coordinator context (`buf[2]`) during periodic refresh, guaranteeing that readers never observe torn state.
- Implemented true EWMA calculation with decay factor $\beta = 0.7$ across epochs rather than monotonic counter growth.

---

## 3. Sprint Verification Results

### 3.1 Deterministic Queue Race Suite (`tests/test_nova_queue_race.c`)
- **Suite 1: Two Thieves Racing for 1 Task:** 2,000 iterations.
  - Results: Thief 1 claimed 1,011, Thief 2 claimed 989.
  - Single claims: 2,000 / 2,000. Zero claims: 0. Double claims: 0.
- **Suite 2: Local Worker `pop()` vs Remote Thief `lock_and_remove()`:** 2,000 iterations.
  - Results: Worker popped 977, Thief stole 1,023.
  - Single claims: 2,000 / 2,000. Zero claims: 0. Double claims: 0.
- **Suite 3: Two Thieves Racing for Fallback `pop_end()`:** 2,000 iterations.
  - Results: Thief 1 got 937, Thief 2 got 1,063.
  - Single claims: 2,000 / 2,000. Zero claims: 0. Double claims: 0.
- **Outcome:** **6,000 / 6,000 races passed with zero duplicate executions, zero lost tasks, and zero deadlocks.**

### 3.2 Full Concurrency Stress Test (`tests/test_nova_concurrency.c`)
- 12 concurrent POSIX threads (4 dealers pushing, 8 workers/thieves popping/stealing) processing 40,000 tasks across simulated nodes.
- **Run 1 (Shadow Index Enabled):**
  - Executed: 40,000 / 40,000 (100% exact-once).
  - Local Owned: 31,918 (79.8%). Stolen: 8,082 (20.2%).
  - Duplicates: 0. Lost: 0.
- **Run 2 (Shadow Index Disabled / Ablation Mode):**
  - Executed: 40,000 / 40,000 (100% exact-once).
  - Local Owned: 19,859 (49.6%). Stolen: 20,141 (50.4%).
  - Duplicates: 0. Lost: 0.
- **Outcome:** **PASSED. Confirms single-ownership invariant under heavy concurrent load and demonstrates Shadow Index locality advantage (79.8% vs 49.6% local execution).**

### 3.3 Experiment Harness Separation
- Separated `harness/run_matrix.py` into:
  - `harness/run_baseline_matrix.py`: Strictly 5 benchmarks $\times$ 4 policies $\times$ 5 reps = 100 runs, writing to `results/baseline_figure7/matrix.csv`.
  - `harness/run_nova_matrix.py`: Full NOVA evaluation and ablation suite, writing to `results/nova_evaluation/matrix.csv`.
- Historical baseline `results/numa/matrix.csv` (100 empirical rows) preserved intact.
- Both runners include strict validation (`check_ok == "1"`, non-zero cycles, malformed output rejection).

### 3.4 gem5 Mesh Topology Validation
- Parameterized `gem5/configs/topologies/TilePro64_Mesh.py` to support both 4×4 (16 tiles) and 8×8 (64 tiles).
- **4×4 Mini-Mesh Validation:**
  ```bash
  ./gem5/build/X86_MESI_Two_Level/gem5.opt gem5/configs/example/ruby_random_test.py \
    --num-cpus=16 --num-l2caches=16 --num-dirs=2 --network=garnet --topology=TilePro64_Mesh \
    --ruby-clock=1GHz --maxloads=100
  ```
  Exited cleanly at tick 12671 with 0 errors.
- **8×8 Full-Mesh Validation:**
  ```bash
  ./gem5/build/X86_MESI_Two_Level/gem5.opt gem5/configs/example/ruby_random_test.py \
    --num-cpus=64 --num-l2caches=64 --num-dirs=4 --network=garnet --topology=TilePro64_Mesh \
    --ruby-clock=1GHz --maxloads=100
  ```
  Exited cleanly at tick 10671 with 0 errors.
- Tracked topology preserved in `benchmarks/tilepro64_gem5/TilePro64_Mesh.py`.
