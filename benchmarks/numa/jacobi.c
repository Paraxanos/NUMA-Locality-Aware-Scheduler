/* Jacobi benchmark: blocked 2-D 5-point stencil, one sweep. Table 3 heuristic: one malloc (the
 * whole grid), many tasks -> fine (see docs/00-study-guide.md §3.4). Each task reads its own
 * block plus its 4 neighbours (halo) and writes its own output block -- a regular but SHARED
 * access pattern, which is why the paper's NUMA story for Jacobi is unusual: work-stealing with
 * EITHER fine or coarse runs ~2.6x slower than baseline, and only locality-aware scheduling
 * recovers it (~1.0) -- distribution policy alone isn't enough when neighbours overlap; the
 * SCHEDULER has to place tasks near the shared data. See docs/00-study-guide.md "Jacobi" section.
 *
 * Single sweep (not iterated to convergence) -- this benchmark's role here is to exercise the
 * locality/scheduling behaviour of one large task-parallel stencil region, same spirit as the
 * other NUMA benchmarks, not to solve anything. Block size 512 matches the paper's Figure 7 input
 * exactly (so the significance gate sees the same per-task footprint shape); grid is NB=8 blocks
 * per side (4096x4096), not the paper's NB=32 (16384x16384) -- same scale-down reasoning as
 * Map/Matmul's validation-pass sizing. Dirichlet (zero) boundary outside the grid.
 */
#include <stdio.h>
#include <stdlib.h>
#include "mir_public_int.h"
#include "bench_common.h"

#define BLOCK 512
#define NB 8
#define BLOCK_ELEMS (BLOCK * BLOCK)
#define BLOCK_BYTES (BLOCK_ELEMS * sizeof(float))

static float* grid_cur;  /* single malloc for the whole grid -- Table 3's "one malloc" case */
static float* grid_next;

typedef struct {
    int bi;
    int bj;
} jacobi_args_t;

static inline float get_val(int bi, int bj, int r, int c)
{
    if (r < 0) { bi--; r = BLOCK - 1; }
    else if (r >= BLOCK) { bi++; r = 0; }
    if (c < 0) { bj--; c = BLOCK - 1; }
    else if (c >= BLOCK) { bj++; c = 0; }
    if (bi < 0 || bi >= NB || bj < 0 || bj >= NB)
        return 0.0f; /* Dirichlet boundary */
    return grid_cur[(bi * NB + bj) * BLOCK_ELEMS + r * BLOCK + c];
}

static void* jacobi_task_fn(void* data)
{
    jacobi_args_t* a = (jacobi_args_t*)data;
    int bi = a->bi, bj = a->bj;
    float* out = grid_next + (size_t)(bi * NB + bj) * BLOCK_ELEMS;

    for (int r = 0; r < BLOCK; r++) {
        for (int c = 0; c < BLOCK; c++) {
            float up = get_val(bi, bj, r - 1, c);
            float down = get_val(bi, bj, r + 1, c);
            float left = get_val(bi, bj, r, c - 1);
            float right = get_val(bi, bj, r, c + 1);
            out[r * BLOCK + c] = 0.25f * (up + down + left + right);
        }
    }
    return NULL;
}

void bench_init(void)
{
    size_t total_bytes = (size_t)NB * NB * BLOCK_BYTES;
    grid_cur = (float*)mir_mem_pol_allocate(total_bytes);
    grid_next = (float*)mir_mem_pol_allocate(total_bytes);
    size_t total_elems = (size_t)NB * NB * BLOCK_ELEMS;
    for (size_t i = 0; i < total_elems; i++) {
        grid_cur[i] = 1.0f;
        grid_next[i] = 0.0f;
    }
}

void bench_run(void)
{
    for (int bi = 0; bi < NB; bi++) {
        for (int bj = 0; bj < NB; bj++) {
            jacobi_args_t args = { .bi = bi, .bj = bj };

            /* Own block + up to 4 neighbours (fewer at grid edges), each a READ footprint into
             * grid_cur -- .part_of is always grid_cur's exact base (single allocation). */
            struct mir_data_footprint_t fp[5];
            int n = 0;
            int nb_bi[5] = { bi, bi - 1, bi + 1, bi, bi };
            int nb_bj[5] = { bj, bj, bj, bj - 1, bj + 1 };
            for (int k = 0; k < 5; k++) {
                if (nb_bi[k] < 0 || nb_bi[k] >= NB || nb_bj[k] < 0 || nb_bj[k] >= NB)
                    continue;
                float* blk = grid_cur + (size_t)(nb_bi[k] * NB + nb_bj[k]) * BLOCK_ELEMS;
                fp[n++] = (struct mir_data_footprint_t){
                    .base = blk, .type = 1, .start = 0, .end = BLOCK_BYTES - 1, .row_sz = 0,
                    .data_access = MIR_DATA_ACCESS_READ, .part_of = grid_cur
                };
            }

            mir_task_create(jacobi_task_fn, &args, sizeof(args), n, fp, "jacobi");
        }
    }
    mir_task_wait();
}

int bench_check(void)
{
    /* Uniform 1.0 field is a fixed point of the 5-point stencil away from the boundary:
     * 0.25*(1+1+1+1) = 1.0. Check blocks fully in the interior (not touching the grid edge). */
    int ok = 1;
    for (int bi = 1; bi < NB - 1 && ok; bi++) {
        for (int bj = 1; bj < NB - 1 && ok; bj++) {
            float* out = grid_next + (size_t)(bi * NB + bj) * BLOCK_ELEMS;
            for (int x = 0; x < BLOCK_ELEMS; x += (BLOCK_ELEMS / 4)) {
                if (out[x] != 1.0f) {
                    fprintf(stderr, "bench_check: block (%d,%d) elem %d expected 1.0 got %f\n",
                            bi, bj, x, out[x]);
                    ok = 0;
                    break;
                }
            }
        }
    }
    return ok;
}

void bench_teardown(void)
{
    size_t total_bytes = (size_t)NB * NB * BLOCK_BYTES;
    mir_mem_pol_release(grid_cur, total_bytes);
    mir_mem_pol_release(grid_next, total_bytes);
}
