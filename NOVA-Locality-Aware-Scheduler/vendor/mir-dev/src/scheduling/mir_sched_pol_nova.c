/* NOVA scheduling policies -- docs/10-nova-architecture.md. Registers three additive ablation
 * variants (section9): nova-a (N-A only), nova-ab (N-A+N-B), nova (N-A+N-B+N-C, the full
 * contribution). N-D and the Heuristic Feedback Log (section4.7/4.8) are explicitly stretch in
 * both docs 06 and 10 and are not implemented here.
 *
 * Never edits mir_sched_pol_numa.c (principle 2, section1.3) -- create_numa/destroy_numa/pop_numa
 * are reused directly (queue setup is identical; pop_numa IS nova-a's pop verbatim, since N-A only
 * changes dealing, never stealing).
 */
#ifdef MIR_MEM_POL_ENABLE
#ifdef MIR_NOVA_ENABLE

#include "mir_runtime.h"
#include "scheduling/mir_sched_pol.h"
#include "mir_worker.h"
#include "mir_task.h"
#include "mir_task_queue.h"
#include "mir_recorder.h"
#include "mir_memory.h"
#include "mir_utils.h"
#include "mir_defines.h"
#include "mir_mem_pol.h"

#include "nova/nova_coordinator.h"
#include "nova/nova_cost.h"
#include "nova/nova_shadow_index.h"
#include "nova/nova_dealer.h"
#include "nova/nova_stealer.h"
#include "nova/nova_radius.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Reused verbatim from mir_sched_pol_numa.c -- not redeclared in any header, so forward-declared
 * here (same pattern mir_sched_pol.h already uses for the policy_* struct externs). */
extern void create_numa(void);
extern void destroy_numa(void);
extern int pop_numa(struct mir_task_t** task);

static struct shadow_index_t* g_shadow_by_node = NULL;
static struct shadow_index_t* g_shadow_alt_by_node = NULL;
static uint16_t g_num_nodes = 0;

static void create_nova_common(void)
{
    create_numa(); /* identical queue/alt_queue setup -- principle 2, no duplication */

    g_num_nodes = runtime->arch->num_nodes;
    g_shadow_by_node = mir_malloc_int(sizeof(struct shadow_index_t) * g_num_nodes);
    MIR_CHECK_MEM(g_shadow_by_node != NULL);
    g_shadow_alt_by_node = mir_malloc_int(sizeof(struct shadow_index_t) * g_num_nodes);
    MIR_CHECK_MEM(g_shadow_alt_by_node != NULL);
    for (int i = 0; i < g_num_nodes; i++) {
        nova_shadow_index_init(&g_shadow_by_node[i]);
        nova_shadow_index_init(&g_shadow_alt_by_node[i]);
    }

    nova_coordinator_create();
}

static void destroy_nova_common(void)
{
    nova_coordinator_destroy();
    mir_free_int(g_shadow_by_node, sizeof(struct shadow_index_t) * g_num_nodes);
    mir_free_int(g_shadow_alt_by_node, sizeof(struct shadow_index_t) * g_num_nodes);
    g_shadow_by_node = NULL;
    g_shadow_alt_by_node = NULL;
    destroy_numa();
}

static int push_nova(struct mir_worker_t* this_worker, struct mir_task_t* task)
{
    return nova_deal(this_worker, task, g_shadow_by_node, g_shadow_alt_by_node);
}

/* Shared by nova-ab and nova: record a steal's outcome in the worker statistics exactly like
 * pop_numa does, after a steal (via Shadow Index or plain fallback) actually succeeds. */
static void record_steal_stats(struct mir_worker_t* worker, struct mir_task_t* task, uint16_t node)
{
    if (runtime->enable_worker_stats != 1)
        return;
    if (task->comm_cost == (unsigned long)-1) {
        struct mir_mem_node_dist_t* dist = mir_task_get_mem_node_dist(task, MIR_DATA_ACCESS_READ);
        if (dist)
            task->comm_cost = mir_mem_node_dist_get_comm_cost(dist, node);
    }
    if (task->comm_cost != (unsigned long)-1)
        mir_worker_statistics_update_comm_cost(worker->statistics, task->comm_cost);
    worker->statistics->num_tasks_stolen++;
}

/* nova-ab: N-A dealing (via push_nova) + N-B task-level stealing. Steal structure (which queues
 * are visited, in which order) is unchanged from pop_numa -- the unordered alt_queue sweep (D3,
 * not yet fixed at this ablation level) then the static-vicinity distance-ordered ring search
 * (H1, not yet fixed at this ablation level) -- only *which task* is taken from a chosen queue
 * changes, from blind FIFO to nova_steal_best()'s argmin-cost pick. */
static int pop_nova_ab(struct mir_task_t** task)
{
    struct mir_sched_pol_t* sp = runtime->sched_pol;
    MIR_ASSERT(NULL != sp);
    uint32_t num_queues = sp->num_queues;
    struct mir_worker_t* worker = mir_worker_get_context();
    MIR_ASSERT(NULL != worker);
    uint16_t node = runtime->arch->node_of(worker->cpu_id);

    /* Local pop (own alt_queue, own queue) is unchanged -- N-B only affects stealing. */
    struct mir_task_queue_t* own_alt = (struct mir_task_queue_t*)sp->alt_queues[node];
    if (mir_task_queue_size((struct mir_task_queue_t*)own_alt) > 0) {
        *task = mir_task_queue_pop(own_alt);
        if (*task) {
            if (runtime->enable_worker_stats == 1) {
                if ((*task)->comm_cost != (unsigned long)-1)
                    mir_worker_statistics_update_comm_cost(worker->statistics, (*task)->comm_cost);
                worker->statistics->num_tasks_owned++;
            }
            __sync_fetch_and_sub(&g_num_tasks_waiting, 1);
            return 1;
        }
    }

    struct mir_task_queue_t* own_q = (struct mir_task_queue_t*)sp->queues[node];
    if (mir_task_queue_size(own_q) > 0) {
        *task = mir_task_queue_pop(own_q);
        if (*task) {
            if (runtime->enable_worker_stats == 1) {
                if ((*task)->comm_cost != (unsigned long)-1)
                    mir_worker_statistics_update_comm_cost(worker->statistics, (*task)->comm_cost);
                worker->statistics->num_tasks_owned++;
            }
            __sync_fetch_and_sub(&g_num_tasks_waiting, 1);
            return 1;
        }
    }

    /* Unordered alt_queue sweep (D3, same ordering as pop_numa) -- task choice is N-B's. */
    uint16_t ctr = (node + 1 == num_queues) ? 0 : node + 1;
    while (ctr != node) {
        struct mir_task_queue_t* aq = (struct mir_task_queue_t*)sp->alt_queues[ctr];
        if (mir_task_queue_size(aq) > 0) {
            *task = nova_steal_best(aq, &g_shadow_alt_by_node[ctr], node);
            if (*task == NULL)
                *task = mir_task_queue_pop(aq); /* degradation: Shadow Index empty/stale */
            if (*task) {
                record_steal_stats(worker, *task, node);
                __sync_fetch_and_sub(&g_num_tasks_waiting, 1);
                return 1;
            }
        }
        ctr = (ctr + 1 == num_queues) ? 0 : ctr + 1;
    }

    /* Distance-ordered ring search (static vicinity, same low_limit gate as pop_numa -- H1 is
     * only fixed at the full `nova` ablation level). */
    uint16_t neighbors[runtime->arch->num_nodes];
    for (int d = 1; d <= runtime->arch->diameter; d++) {
        uint16_t count = runtime->arch->vicinity_of(neighbors, node, d);
        for (int i = 0; i < count; i++) {
            struct mir_task_queue_t* q = (struct mir_task_queue_t*)sp->queues[neighbors[i]];
            size_t low_limit = (runtime->arch->num_cores / runtime->arch->num_nodes) * d;
            if (mir_task_queue_size(q) > low_limit) {
                *task = nova_steal_best(q, &g_shadow_by_node[neighbors[i]], node);
                if (*task == NULL)
                    *task = mir_task_queue_pop(q);
                if (*task) {
                    if (runtime->enable_worker_stats == 1)
                        worker->statistics->num_comm_tasks_stolen_by_diameter[d - 1]++;
                    record_steal_stats(worker, *task, node);
                    __sync_fetch_and_sub(&g_num_tasks_waiting, 1);
                    return 1;
                }
            }
        }
    }

    return 0;
}

/* nova (full): N-A + N-B + N-C. D3 is folded into the single radius-bounded, distance-ordered
 * walk (section4.5's "Folding in D3") -- no separate unordered alt_queue sweep. Radius replaces
 * the static vicinity threshold (H1). One nova_report_steal() call per overall attempt. */
static int pop_nova(struct mir_task_t** task)
{
    struct mir_sched_pol_t* sp = runtime->sched_pol;
    MIR_ASSERT(NULL != sp);
    struct mir_worker_t* worker = mir_worker_get_context();
    MIR_ASSERT(NULL != worker);
    uint16_t node = runtime->arch->node_of(worker->cpu_id);

    struct mir_task_queue_t* own_alt = (struct mir_task_queue_t*)sp->alt_queues[node];
    if (mir_task_queue_size(own_alt) > 0) {
        *task = mir_task_queue_pop(own_alt);
        if (*task) {
            if (runtime->enable_worker_stats == 1) {
                if ((*task)->comm_cost != (unsigned long)-1)
                    mir_worker_statistics_update_comm_cost(worker->statistics, (*task)->comm_cost);
                worker->statistics->num_tasks_owned++;
            }
            __sync_fetch_and_sub(&g_num_tasks_waiting, 1);
            return 1;
        }
    }

    struct mir_task_queue_t* own_q = (struct mir_task_queue_t*)sp->queues[node];
    if (mir_task_queue_size(own_q) > 0) {
        *task = mir_task_queue_pop(own_q);
        if (*task) {
            if (runtime->enable_worker_stats == 1) {
                if ((*task)->comm_cost != (unsigned long)-1)
                    mir_worker_statistics_update_comm_cost(worker->statistics, (*task)->comm_cost);
                worker->statistics->num_tasks_owned++;
            }
            __sync_fetch_and_sub(&g_num_tasks_waiting, 1);
            return 1;
        }
    }

    uint32_t radius = nova_radius(worker);
    uint16_t neighbors[runtime->arch->num_nodes];
    for (uint32_t d = 1; d <= radius; d++) {
        uint16_t count = runtime->arch->vicinity_of(neighbors, node, d);
        for (int i = 0; i < count; i++) {
            uint16_t cand = neighbors[i];

            /* Alt-queue entries at this hop, folded into the same ordered walk -- section4.5. */
            struct mir_task_queue_t* aq = (struct mir_task_queue_t*)sp->alt_queues[cand];
            if (mir_task_queue_size(aq) > 0) {
                *task = nova_steal_best(aq, &g_shadow_alt_by_node[cand], node);
                if (*task == NULL)
                    *task = mir_task_queue_pop(aq);
                if (*task) {
                    record_steal_stats(worker, *task, node);
                    __sync_fetch_and_sub(&g_num_tasks_waiting, 1);
                    nova_report_steal(node, 1);
                    return 1;
                }
            }

            struct mir_task_queue_t* q = (struct mir_task_queue_t*)sp->queues[cand];
            if (mir_task_queue_size(q) > 0) {
                *task = nova_steal_best(q, &g_shadow_by_node[cand], node);
                if (*task == NULL)
                    *task = mir_task_queue_pop(q);
                if (*task) {
                    if (runtime->enable_worker_stats == 1)
                        worker->statistics->num_comm_tasks_stolen_by_diameter[d - 1]++;
                    record_steal_stats(worker, *task, node);
                    __sync_fetch_and_sub(&g_num_tasks_waiting, 1);
                    nova_report_steal(node, 1);
                    return 1;
                }
            }
        }
    }

    nova_report_steal(node, 0);
    return 0;
}

struct mir_sched_pol_t policy_nova_a = {
    .num_queues = MIR_WORKER_MAX_COUNT,
    .queue_capacity = MIR_QUEUE_MAX_CAPACITY,
    .queues = NULL,
    .alt_queues = NULL,
    .name = "nova-a",
    .create = create_nova_common,
    .destroy = destroy_nova_common,
    .push = push_nova,
    .pop = pop_numa /* N-A only changes dealing; stealing is the base paper's, unmodified */
};

struct mir_sched_pol_t policy_nova_ab = {
    .num_queues = MIR_WORKER_MAX_COUNT,
    .queue_capacity = MIR_QUEUE_MAX_CAPACITY,
    .queues = NULL,
    .alt_queues = NULL,
    .name = "nova-ab",
    .create = create_nova_common,
    .destroy = destroy_nova_common,
    .push = push_nova,
    .pop = pop_nova_ab
};

struct mir_sched_pol_t policy_nova = {
    .num_queues = MIR_WORKER_MAX_COUNT,
    .queue_capacity = MIR_QUEUE_MAX_CAPACITY,
    .queues = NULL,
    .alt_queues = NULL,
    .name = "nova",
    .create = create_nova_common,
    .destroy = destroy_nova_common,
    .push = push_nova,
    .pop = pop_nova
};

#endif /* MIR_NOVA_ENABLE */
#endif /* MIR_MEM_POL_ENABLE */
