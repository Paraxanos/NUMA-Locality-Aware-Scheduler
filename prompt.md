I am attaching the research paper:

"Locality-Aware Task Scheduling and Data Distribution for OpenMP Programs on NUMA Systems and Manycore Processors"

I want you to implement ONLY the TILEPro64 / manycore portion of this paper.

IMPORTANT:
Do NOT implement the NUMA-system portion of the paper.
Do NOT implement AMD Opteron experiments.
Do NOT implement libnuma.
Do NOT implement NUMA-node allocation.
Do NOT reproduce the NUMA benchmark results.
Do NOT implement the paper's NUMA-specific Algorithm 1 or Algorithm 2.

The only target is the TILEPro64 manycore architecture and the locality-aware scheduling/data-distribution mechanism described for TILEPro64.

The attached paper is the primary source of truth.

============================================================
                         GOAL
============================================================

Build a C-based software model of the TILEPro64 architecture described in the paper and implement the paper's TILEPro64 locality-aware task scheduler.

The goal is:

    TILEPro64 architecture model
             +
    paper's TILEPro64 data distribution
             +
    paper's TILEPro64 locality-aware work dealing
             +
    paper's TILEPro64 work finding / stealing
             +
    paper's vicinity mechanism
             +
    paper's benchmark behavior

Nothing beyond what is required to reproduce the TILEPro64 part of the paper.

This is NOT intended to be a cycle-accurate TILEPro64 hardware emulator.

It is a software simulation/model of the architectural and scheduling abstractions used by the paper.

============================================================
                 1. TILEPRO64 ONLY
============================================================

The paper discusses two architectural environments:

1. NUMA systems
2. Manycore processors such as TILEPro64

For this implementation, completely ignore #1.

The implementation should contain ONLY:

    TILEPro64 / manycore

Do not create:

    NUMA nodes
    NUMA distances
    libnuma allocations
    AMD Opteron model
    NUMA memory pages
    NUMA benchmark results

The simulated machine must instead be the TILEPro64 architecture described in the paper.

============================================================
                 2. TILEPRO64 ARCHITECTURE
============================================================

Implement the TILEPro64 architecture described in the paper.

The architecture consists of:

    64 tiles
    arranged as an 8 × 8 mesh

Each tile should be represented explicitly.

Each tile should contain the architectural concepts relevant to the paper:

    - tile ID
    - core/worker
    - private L1 caches as an architectural concept
    - a portion/bank of the shared L2 cache
    - home-cache identity
    - position in the mesh
    - task queue

Do NOT attempt to simulate actual cache replacement, cache coherence, instructions, pipelines, or individual CPU cycles unless the paper requires them.

Only model the architectural properties that the scheduler uses.

============================================================
                 3. 8 × 8 MESH
============================================================

Represent the 64 tiles as:

    8 × 8 mesh

For example:

    Tile 0   Tile 1   Tile 2   ... Tile 7
    Tile 8   Tile 9   Tile 10  ... Tile 15
    ...
    Tile 56  Tile 57  ...       Tile 63

Each tile must have:

    id
    x
    y

Implement the mesh topology explicitly.

Create functions for determining the relationship between two tiles.

Do NOT assume that the TILEPro64 architecture is equivalent to 64 NUMA nodes.

It is not.

============================================================
                 4. HOME CACHE
============================================================

This is one of the most important concepts.

Implement the paper's TILEPro64 home-cache model.

Data/cache lines have a home cache.

The scheduler uses the location of the task's data/home caches to determine where the task should execute.

Represent the home cache associated with each data unit.

Conceptually:

    data
      ↓
    cache line
      ↓
    home cache
      ↓
    tile

A task therefore has a distribution of data across home caches.

Represent this distribution as something equivalent to:

    D[64]

where:

    D[i] = amount of the task's relevant data homed at tile/home-cache i

Use the paper's terminology.

============================================================
                 5. HOME-CACHE ACCESS LATENCY
============================================================

The TILEPro64 architecture has non-uniform access costs.

A core accessing its local/nearby home cache has a different cost from accessing a distant home cache.

Implement the latency information required by the paper's scheduler.

Use the actual information provided in the paper wherever available.

If the paper does not provide enough numerical information to reconstruct the complete hardware latency matrix, create a configurable latency matrix:

    latency[64][64]

and clearly document that the missing numerical values are simulation parameters.

Do NOT claim that an invented latency matrix is the actual TILEPro64 hardware latency.

The important requirement is that the scheduler must be able to calculate:

    cost of executing task T on tile X

based on:

    task data distribution
            +
    access latency from X to each relevant home cache

============================================================
                 6. TASK MODEL
============================================================

Implement the task abstraction required by the TILEPro64 scheduler.

Each task should contain:

    task ID
    execution/work information
    dependencies
    data footprint
    data distribution across home caches
    scheduler state

The task must expose enough information for the scheduler to determine:

    D[64]

for that task.

Do not implement additional task metadata that has no purpose in the paper.

============================================================
                 7. DATA DISTRIBUTION
============================================================

Implement ONLY the data-distribution mechanisms used in the TILEPro64 part of the paper.

In particular, implement the distribution approaches discussed by the paper for manycore/TILEPro64:

    - fine distribution
    - coarse distribution

If the paper uses standard/default distribution as a comparison in the TILEPro64 experiments, implement it only for that comparison.

Do NOT add:

    block-cyclic distribution
    adaptive distribution
    dynamic page migration
    replication
    machine-learning distribution
    access-pattern prediction
    any proposed improvement

The implementation should reproduce what the paper actually evaluates.

============================================================
                 8. FINE DISTRIBUTION
============================================================

Implement the paper's definition of fine data distribution.

Data units/cache lines should be distributed among the available home caches according to the paper.

Do not invent a different policy.

Document precisely:

    data unit
        ↓
    home cache
        ↓
    tile

============================================================
                 9. COARSE DISTRIBUTION
============================================================

Implement the paper's coarse distribution.

The relevant allocation should remain associated with a home cache/tile according to the paper.

The scheduler must then be able to determine where the task's data resides.

Again:

    follow the paper
    do not invent a different allocation scheme

============================================================
                 10. TASK DATA DISTRIBUTION
============================================================

For each task calculate:

    D[64]

For example:

    D[0] = amount of task data at home cache 0
    D[1] = amount at home cache 1
    ...
    D[63] = amount at home cache 63

This is the information used by the locality-aware scheduler.

If a task accesses data primarily associated with one home cache, the distribution should reflect that.

If it accesses data across several home caches, all relevant entries must be represented.

============================================================
                 11. LOCALITY COST
============================================================

Implement the TILEPro64 locality/access-cost calculation described in the paper.

Conceptually:

    task data distribution
             +
    home-cache access latency
             ↓
    communication/access cost
             ↓
    preferred tile/home-cache queue

Use the paper's actual formula/definition wherever specified.

Do not replace the paper's cost model with a generic NUMA formula.

This is TILEPro64.

============================================================
                 12. TASK QUEUES
============================================================

Create one scheduler queue associated with each tile/home cache.

Therefore:

    Queue 0  → Tile 0
    Queue 1  → Tile 1
    ...
    Queue 63 → Tile 63

The scheduler must be able to:

    enqueue task
    execute local task
    inspect queue
    steal task
    determine queue size

Use a simple correct queue implementation initially.

The objective is to reproduce the paper's scheduling behavior, not to create a highly optimized production scheduler.

============================================================
                 13. TILEPRO64 WORK DEALING
============================================================

Implement the TILEPro64 work-dealing algorithm from the paper.

This is the algorithm that decides where a newly created/ready task should be placed.

The conceptual flow is:

    task created
          ↓
    determine data footprint
          ↓
    determine home-cache distribution
          ↓
    calculate locality/access cost
          ↓
    determine preferred queue/home cache
          ↓
    enqueue task

IMPORTANT:

Do NOT simply implement a generic:

    argmin(distance)

scheduler.

Read the TILEPro64 algorithm in the paper and implement its actual logic, including any special handling described for coarse/fine distribution and task data dependencies.

============================================================
                 14. TILEPRO64 WORK FINDING
============================================================

Implement the TILEPro64 work-finding/work-stealing mechanism from the paper.

When a tile's worker has no local task:

    1. inspect its local queue
    2. determine eligible remote queues
    3. respect vicinity
    4. search according to the paper
    5. steal work if allowed
    6. execute the stolen task

Do NOT implement generic unrestricted work stealing.

The paper's locality restrictions are part of the algorithm.

============================================================
                 15. VICINITY
============================================================

Implement the TILEPro64 vicinity mechanism exactly as described by the paper.

The paper evaluates different vicinity sizes.

The implementation must therefore allow the vicinity to be configured.

For example, if the paper evaluates particular values, allow those values.

The vicinity should restrict which queues a worker is permitted to steal from.

IMPORTANT:

Do NOT make vicinity adaptive.

Do NOT implement H1.

Do NOT dynamically change vicinity based on load.

The goal is to reproduce the paper's fixed vicinity mechanism.

============================================================
                 16. NO H1-H7
============================================================

Do NOT implement any of the following:

    H1 adaptive steal reach
    H2 task-level victim selection
    H3 load-aware placement
    H4 continuous locality thresholds
    H5 improved footprint inference
    H6 feedback-based redistribution
    H7 additional distribution policies

Those are NOT part of this implementation.

This project is the baseline reproduction of the paper.

============================================================
                 17. BENCHMARKS
============================================================

Implement ONLY the TILEPro64 benchmarks/workloads from the paper that are necessary to reproduce its manycore experiments.

Pay particular attention to the benchmarks used in the TILEPro64 section and the experiments involving:

    - Map
    - Vecmul

If the paper includes additional TILEPro64 benchmarks, implement those as well.

Do not implement the paper's NUMA-only benchmark setup.

The benchmark should generate the same type of task/data locality described in the paper.

============================================================
                 18. BASELINES
============================================================

Implement the comparison scheduling policies that are actually used in the TILEPro64 experiments.

At minimum, if the paper compares:

    work stealing
    locality-aware scheduling

implement both.

Also implement the relevant data-distribution configurations used in those experiments.

Do not introduce additional schedulers.

============================================================
                 19. EXPERIMENTAL PARAMETERS
============================================================

Make the TILEPro64 parameters configurable.

At minimum:

    number of tiles = 64
    mesh = 8 × 8
    distribution policy
    scheduler
    vicinity size
    benchmark size

The simulated architecture should always represent the TILEPro64 machine described by the paper.

The host machine's number of CPU cores must NOT change the simulated architecture.

============================================================
                 20. SIMULATION VS HARDWARE
============================================================

This is a software model running on my ordinary computer.

Do not claim that the laptop is actually executing TILEPro64 instructions.

Separate:

    simulated TILEPro64 behavior

from:

    host C program execution time

The goal is to reproduce the paper's scheduling decisions and architectural locality behavior.

Exact hardware performance numbers should only be claimed if the paper provides sufficient information to model them.

============================================================
                 21. VALIDATION
============================================================

Before running benchmarks, create small tests for:

1. 64 tiles exist.

2. Tiles form an 8 × 8 mesh.

3. Each tile has one queue.

4. Data can be assigned to home caches.

5. A task's D[64] distribution is correct.

6. Locality cost is calculated correctly.

7. A task is placed according to the TILEPro64 work-dealing algorithm.

8. A worker can execute from its local queue.

9. A worker can steal only from queues allowed by its vicinity.

10. Task dependencies are respected.

11. Every task executes exactly once.

============================================================
                 22. CODE STRUCTURE
============================================================

Keep the implementation simple.

Suggested structure:

    src/
        tilepro64.h
        tilepro64.c

        home_cache.h
        home_cache.c

        latency.h
        latency.c

        data_distribution.h
        data_distribution.c

        task.h
        task.c

        queue.h
        queue.c

        scheduler.h
        scheduler.c

        benchmark_map.c
        benchmark_vecmul.c

        main.c

    tests/

    README.md

Do not create unnecessary abstractions.

============================================================
                 23. README
============================================================

Create a README explaining:

1. What part of the paper is implemented.
2. That this implementation targets ONLY TILEPro64/manycore.
3. That NUMA/AMD Opteron experiments are intentionally excluded.
4. How TILEPro64 is represented.
5. How home caches are represented.
6. How data distribution works.
7. How locality-aware work dealing works.
8. How vicinity-based work finding works.
9. How to compile.
10. How to run the TILEPro64 benchmarks.
11. Which parts are exact paper mechanisms.
12. Which parts are implementation assumptions due to information unavailable in the paper.

============================================================
                 24. MOST IMPORTANT RULE
============================================================

Do not improve the paper.

Do not redesign the scheduler.

Do not add adaptive behavior.

Do not add H1-H7.

Do not implement NUMA.

Do not implement AMD Opteron.

Do not create a generalized manycore simulator.

Implement the TILEPro64 portion of the attached paper as faithfully and minimally as possible.

When there is ambiguity:

    paper → exact text/algorithm/figure
            ↓
    minimal implementation assumption
            ↓
    document the assumption

The final result should be a clean baseline implementation of the TILEPro64 architecture and locality-aware scheduling approach presented in the paper.

============================================================
                 25. PROJECT DIRECTORY
============================================================

Create a new directory named:

    tilepro64/

ALL files and work related to this implementation must be
contained inside this directory.

Do not scatter source files, scripts, benchmark files, results,
documentation, or generated artifacts outside tilepro64/.

Use a clean structure such as:

    tilepro64/
    ├── src/
    ├── tests/
    ├── benchmarks/
    ├── results/
    ├── scripts/
    ├── docs/
    ├── Makefile
    └── README.md

You may adjust the exact structure if necessary, but everything
belonging to this implementation must remain inside:

    tilepro64/

============================================================
                 26. EXPERIMENT RESULTS
============================================================

After implementing and validating the TILEPro64 architecture and
the scheduling mechanisms from the paper, run the relevant
TILEPro64 experiments.

The experiments must generate actual output from the implemented
simulator.

Do NOT manually enter experimental values.

Do NOT fabricate values.

Do NOT use values from external examples.

Do NOT hard-code expected performance numbers.

All reported values must come from actual executions of the
implemented simulator.

Store the raw experimental output inside:

    tilepro64/results/

For example:

    tilepro64/results/raw_results.csv

The exact filename may differ, but the raw experimental data
must be preserved.

============================================================
                 27. RESULT DATA
============================================================

The result file must contain enough information to regenerate
the final visualization without rerunning the experiments.

Include only metrics that are actually produced by the
implementation.

Where applicable, record:

    benchmark
    scheduler
    data_distribution
    vicinity
    simulated_communication_cost
    simulated_execution_time
    task_count
    steal_count
    local_execution_count
    remote_execution_count

Do not create columns for metrics that the simulator does not
actually calculate.

The results must clearly identify the configuration under which
each measurement was obtained.

============================================================
                 28. RESULT CHART
============================================================

After the experiments complete, automatically generate a chart
from the actual result data.

The chart must be created from:

    tilepro64/results/raw_results.csv

or the corresponding raw result file produced by the simulator.

Do not manually enter values into the plotting script.

The plotting script must read the experimental output and
generate the visualization programmatically.

Create the plotting script inside:

    tilepro64/scripts/

For example:

    tilepro64/scripts/plot_results.py

The generated chart must be saved inside:

    tilepro64/results/

For example:

    tilepro64/results/tilepro64_results.png

The exact filename may be chosen by the implementation.

============================================================
                 29. RESULT VISUALIZATION
============================================================

Create a clear grouped-bar-chart visualization of the actual
TILEPro64 experimental results.

The chart should make it easy to compare the scheduling and
data-distribution configurations that are actually evaluated in
the TILEPro64 portion of the paper.

Where applicable, distinguish:

    locality-aware scheduling

from:

    work-stealing baseline

and distinguish the data-distribution policies actually used by
the TILEPro64 experiments.

Use clear labels for each configuration.

For example, if these configurations are actually present in
the experiments:

    LA + Coarse
    LA + Fine
    WS + Coarse
    WS + Fine

then display them clearly.

Do NOT add configurations that are not part of the paper.

============================================================
                 30. CHART METRICS
============================================================

If the simulator produces multiple meaningful metrics, present
them clearly.

For example, if both of the following are actually generated:

    modeled communication/access cost

and:

    simulated execution time

they may be displayed as separate panels in the same figure.

For example:

    ┌─────────────────────────┬─────────────────────────┐
    │ Communication / Access  │ Simulated Execution     │
    │ Cost                    │ Time                    │
    │                         │                         │
    │ grouped bars            │ grouped bars            │
    │                         │                         │
    └─────────────────────────┴─────────────────────────┘

However, do NOT create a metric merely for the purpose of
making the chart look complete.

If only one meaningful metric is produced, create a single
appropriate chart.

Use accurate terminology.

For example:

    "Simulated Execution Time"

must be used instead of:

    "Real Execution Time"

when the experiment is being executed by the simulator on a
normal computer.

============================================================
                 31. NORMALIZATION
============================================================

If the paper's TILEPro64 experiment uses normalized results,
reproduce the paper's normalization method.

Do not introduce arbitrary normalization.

If normalization is not required by the corresponding paper
experiment, display the actual measured/simulated values.

Any normalization performed must be documented in:

    tilepro64/scripts/plot_results.py

and:

    tilepro64/README.md

============================================================
                 32. AUTOMATED WORKFLOW
============================================================

The complete workflow should be reproducible.

The intended workflow is:

    1. Compile the simulator
              ↓
    2. Run the TILEPro64 experiments
              ↓
    3. Generate raw result data
              ↓
    4. Generate the result chart
              ↓
    5. Save the chart in tilepro64/results/

For example:

    cd tilepro64

    make

    ./tilepro64 ...

    python scripts/plot_results.py

If a different command structure is more appropriate, document
it clearly in README.md.

Ideally provide a convenient command that performs the complete
experiment + result-generation workflow.

For example:

    make experiment

or:

    make results

The exact command is up to the implementation.

============================================================
                 33. RESULT INTEGRITY
============================================================

The final chart must contain ONLY values generated from the
actual implementation.

Never:

    - manually type performance values into the chart
    - fabricate missing results
    - estimate results that were not measured
    - copy values from another experiment
    - claim actual TILEPro64 hardware performance when running
      the simulator on a normal computer

If an experiment cannot be reproduced because the paper does not
provide sufficient information, document that limitation rather
than inventing a value.

Clearly distinguish between:

    paper-reported result

and:

    result produced by this implementation.

============================================================
                 34. FINAL DELIVERABLE
============================================================

The final project must contain:

    tilepro64/
    ├── src/
    │   └── TILEPro64 implementation
    │
    ├── tests/
    │   └── correctness tests
    │
    ├── benchmarks/
    │   └── TILEPro64 workloads
    │
    ├── results/
    │   ├── raw experimental data
    │   └── generated result chart
    │
    ├── scripts/
    │   └── result-generation / plotting script
    │
    ├── docs/
    │   └── implementation documentation
    │
    ├── Makefile
    │
    └── README.md

The README must explain:

    - how to compile the implementation
    - how to run the TILEPro64 experiments
    - how raw results are generated
    - how the result chart is generated
    - where the final chart is located
    - what each plotted metric represents
    - which paper mechanisms were implemented

The final chart must be located in:

    tilepro64/results/

and must be generated automatically from the actual experimental
output.

============================================================
                 FINAL SCOPE REMINDER
============================================================

Implement ONLY the TILEPro64 portion of the paper.

Do NOT implement the NUMA/AMD Opteron portion.

Do NOT implement H1-H7 improvements.

Do NOT implement new scheduling algorithms.

Do NOT invent additional data-distribution policies.

The attached paper is the specification for the implementation.

The final output of this project is:

    TILEPro64 implementation
             +
    actual experimental results
             +
    generated result chart

Everything must be contained inside:

    tilepro64/