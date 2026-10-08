/* Matmul benchmark: blocked matrix multiply, one task per output block.
 * Each task's footprint spans a chain of blocks from both input matrices (row-strip of A,
 * col-strip of B) -- the structural reason Matmul's locality gain is smaller than Map's
 * (no single node can hold everything a task needs). See docs/00-study-guide.md Part 5.
 *
 * Block size 128 matches the paper's Figure 7 input. Matrix dimension (NB blocks/side) is
 * NOT the paper's literal 4096 (NB=32) for this first validation pass -- at NB=32 each task's
 * footprint (64 blocks x 64KB = 4MB) comfortably clears this topology's significance threshold,
 * but the total compute (NB^3 * BLOCK^3 ~ 68B mult-adds) is a slow first smoke test. NB=16
 * (2048x2048) keeps per-task footprint at 2MB -- still safely above the ~1.63MB threshold that
 * caught Map's bug #2 -- while running in a few seconds. Scaling to NB=32 is the "expand matrix"
 * stretch step (docs/14-numa-opteron-implementation-plan.md Step 4 §10 checklist).
 *
 * Naive triple-loop block multiply, not BLAS -- equivalent locality/scheduling behavior, much
 * less setup, fine for a validation benchmark.
 */
#include <stdlib.h>
#include <stdio.h>
#include "mir_public_int.h"
#include "bench_common.h"

#define BLOCK 128
#define NB 16
#define BLOCK_ELEMS (BLOCK * BLOCK)
#define BLOCK_BYTES (BLOCK_ELEMS * sizeof(float))

static float* A_blk[NB * NB];
static float* B_blk[NB * NB];
static float* C_blk[NB * NB];

typedef struct {
    int i;
    int j;
} matmul_task_args_t;

static void* matmul_task_fn(void* data)
{
    matmul_task_args_t* a = (matmul_task_args_t*)data;
    int i = a->i, j = a->j;
    float* C = C_blk[i * NB + j];

    for (int x = 0; x < BLOCK_ELEMS; x++)
        C[x] = 0.0f;

    for (int k = 0; k < NB; k++) {
        float* A = A_blk[i * NB + k];
        float* B = B_blk[k * NB + j];
        for (int r = 0; r < BLOCK; r++) {
            for (int c = 0; c < BLOCK; c++) {
                float sum = 0.0f;
                for (int m = 0; m < BLOCK; m++)
                    sum += A[r * BLOCK + m] * B[m * BLOCK + c];
                C[r * BLOCK + c] += sum;
            }
        }
    }
    return NULL;
}

void bench_init(void)
{
    for (int idx = 0; idx < NB * NB; idx++) {
        A_blk[idx] = (float*)mir_mem_pol_allocate(BLOCK_BYTES);
        B_blk[idx] = (float*)mir_mem_pol_allocate(BLOCK_BYTES);
        C_blk[idx] = (float*)mir_mem_pol_allocate(BLOCK_BYTES);
        for (int x = 0; x < BLOCK_ELEMS; x++) {
            A_blk[idx][x] = 1.0f;
            B_blk[idx][x] = 1.0f;
            C_blk[idx][x] = 0.0f;
        }
    }
}

void bench_run(void)
{
    for (int i = 0; i < NB; i++) {
        for (int j = 0; j < NB; j++) {
            matmul_task_args_t args = { .i = i, .j = j };

            /* Footprint: the i-th row-strip of A (NB blocks) + the j-th col-strip of B
             * (NB blocks). Each block is its own allocation (coarse round-robins per
             * allocation), so each needs its own footprint entry -- can't express a
             * multi-block strip as one contiguous region. */
            struct mir_data_footprint_t fp[2 * NB];
            for (int k = 0; k < NB; k++) {
                fp[k] = (struct mir_data_footprint_t){
                    .base = A_blk[i * NB + k], .type = 1, .start = 0,
                    .end = BLOCK_BYTES - 1, .row_sz = 0,
                    .data_access = MIR_DATA_ACCESS_READ, .part_of = A_blk[i * NB + k]
                };
                fp[NB + k] = (struct mir_data_footprint_t){
                    .base = B_blk[k * NB + j], .type = 1, .start = 0,
                    .end = BLOCK_BYTES - 1, .row_sz = 0,
                    .data_access = MIR_DATA_ACCESS_READ, .part_of = B_blk[k * NB + j]
                };
            }

            mir_task_create(matmul_task_fn, &args, sizeof(args), 2 * NB, fp, "matmul");
        }
    }
    mir_task_wait();
}

int bench_check(void)
{
    int ok = 1;
    float expected = (float)(NB * BLOCK); /* each element: NB blocks x BLOCK ones-dot-ones */
    for (int idx = 0; idx < NB * NB && ok; idx++) {
        for (int x = 0; x < BLOCK_ELEMS; x += (BLOCK_ELEMS / 4)) {
            if (C_blk[idx][x] != expected) {
                fprintf(stderr, "bench_check: block %d elem %d expected %f got %f\n",
                        idx, x, expected, C_blk[idx][x]);
                ok = 0;
                break;
            }
        }
    }
    return ok;
}

void bench_teardown(void)
{
    for (int idx = 0; idx < NB * NB; idx++) {
        mir_mem_pol_release(A_blk[idx], BLOCK_BYTES);
        mir_mem_pol_release(B_blk[idx], BLOCK_BYTES);
        mir_mem_pol_release(C_blk[idx], BLOCK_BYTES);
    }
}
