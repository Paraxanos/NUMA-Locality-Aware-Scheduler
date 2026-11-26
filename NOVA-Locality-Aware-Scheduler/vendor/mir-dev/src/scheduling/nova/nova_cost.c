/* NOVA Footprint & Cost Service -- docs/10-nova-architecture.md section4.6. */
#include "nova_cost.h"
#include "mir_runtime.h"

unsigned long nova_cost(const struct mir_mem_node_dist_t* dist, uint16_t node)
{
    return mir_mem_node_dist_get_comm_cost(dist, node);
}

int nova_is_significant(const struct mir_mem_node_dist_t* dist)
{
    size_t low_limit = (runtime->arch->llc_size_KB * 1024) / (runtime->arch->num_cores / runtime->arch->num_nodes);

    struct mir_mem_node_dist_stat_t stat;
    mir_mem_node_dist_get_stat(&stat, dist);

    return stat.sum >= low_limit;
}
