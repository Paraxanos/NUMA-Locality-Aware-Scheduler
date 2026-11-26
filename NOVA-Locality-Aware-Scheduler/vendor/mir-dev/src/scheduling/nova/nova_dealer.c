/* N-A Load-Aware Dealer -- docs/10-nova-architecture.md section4.3, docs/06-novelties-alloc.md N-A. */
#include "nova_dealer.h"
#include "nova_cost.h"
#include "nova_coordinator.h"
#include "mir_runtime.h"
#include "mir_task_queue.h"
#include "mir_utils.h"
#include "mir_defines.h"

int nova_deal(struct mir_worker_t* this_worker, struct mir_task_t* task,
              struct shadow_index_t* shadow_by_node, struct shadow_index_t* shadow_alt_by_node)
{
    MIR_ASSERT(task != NULL);
    MIR_ASSERT(this_worker != NULL);
    int pushed = 1;
    int push_to_alt = 0;
    uint16_t target_node;

    struct mir_mem_node_dist_t* dist = mir_task_get_mem_node_dist(task, MIR_DATA_ACCESS_READ);

    if (dist != NULL && nova_is_significant(dist)) {
        const struct alloc_context_t* ctx = nova_coordinator_snapshot();
        uint16_t num_nodes = runtime->arch->num_nodes;
        unsigned long costs[num_nodes];
        unsigned long min_cost = (unsigned long)-1, max_cost = 0;
        uint64_t min_occ = (uint64_t)-1, max_occ = 0;

        for (uint16_t i = 0; i < num_nodes; i++) {
            costs[i] = nova_cost(dist, i);
            if (costs[i] < min_cost) min_cost = costs[i];
            if (costs[i] > max_cost) max_cost = costs[i];
            uint64_t occ = ctx->occ_snapshot[i];
            if (occ < min_occ) min_occ = occ;
            if (occ > max_occ) max_occ = occ;
        }

        float best_score = 0.0f;
        uint16_t best_node = 0;
        for (uint16_t i = 0; i < num_nodes; i++) {
            float cost_norm = (max_cost > min_cost)
                ? (float)(costs[i] - min_cost) / (float)(max_cost - min_cost) : 0.0f;
            float occ_norm = (max_occ > min_occ)
                ? (float)(ctx->occ_snapshot[i] - min_occ) / (float)(max_occ - min_occ) : 0.0f;
            float score = ctx->alpha * cost_norm + (1.0f - ctx->alpha) * occ_norm;
            if (i == 0 || score < best_score) {
                best_score = score;
                best_node = i;
            }
        }
        target_node = best_node;
        if (runtime->enable_worker_stats == 1)
            task->comm_cost = costs[best_node];
    } else {
        target_node = runtime->arch->node_of(this_worker->cpu_id);
        push_to_alt = 1;
        if (dist != NULL && runtime->enable_worker_stats == 1)
            task->comm_cost = nova_cost(dist, target_node);
    }

    struct mir_task_queue_t* queue = push_to_alt
        ? (struct mir_task_queue_t*)runtime->sched_pol->alt_queues[target_node]
        : (struct mir_task_queue_t*)runtime->sched_pol->queues[target_node];
    MIR_ASSERT(queue != NULL);

    /* Append to the Shadow Index BEFORE the task becomes visible in the real queue (not after --
     * see docs/10-nova-architecture.md section4.4.1: "at the SAME POINT it pushes the task to the
     * real queue"). Appending after mir_task_queue_push() returns is a genuine use-after-free
     * race: the instant push succeeds, any idle worker can steal, execute, and free this task,
     * and this thread would then read/write a freed task's fields here -- found empirically via
     * a native ASan run that caught a SEGV manifesting much later, in unrelated code, after heap
     * corruption from exactly this race. Appending first is always safe even if the push below
     * fails and the task is inlined/executed in this same thread instead: the shadow entry just
     * goes stale, which nova_shadow_peek()'s task->taken check (or a failed try_remove_task scan
     * against a queue the task never entered) already handles gracefully. */
    struct shadow_index_t* shadow = push_to_alt
        ? &shadow_alt_by_node[target_node] : &shadow_by_node[target_node];
    nova_shadow_append(shadow, task);

    if (0 == mir_task_queue_push(queue, task)) {
#ifdef MIR_INLINE_TASK_IF_QUEUE_FULL
        pushed = 0;
        mir_task_execute(task);
        if (runtime->enable_worker_stats == 1)
            this_worker->statistics->num_tasks_inlined++;
#else
        MIR_LOG_ERR("Cannot enque task. Increase queue capacity using MIR_CONF.");
#endif
    } else {
        __sync_fetch_and_add(&g_num_tasks_waiting, 1);
        if (runtime->enable_worker_stats == 1)
            this_worker->statistics->num_tasks_created++;
    }

    nova_coordinator_on_op(target_node);

    return pushed;
}
