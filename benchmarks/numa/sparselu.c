/* SparseLU benchmark: blocked LU factorization (right-looking, no pivoting -- matching BOTS
 * SparseLU's own approach), one block per allocation. Table 3: many mallocs, many tasks ->
 * coarse. The paper's own counter-example: SparseLU's blocks are shared irregularly and
 * data-dependently *across factorization steps*, violating the "tasks work on allocations in
 * isolation" assumption Table 3's coarse recommendation rests on -- so locality-aware scheduling
 * merely MAINTAINS performance on NUMA here, it doesn't win outright. See
 * docs/00-study-guide.md "SparseLU" section.
 *
 * Simplified from genuine block-sparse (some blocks randomly absent, as BOTS does) to dense --
 * documented substitution. The property that matters for locality evaluation is the complex,
 * data-dependent, multi-step block reuse pattern (lu0 -> fwd/bdiv -> bmod, repeated per step),
 * which dense blocked LU already has in full; genuine sparsity would add implementation risk
 * (NULL-block bookkeeping through every kernel) without changing that structural story.
 *
 * Real task dependencies, unlike every other benchmark ported so far (Map/Matmul/Jacobi/Reduction
 * are all independent, single mir_task_wait() at the end). Step k's fwd/bdiv need lu0(k,k) done;
 * step k's bmod needs that step's fwd/bdiv done. MIR has no dataflow/depend-clause tracking
 * (confirmed absent from the public source during Map/Matmul work), so dependencies are enforced
 * with mir_task_wait() as a barrier between phases -- coarser-grained than true dataflow
 * scheduling would allow, but correct, and simple.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "mir_public_int.h"
#include "bench_common.h"

#define BLOCK 512
#define NB 8
#define BLOCK_ELEMS (BLOCK * BLOCK)
#define BLOCK_BYTES (BLOCK_ELEMS * sizeof(float))

static float* A_blk[NB * NB];
static float orig00[BLOCK_ELEMS]; /* saved copy of block (0,0) before factorization, for the
                                      L*U reconstruction check */

/* ---- dense kernels, standard blocked LU, no pivoting ---- */

static void lu0(float* A, int n)
{
    for (int k = 0; k < n; k++) {
        for (int i = k + 1; i < n; i++) {
            A[i * n + k] /= A[k * n + k];
            for (int j = k + 1; j < n; j++)
                A[i * n + j] -= A[i * n + k] * A[k * n + j];
        }
    }
}

/* Solve L*X = B for X (overwrites B); L is diag's strictly-lower part, unit diagonal implied. */
static void fwd(const float* L, float* B, int n)
{
    for (int j = 0; j < n; j++) {
        for (int i = 0; i < n; i++) {
            float s = B[i * n + j];
            for (int k = 0; k < i; k++)
                s -= L[i * n + k] * B[k * n + j];
            B[i * n + j] = s;
        }
    }
}

/* Solve X*U = B for X (overwrites B); U is diag's upper part including diagonal. */
static void bdiv(const float* U, float* B, int n)
{
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            float s = B[i * n + j];
            for (int k = 0; k < j; k++)
                s -= B[i * n + k] * U[k * n + j];
            B[i * n + j] = s / U[j * n + j];
        }
    }
}

/* B -= A*C */
static void bmod(const float* A, const float* C, float* B, int n)
{
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            float s = 0.0f;
            for (int k = 0; k < n; k++)
                s += A[i * n + k] * C[k * n + j];
            B[i * n + j] -= s;
        }
    }
}

/* ---- task wrappers ---- */

typedef struct { int k; } lu0_args_t;
typedef struct { int k, j; } fwd_args_t;
typedef struct { int k, i; } bdiv_args_t;
typedef struct { int i, k, j; } bmod_args_t;

static void* lu0_task_fn(void* data)
{
    lu0_args_t* a = (lu0_args_t*)data;
    lu0(A_blk[a->k * NB + a->k], BLOCK);
    return NULL;
}
static void* fwd_task_fn(void* data)
{
    fwd_args_t* a = (fwd_args_t*)data;
    fwd(A_blk[a->k * NB + a->k], A_blk[a->k * NB + a->j], BLOCK);
    return NULL;
}
static void* bdiv_task_fn(void* data)
{
    bdiv_args_t* a = (bdiv_args_t*)data;
    bdiv(A_blk[a->k * NB + a->k], A_blk[a->i * NB + a->k], BLOCK);
    return NULL;
}
static void* bmod_task_fn(void* data)
{
    bmod_args_t* a = (bmod_args_t*)data;
    bmod(A_blk[a->i * NB + a->k], A_blk[a->k * NB + a->j], A_blk[a->i * NB + a->j], BLOCK);
    return NULL;
}

static struct mir_data_footprint_t one_fp(float* blk, mir_data_access_t access)
{
    return (struct mir_data_footprint_t){
        .base = blk, .type = 1, .start = 0, .end = BLOCK_BYTES - 1, .row_sz = 0,
        .data_access = access, .part_of = blk
    };
}

void bench_init(void)
{
    for (int idx = 0; idx < NB * NB; idx++) {
        A_blk[idx] = (float*)mir_mem_pol_allocate(BLOCK_BYTES);
        int bi = idx / NB, bj = idx % NB;
        for (int r = 0; r < BLOCK; r++) {
            for (int c = 0; c < BLOCK; c++) {
                float v;
                if (bi == bj)
                    v = (r == c) ? 4.0f : ((r == c + 1 || c == r + 1) ? 0.5f : 0.0f);
                else
                    v = 0.01f;
                A_blk[idx][r * BLOCK + c] = v;
            }
        }
    }
    for (int x = 0; x < BLOCK_ELEMS; x++)
        orig00[x] = A_blk[0][x];
}

void bench_run(void)
{
    for (int k = 0; k < NB; k++) {
        lu0_args_t lu_args = { .k = k };
        struct mir_data_footprint_t lu_fp[1] = { one_fp(A_blk[k * NB + k], MIR_DATA_ACCESS_READ) };
        mir_task_create(lu0_task_fn, &lu_args, sizeof(lu_args), 1, lu_fp, "lu0");
        mir_task_wait(); /* fwd/bdiv at this step need lu0(k,k) done */

        for (int j = k + 1; j < NB; j++) {
            fwd_args_t args = { .k = k, .j = j };
            struct mir_data_footprint_t fp[2] = {
                one_fp(A_blk[k * NB + k], MIR_DATA_ACCESS_READ),
                one_fp(A_blk[k * NB + j], MIR_DATA_ACCESS_READ)
            };
            mir_task_create(fwd_task_fn, &args, sizeof(args), 2, fp, "fwd");
        }
        for (int i = k + 1; i < NB; i++) {
            bdiv_args_t args = { .k = k, .i = i };
            struct mir_data_footprint_t fp[2] = {
                one_fp(A_blk[k * NB + k], MIR_DATA_ACCESS_READ),
                one_fp(A_blk[i * NB + k], MIR_DATA_ACCESS_READ)
            };
            mir_task_create(bdiv_task_fn, &args, sizeof(args), 2, fp, "bdiv");
        }
        mir_task_wait(); /* bmod at this step needs this step's fwd/bdiv done */

        for (int i = k + 1; i < NB; i++) {
            for (int j = k + 1; j < NB; j++) {
                bmod_args_t args = { .i = i, .k = k, .j = j };
                struct mir_data_footprint_t fp[3] = {
                    one_fp(A_blk[i * NB + k], MIR_DATA_ACCESS_READ),
                    one_fp(A_blk[k * NB + j], MIR_DATA_ACCESS_READ),
                    one_fp(A_blk[i * NB + j], MIR_DATA_ACCESS_READ)
                };
                mir_task_create(bmod_task_fn, &args, sizeof(args), 3, fp, "bmod");
            }
        }
        mir_task_wait(); /* next step's lu0 needs this step's bmod done */
    }
}

int bench_check(void)
{
    /* Broad sanity net: nothing went non-finite anywhere (would catch most indexing/ordering
     * bugs in a 8-step, 4-kernel dependency chain). */
    for (int idx = 0; idx < NB * NB; idx++)
        for (int x = 0; x < BLOCK_ELEMS; x++)
            if (!isfinite(A_blk[idx][x])) {
                fprintf(stderr, "bench_check: block %d elem %d is non-finite: %f\n",
                        idx, x, A_blk[idx][x]);
                return 0;
            }

    /* Precise check: reconstruct L*U for block (0,0) (unaffected by any bmod -- it's only ever
     * read after step 0) and compare to the saved original. */
    float* lu = A_blk[0];
    for (int i = 0; i < BLOCK; i++) {
        for (int j = 0; j < BLOCK; j++) {
            float s = 0.0f;
            int kmax = i < j ? i : j;
            for (int k = 0; k <= kmax; k++) {
                float l_ik = (k == i) ? 1.0f : lu[i * BLOCK + k];
                float u_kj = lu[k * BLOCK + j];
                s += l_ik * u_kj;
            }
            float expected = orig00[i * BLOCK + j];
            if (fabsf(s - expected) > 1e-2f * (fabsf(expected) + 1.0f)) {
                fprintf(stderr, "bench_check: LU reconstruction mismatch at (%d,%d): "
                        "got %f expected %f\n", i, j, s, expected);
                return 0;
            }
        }
    }
    return 1;
}

void bench_teardown(void)
{
    for (int idx = 0; idx < NB * NB; idx++)
        mir_mem_pol_release(A_blk[idx], BLOCK_BYTES);
}
