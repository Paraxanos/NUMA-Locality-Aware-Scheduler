#include "nova_dealer.h"
#include "nova_cost.h"
#include "nova_coordinator.h"
#include "nova_shadow_index.h"
#include "../../arch/mir_arch_sim.h"
#include <float.h>

uint16_t nova_deal(struct alloc_coordinator_t* coord, struct shadow_index_t* shadow_indices,
                   struct mir_task_t* task, uint32_t worker_id, uint64_t llc_bytes) {
    uint16_t local_node = mir_arch_sim_node_of(worker_id);
    nova_coordinator_step_op(coord, local_node);

    if (!nova_is_significant(task, llc_bytes)) {
        nova_shadow_append(&shadow_indices[local_node], task);
        return local_node;
    }

    struct alloc_context_t* snap = nova_coordinator_get_snapshot(coord);
    uint16_t num_nodes = mir_arch_sim_num_nodes();

    float alpha = snap->alpha;
    uint64_t cur_ops = atomic_load_explicit(&coord->op_counter[local_node], memory_order_relaxed);
    if ((cur_ops > snap->epoch) && (cur_ops - snap->epoch > NOVA_STALE_MAX)) {
        alpha = 0.5f;
    }

    uint64_t cost[NOVA_MAX_NODES];
    uint64_t max_cost = 1, min_cost = UINT64_MAX;
    uint64_t max_occ = 1, min_occ = UINT64_MAX;

    void* dist = task->dist_by_access_type[MIR_DATA_ACCESS_READ];
    for (uint16_t n = 0; n < num_nodes; n++) {
        cost[n] = nova_comm_cost(dist, n);
        if (cost[n] > max_cost) max_cost = cost[n];
        if (cost[n] < min_cost) min_cost = cost[n];

        uint64_t occ = snap->occ_snapshot[n];
        if (occ > max_occ) max_occ = occ;
        if (occ < min_occ) min_occ = occ;
    }

    float best_score = FLT_MAX;
    uint16_t best_node = local_node;

    for (uint16_t n = 0; n < num_nodes; n++) {
        float cost_norm = (max_cost == min_cost) ? 0.0f : (float)(cost[n] - min_cost) / (float)(max_cost - min_cost);
        float occ_norm  = (max_occ == min_occ)   ? 0.0f : (float)(snap->occ_snapshot[n] - min_occ) / (float)(max_occ - min_occ);

        float score = (alpha * cost_norm) + ((1.0f - alpha) * occ_norm);
        if (score < best_score) {
            best_score = score;
            best_node = n;
        }
    }

    nova_shadow_append(&shadow_indices[best_node], task);
    return best_node;
}