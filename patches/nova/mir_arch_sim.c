#include "arch/mir_arch_sim.h"
#include <stdlib.h>
#include <math.h>

static uint16_t g_sim_nodes = MIR_SIM_DEFAULT_NODES;
static uint32_t g_cost_matrix[64][64];

void mir_arch_sim_init(uint16_t num_nodes) {
    g_sim_nodes = (num_nodes > 0 && num_nodes <= 64) ? num_nodes : MIR_SIM_DEFAULT_NODES;
    for (uint16_t i = 0; i < g_sim_nodes; i++) {
        for (uint16_t j = 0; j < g_sim_nodes; j++) {
            if (i == j) {
                g_cost_matrix[i][j] = MIR_SIM_LOCAL_COST;
            } else {
                uint16_t hop_distance = (abs(i - j) > (g_sim_nodes / 2))
                                      ? (g_sim_nodes - abs(i - j))
                                      : abs(i - j);
                g_cost_matrix[i][j] = MIR_SIM_REMOTE_MIN + (hop_distance * 25);
            }
        }
    }
}

uint16_t mir_arch_sim_num_nodes(void) { return g_sim_nodes; }
uint16_t mir_arch_sim_node_of(uint32_t worker_id) { return (worker_id / 3) % g_sim_nodes; }
uint32_t mir_arch_sim_comm_cost_of(uint16_t src, uint16_t dst) { return g_cost_matrix[src][dst]; }
uint16_t mir_arch_sim_diameter(void) { return g_sim_nodes / 2; }