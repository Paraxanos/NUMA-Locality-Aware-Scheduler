#include "nova_cost.h"
#include "../../arch/mir_arch_sim.h"
#include "../../mir_mem_pol.h"

// Helper: get footprint bytes from task
static uint64_t mir_task_get_read_footprint_bytes(struct mir_task_t* task) {
    if (!task || !task->dist_by_access_type || task->num_data_footprints == 0) return 0;
    struct mir_mem_node_dist_t* dist = (struct mir_mem_node_dist_t*)task->dist_by_access_type[MIR_DATA_ACCESS_READ];
    if (!dist || !dist->buf) return 0;
    uint64_t sum = 0;
    for (uint16_t n = 0; n < mir_arch_sim_num_nodes(); n++) sum += dist->buf[n];
    return sum;
}

bool nova_is_significant(struct mir_task_t* task, uint64_t llc_per_core_bytes) {
    if (!task) return false;
    uint64_t footprint_sum = mir_task_get_read_footprint_bytes(task);
    return (footprint_sum > llc_per_core_bytes);
}

uint64_t nova_comm_cost(void* dist, uint16_t target_node) {
    if (!dist) return 0;
    uint64_t total_cost = 0;
    uint16_t num_nodes = mir_arch_sim_num_nodes();
    for (uint16_t n = 0; n < num_nodes; n++) {
        uint64_t bytes_on_n = mir_dist_get_bytes(dist, n);
        total_cost += bytes_on_n * mir_arch_sim_comm_cost_of(n, target_node);
    }
    return total_cost;
}