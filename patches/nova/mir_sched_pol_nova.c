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
#include "arch/mir_arch_sim.h"
#include "nova/nova_coordinator.h"
#include "nova/nova_shadow_index.h"
#include "nova/nova_dealer.h"
#include "nova/nova_stealer.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static struct alloc_coordinator_t g_coordinator;
static struct shadow_index_t g_shadow_indices[NOVA_MAX_NODES];

static void nova_init(void) {
    mir_arch_sim_init(MIR_SIM_DEFAULT_NODES);
    nova_coordinator_init(&g_coordinator);
    for (int i = 0; i < NOVA_MAX_NODES; i++) {
        nova_shadow_init(&g_shadow_indices[i]);
    }
}

static void nova_create(void) {
    // Nova uses per-node queues (same as NUMA)
    struct mir_sched_pol_t* sp = runtime->sched_pol;
    sp->num_queues = runtime->arch->num_nodes;
    sp->queues = mir_malloc_int(sp->num_queues * sizeof(struct mir_task_queue_t*));
    for (int i = 0; i < sp->num_queues; i++) {
        sp->queues[i] = mir_task_queue_create(sp->queue_capacity);
    }
    sp->alt_queues = NULL;
    nova_init();
}

static void nova_destroy(void) {
    struct mir_sched_pol_t* sp = runtime->sched_pol;
    for (int i = 0; i < sp->num_queues; i++) {
        if (sp->queues[i]) {
            mir_task_queue_destroy((struct mir_task_queue_t*)sp->queues[i]);
            sp->queues[i] = NULL;
        }
    }
    mir_free_int(sp->queues, sizeof(struct mir_task_queue_t*) * sp->num_queues);
    sp->queues = NULL;
}

int nova_push(struct mir_worker_t* worker, struct mir_task_t* task) {
    MIR_ASSERT(NULL != task);
    uint64_t llc_bytes = 20 * 1024 * 1024; // 20 MB default LLC
    uint16_t target_node = nova_deal(&g_coordinator, g_shadow_indices, task, worker->id, llc_bytes);

    // Push to the target node's queue
    struct mir_task_queue_t* queue = (struct mir_task_queue_t*)runtime->sched_pol->queues[target_node];
    MIR_ASSERT(NULL != queue);

    int pushed = mir_task_queue_push(queue, task);
    if (pushed) {
        __sync_fetch_and_add(&g_num_tasks_waiting, 1);
        if (runtime->enable_worker_stats == 1)
            worker->statistics->num_tasks_created++;
    }
#ifdef MIR_INLINE_TASK_IF_QUEUE_FULL
    if (!pushed) {
        mir_task_execute(task);
        if (runtime->enable_worker_stats == 1)
            worker->statistics->num_tasks_inlined++;
        pushed = 1;
    }
#endif
    return pushed;
}

int nova_pop(struct mir_task_t** task) {
    MIR_ASSERT(NULL != task);
    struct mir_sched_pol_t* sp = runtime->sched_pol;
    MIR_ASSERT(NULL != sp);

    struct mir_worker_t* worker = mir_worker_get_context();
    MIR_ASSERT(NULL != worker);
    uint16_t node = runtime->arch->node_of(worker->cpu_id);

    // Pop from own node's queue first
    struct mir_task_queue_t* queue = (struct mir_task_queue_t*)sp->queues[node];
    MIR_ASSERT(NULL != queue);
    *task = mir_task_queue_pop(queue);
    if (*task) {
        if (runtime->enable_worker_stats == 1) {
            worker->statistics->num_tasks_owned++;
        }
        __sync_fetch_and_sub(&g_num_tasks_waiting, 1);
        return 1;
    }

    // Try to steal from other nodes using Nova's steal logic
    struct mir_task_t* stolen = nova_steal(&g_coordinator, g_shadow_indices, worker->id);
    if (stolen) {
        *task = stolen;
        if (runtime->enable_worker_stats == 1) {
            worker->statistics->num_tasks_stolen++;
        }
        __sync_fetch_and_sub(&g_num_tasks_waiting, 1);
        return 1;
    }

    return 0;
}

struct mir_sched_pol_t policy_nova = {
    .num_queues = MIR_WORKER_MAX_COUNT,
    .queue_capacity = MIR_QUEUE_MAX_CAPACITY,
    .queues = NULL,
    .alt_queues = NULL,
    .name = "nova",
    .create = nova_create,
    .destroy = nova_destroy,
    .push = nova_push,
    .pop = nova_pop
};