#ifndef MIR_ARCH_SIM_H
#define MIR_ARCH_SIM_H

#include <stdint.h>
#include <stdbool.h>

#define MIR_SIM_DEFAULT_NODES 8
#define MIR_SIM_LOCAL_COST 40
#define MIR_SIM_REMOTE_MIN 240
#define MIR_SIM_REMOTE_MAX 340

void mir_arch_sim_init(uint16_t num_nodes);
uint16_t mir_arch_sim_num_nodes(void);
uint16_t mir_arch_sim_node_of(uint32_t worker_id);
uint32_t mir_arch_sim_comm_cost_of(uint16_t src_node, uint16_t dst_node);
uint16_t mir_arch_sim_diameter(void);

#endif // MIR_ARCH_SIM_H