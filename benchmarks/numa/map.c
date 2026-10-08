/* Map benchmark: one task per vector, each task scales its own vector in place.
 * Mirrors the base paper's Listing 1 / Figure 7 Map input: 48 FP vectors, 1 MB each.
 * Heuristic guidance (Table 3): coarse distribution (one malloc, many tasks -> actually many
 * mallocs since each vector is its own allocation -> coarse). Selected via MIR_CONF -m, not here.
 * See docs/14-numa-opteron-implementation-plan.md Step 3 / Section 6.2.
 */
#include <stdlib.h>
#include <stdio.h>
#include "mir_public_int.h"
#include "bench_common.h"

#define N_VECS 48
/* 4 MB, not the paper's 1 MB: Algorithm 1's significance gate requires a task's whole footprint
 * to exceed sizeof(LLC)/C. Our topology runs 3 cores/node (24 total, re-provisioned to fit this
 * host -- see docs/14 Step 1) instead of the paper's real 6, so LLC/C here is ~1.63 MB
 * (5000KB/3), not the paper's ~833KB (5000KB/6). A 1 MB vector clears the paper's threshold but
 * not ours; 4 MB clears both. See docs/14-numa-opteron-implementation-plan.md Step 3 build log.
 */
#define VEC_BYTES (4 * 1024 * 1024)
#define VEC_ELEMS (VEC_BYTES / sizeof(float))

static float* vecs[N_VECS];

typedef struct {
    float* vec;
    size_t n;
} map_task_args_t;

static void* map_task_fn(void* data)
{
    map_task_args_t* args = (map_task_args_t*)data;
    for (size_t i = 0; i < args->n; i++)
        args->vec[i] = args->vec[i] * 2.0f + 1.0f;
    return NULL;
}

void bench_init(void)
{
    for (int i = 0; i < N_VECS; i++) {
        vecs[i] = (float*)mir_mem_pol_allocate(VEC_BYTES);
        for (size_t j = 0; j < VEC_ELEMS; j++)
            vecs[i][j] = (float)j;
    }
}

void bench_run(void)
{
    for (int i = 0; i < N_VECS; i++) {
        map_task_args_t args = { .vec = vecs[i], .n = VEC_ELEMS };
        /* type = bytes/unit; start/end are UNIT indices (inclusive), sz = (end-start+1)*type.
         * type=1, end=VEC_BYTES-1 expresses "the whole allocation" at byte granularity. */
        struct mir_data_footprint_t fp = {
            .base = vecs[i],
            .type = 1,
            .start = 0,
            .end = VEC_BYTES - 1,
            .row_sz = 0,
            .data_access = MIR_DATA_ACCESS_READ,
            .part_of = vecs[i]
        };
        mir_task_create(map_task_fn, &args, sizeof(args), 1, &fp, "map");
    }
    mir_task_wait();
}

int bench_check(void)
{
    int ok = 1;
    for (int i = 0; i < N_VECS && ok; i++) {
        /* Sample a few elements; full scan is unnecessary for a smoke check. */
        for (size_t j = 0; j < VEC_ELEMS; j += (VEC_ELEMS / 8)) {
            float expected = (float)j * 2.0f + 1.0f;
            if (vecs[i][j] != expected) {
                fprintf(stderr, "bench_check: vec %d elem %zu expected %f got %f\n",
                        i, j, expected, vecs[i][j]);
                ok = 0;
                break;
            }
        }
    }
    return ok;
}

void bench_teardown(void)
{
    for (int i = 0; i < N_VECS; i++)
        mir_mem_pol_release(vecs[i], VEC_BYTES);
}
