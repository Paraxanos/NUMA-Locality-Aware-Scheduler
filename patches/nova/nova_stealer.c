#include "nova_stealer.h"
#include "nova_cost.h"
#include "nova_coordinator.h"
#include "nova_shadow_index.h"
#include "../../arch/mir_arch_sim.h"
#include "../../mir_queue.h"
#include "../../mir_task.h"
#include <stdlib.h>

struct mir_task_t* nova_steal(struct alloc_coordinator_t* coord, struct shadow_index_t* shadow_indices,
                              uint32_t worker_id) {
    uint16_t thief_node = mir_arch_sim_node_of(worker_id);
    nova_coordinator_step_op(coord, thief_node);

    struct alloc_context_t* snap = nova_coordinator_get_snapshot(coord);
    uint32_t radius = snap->radius[worker_id];
    uint16_t num_nodes = mir_arch_sim_num_nodes();

    // Clamp radius to diameter
    uint16_t max_diameter = mir_arch_sim_diameter();
    if (radius > max_diameter) radius = max_diameter;

    for (uint32_t d = 1; d <= radius; d++) {
        for (uint16_t target_node = 0; target_node < num_nodes; target_node++) {
            if (target_node == thief_node) continue;
            uint32_t hops = abs(target_node - thief_node);
            if (hops > (num_nodes / 2)) hops = num_nodes - hops;
            if (hops != d) continue;

            struct shadow_entry_t candidates[NOVA_SHADOW_K];
            uint32_t num_cands = nova_shadow_peek(&shadow_indices[target_node], candidates, NOVA_SHADOW_K);

            if (num_cands > 0) {
                uint64_t best_cost = UINT64_MAX;
                struct mir_task_t* best_task = NULL;

                for (uint32_t c = 0; c < num_cands; c++) {
                    if (!candidates[c].task || candidates[c].task->taken) continue;
                    uint64_t c_cost = nova_comm_cost(candidates[c].dist_read, thief_node);
                    if (c_cost < best_cost) {
                        best_cost = c_cost;
                        best_task = candidates[c].task;
                    }
                }

                if (best_task != NULL) {
                    // Mark taken to prevent double-steal
                    best_task->taken = 1;
                    nova_coordinator_report_steal(coord, thief_node, true);
                    return best_task;
                }
            } else {
                // Fallback: pop from target node's queue
                // Use mir_queue_pop on the per-node queue (indexed by node_id)
                struct mir_queue_t* queue = mir_queue_get_node_queue(target_node);
                if (queue) {
                    struct mir_task_t* fallback_task = NULL;
                    mir_queue_pop(queue, (void**)&fallback_task);
                    if (fallback_task != NULL) {
                        nova_coordinator_report_steal(coord, thief_node, true);
                        return fallback_task;
                    }
                }
            }
        }
    }

    nova_coordinator_report_steal(coord, thief_node, false);
    return NULL;
}