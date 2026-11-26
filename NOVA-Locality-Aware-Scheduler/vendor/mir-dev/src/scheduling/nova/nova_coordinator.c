/* NOVA Coordinator -- the control plane. See docs/10-nova-architecture.md section4.2/section5.1/section7.
 *
 * Simplification from the doc, declared here rather than left implicit: the doc specifies a
 * *per-node* op_counter/refreshing/election, allowing independent per-node refresh cadences. This
 * implementation uses a single *global* op_counter and a single global "refreshing" CAS guard
 * instead -- one election for the whole context, not eight. This is strictly simpler to get
 * correct and still satisfies every safety property section7 actually requires (lock-free hot
 * path, O(1) per-operation cost outside the elected refresh, CAS-guarded single-writer election,
 * atomic double-buffer swap); it does not change section8's O(nodes)-amortised-over-64-ops cost
 * claim, since the refresh itself is still O(nodes) work no matter which counter triggered it.
 * Similarly, steal_fail_ewma/steal_ok_ewma are tracked as genuinely separate decayed series (not
 * algebraic complements of each other), matching section5.1's struct literally, and alpha is
 * derived from the *global* aggregate steal rate across all nodes (doc 06 N-A: "tuned online from
 * an EWMA of the global steal rate").
 */
#include "nova_coordinator.h"
#include "mir_runtime.h"
#include "mir_queue.h"
#include "mir_utils.h"
#include "mir_memory.h"

#include <string.h>

#define NOVA_EWMA_DECAY 0.7f
#define NOVA_FAIL_HIGH  0.5f
#define NOVA_OK_HIGH    0.3f
#define NOVA_ALPHA_MIN  0.1f
#define NOVA_ALPHA_MAX  0.9f

static struct alloc_context_t g_buf[2];
static struct alloc_context_t* volatile g_current = NULL;
static volatile uint64_t g_op_counter = 0;
static volatile uint32_t g_refreshing = 0;
static uint16_t g_num_nodes = 0;

/* Per-node persistent state, touched only by the (CAS-elected, single-writer-at-a-time) refresher,
 * so it needs no atomics of its own despite living outside the double buffer. */
static uint32_t* g_radius_by_node = NULL;
static volatile uint64_t* g_pending_fail = NULL;
static volatile uint64_t* g_pending_ok = NULL;

static void alloc_ctx_alloc_arrays(struct alloc_context_t* ctx, uint16_t num_nodes)
{
    ctx->occ_snapshot = mir_malloc_int(sizeof(uint64_t) * num_nodes);
    MIR_CHECK_MEM(ctx->occ_snapshot != NULL);
    ctx->steal_fail_ewma = mir_malloc_int(sizeof(float) * num_nodes);
    MIR_CHECK_MEM(ctx->steal_fail_ewma != NULL);
    ctx->steal_ok_ewma = mir_malloc_int(sizeof(float) * num_nodes);
    MIR_CHECK_MEM(ctx->steal_ok_ewma != NULL);
    memset(ctx->occ_snapshot, 0, sizeof(uint64_t) * num_nodes);
    memset(ctx->steal_fail_ewma, 0, sizeof(float) * num_nodes);
    memset(ctx->steal_ok_ewma, 0, sizeof(float) * num_nodes);
    memset(ctx->radius, 0, sizeof(ctx->radius));
    for (int i = 0; i < MIR_WORKER_MAX_COUNT; i++)
        ctx->radius[i] = NOVA_RADIUS_MIN;
    ctx->alpha = 0.5f;
    ctx->epoch = 0;
}

void nova_coordinator_create(void)
{
    g_num_nodes = runtime->arch->num_nodes;

    alloc_ctx_alloc_arrays(&g_buf[0], g_num_nodes);
    alloc_ctx_alloc_arrays(&g_buf[1], g_num_nodes);
    g_current = &g_buf[0];
    g_op_counter = 0;
    g_refreshing = 0;

    g_radius_by_node = mir_malloc_int(sizeof(uint32_t) * g_num_nodes);
    MIR_CHECK_MEM(g_radius_by_node != NULL);
    g_pending_fail = mir_malloc_int(sizeof(uint64_t) * g_num_nodes);
    MIR_CHECK_MEM(g_pending_fail != NULL);
    g_pending_ok = mir_malloc_int(sizeof(uint64_t) * g_num_nodes);
    MIR_CHECK_MEM(g_pending_ok != NULL);
    for (int i = 0; i < g_num_nodes; i++) {
        g_radius_by_node[i] = NOVA_RADIUS_MIN;
        g_pending_fail[i] = 0;
        g_pending_ok[i] = 0;
    }
}

void nova_coordinator_destroy(void)
{
    mir_free_int(g_buf[0].occ_snapshot, sizeof(uint64_t) * g_num_nodes);
    mir_free_int(g_buf[0].steal_fail_ewma, sizeof(float) * g_num_nodes);
    mir_free_int(g_buf[0].steal_ok_ewma, sizeof(float) * g_num_nodes);
    mir_free_int(g_buf[1].occ_snapshot, sizeof(uint64_t) * g_num_nodes);
    mir_free_int(g_buf[1].steal_fail_ewma, sizeof(float) * g_num_nodes);
    mir_free_int(g_buf[1].steal_ok_ewma, sizeof(float) * g_num_nodes);
    mir_free_int(g_radius_by_node, sizeof(uint32_t) * g_num_nodes);
    mir_free_int((void*)g_pending_fail, sizeof(uint64_t) * g_num_nodes);
    mir_free_int((void*)g_pending_ok, sizeof(uint64_t) * g_num_nodes);
    g_current = NULL;
    g_radius_by_node = NULL;
    g_pending_fail = NULL;
    g_pending_ok = NULL;
}

const struct alloc_context_t* nova_coordinator_snapshot(void)
{
    return g_current;
}

int nova_coordinator_is_stale(void)
{
    return (g_op_counter - g_current->epoch) > NOVA_STALE_MAX;
}

void nova_coordinator_report_steal(uint16_t node, int ok)
{
    if (ok)
        __sync_fetch_and_add(&g_pending_ok[node], 1);
    else
        __sync_fetch_and_add(&g_pending_fail[node], 1);
}

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void nova_coordinator_refresh(uint64_t at_op)
{
    struct alloc_context_t* inactive = (g_current == &g_buf[0]) ? &g_buf[1] : &g_buf[0];
    const struct alloc_context_t* active = g_current;

    uint64_t global_fail = 0, global_ok = 0;

    for (int i = 0; i < g_num_nodes; i++) {
        /* O(1) read of this node's queue sizes -- same cost pop_numa already pays per candidate. */
        struct mir_queue_t* q = (struct mir_queue_t*)runtime->sched_pol->queues[i];
        struct mir_queue_t* aq = (struct mir_queue_t*)runtime->sched_pol->alt_queues[i];
        inactive->occ_snapshot[i] = (uint64_t)mir_queue_size(q) + (uint64_t)mir_queue_size(aq);

        uint64_t pending_fail = g_pending_fail[i];
        uint64_t pending_ok = g_pending_ok[i];
        g_pending_fail[i] = 0;
        g_pending_ok[i] = 0;
        global_fail += pending_fail;
        global_ok += pending_ok;

        uint64_t total = pending_fail + pending_ok;
        float rate_fail = total > 0 ? (float)pending_fail / (float)total : 0.0f;
        float rate_ok = total > 0 ? (float)pending_ok / (float)total : 0.0f;

        float new_fail_ewma = NOVA_EWMA_DECAY * active->steal_fail_ewma[i]
                             + (1.0f - NOVA_EWMA_DECAY) * rate_fail;
        float new_ok_ewma = NOVA_EWMA_DECAY * active->steal_ok_ewma[i]
                           + (1.0f - NOVA_EWMA_DECAY) * rate_ok;
        inactive->steal_fail_ewma[i] = new_fail_ewma;
        inactive->steal_ok_ewma[i] = new_ok_ewma;

        /* N-C radius update -- doc 10 section4.5. Thresholds set so both bands can't fire together
         * (hysteresis gap between NOVA_OK_HIGH and NOVA_FAIL_HIGH), rate-capped at one step/epoch. */
        if (new_fail_ewma > NOVA_FAIL_HIGH) {
            if (g_radius_by_node[i] < runtime->arch->diameter)
                g_radius_by_node[i]++;
        } else if (new_ok_ewma > NOVA_OK_HIGH) {
            if (g_radius_by_node[i] > NOVA_RADIUS_MIN)
                g_radius_by_node[i]--;
        }
    }

    uint64_t global_total = global_fail + global_ok;
    float global_rate = global_total > 0 ? (float)global_fail / (float)global_total : 0.0f;
    inactive->alpha = clampf(1.0f - global_rate, NOVA_ALPHA_MIN, NOVA_ALPHA_MAX);

    for (int w = 0; w < runtime->num_workers; w++) {
        uint16_t node = runtime->arch->node_of(runtime->workers[w].cpu_id);
        inactive->radius[w] = g_radius_by_node[node];
    }

    inactive->epoch = at_op;
    g_current = inactive; /* single aligned pointer store; readers take one load, no CAS needed */
}

void nova_coordinator_on_op(uint16_t node)
{
    (void)node; /* kept in the API for fidelity to doc 10's per-node signature; election is global
                   here -- see the file-level comment on the simplification. */
    uint64_t old = __sync_fetch_and_add(&g_op_counter, 1);
    uint64_t at = old + 1;
    if (at % NOVA_EPOCH_OPS == 0) {
        if (__sync_bool_compare_and_swap(&g_refreshing, 0, 1)) {
            nova_coordinator_refresh(at);
            g_refreshing = 0;
        }
        /* else: another worker already won this epoch's election; proceed with the stale-but-
         * valid current snapshot, exactly as section7 specifies. */
    }
}
