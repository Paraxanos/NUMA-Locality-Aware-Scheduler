/* Reduction benchmark: single malloc, many tasks each reduce their own slice. Table 3 guidance:
 * fine (one malloc, many tasks -> fine). This is the paper's single most instructive case: under
 * coarse, the WHOLE array lands on one node, so a locality-aware scheduler "faithfully" deals
 * every task to that one node's queue -- perfect locality, zero parallelism. See
 * docs/00-study-guide.md "Reduction" section.
 *
 * Simplified from the paper's actual benchmark (the merge phase of BOTS Sort) to a flat parallel
 * sum over chunks of one array -- same locality-relevant structure (one malloc, many tasks on
 * disjoint slices), much simpler to implement and correctness-check. Documented substitution, not
 * silent; see docs/14-numa-opteron-implementation-plan.md Step 4 Reduction build log.
 *
 * Each chunk is sized to clear this topology's significance threshold (~1.63MB) alone, so the
 * coarse-disaster case genuinely exercises push_numa()'s comparison logic rather than silently
 * no-op'ing through the significance gate (see Map bug #2).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "mir_public_int.h"
#include "bench_common.h"

#define N_CHUNKS 48
#define CHUNK_BYTES (5 * 1024 * 1024) /* 48*5MB=240MB, close to the paper's 256MB (§00 "Inputs,
                                         Figure 7"); comfortably clears the significance threshold */
#define CHUNK_ELEMS (CHUNK_BYTES / sizeof(float))
#define TOTAL_ELEMS ((size_t)N_CHUNKS * CHUNK_ELEMS)
#define TOTAL_BYTES ((size_t)N_CHUNKS * CHUNK_BYTES)

static float* big_array; /* single malloc for the whole array -- the structural property that
                             drives Table 3's "fine" guidance and the coarse failure mode */
static double partial_sum[N_CHUNKS];

typedef struct {
    int chunk;
} reduction_args_t;

static void* reduction_task_fn(void* data)
{
    int chunk = ((reduction_args_t*)data)->chunk;
    float* p = big_array + (size_t)chunk * CHUNK_ELEMS;
    double sum = 0.0;
    for (size_t i = 0; i < CHUNK_ELEMS; i++)
        sum += p[i];
    partial_sum[chunk] = sum;
    return NULL;
}

void bench_init(void)
{
    big_array = (float*)mir_mem_pol_allocate(TOTAL_BYTES);
    for (size_t i = 0; i < TOTAL_ELEMS; i++)
        big_array[i] = 1.0f;
}

void bench_run(void)
{
    for (int c = 0; c < N_CHUNKS; c++) {
        reduction_args_t args = { .chunk = c };
        /* .base is this task's actual slice; .part_of MUST be the allocation's exact base
         * pointer (not an offset into it) -- that's what the header lookup in
         * mir_mem_get_mem_node_dist() keys on. An offset pointer silently falls through to a
         * real-OS NUMA query instead of our simulated distribution. */
        struct mir_data_footprint_t fp = {
            .base = big_array + (size_t)c * CHUNK_ELEMS,
            .type = 1,
            .start = 0,
            .end = CHUNK_BYTES - 1,
            .row_sz = 0,
            .data_access = MIR_DATA_ACCESS_READ,
            .part_of = big_array
        };
        mir_task_create(reduction_task_fn, &args, sizeof(args), 1, &fp, "reduction");
    }
    mir_task_wait();
}

int bench_check(void)
{
    double total = 0.0;
    for (int c = 0; c < N_CHUNKS; c++)
        total += partial_sum[c];
    double expected = (double)TOTAL_ELEMS;
    double rel_err = fabs(total - expected) / expected;
    if (rel_err > 1e-6) {
        fprintf(stderr, "bench_check: expected %f got %f\n", expected, total);
        return 0;
    }
    return 1;
}

void bench_teardown(void)
{
    mir_mem_pol_release(big_array, TOTAL_BYTES);
}
