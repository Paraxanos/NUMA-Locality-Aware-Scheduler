#include "latency.h"

latency_model_t g_latency_model;

void latency_model_init(uint32_t local_cost, uint32_t remote_base, uint32_t per_hop) {
    g_latency_model.local_cost = local_cost;
    g_latency_model.remote_base = remote_base;
    g_latency_model.per_hop = per_hop;

    for (uint16_t i = 0; i < TILEPRO64_NUM_TILES; i++) {
        for (uint16_t j = 0; j < TILEPRO64_NUM_TILES; j++) {
            if (i == j) {
                g_latency_model.matrix[i][j] = local_cost;
            } else {
                uint16_t hops = tile_distance(i, j);
                g_latency_model.matrix[i][j] = remote_base + (per_hop * hops);
            }
        }
    }
}

uint32_t latency_get(uint16_t core_tile, uint16_t home_tile) {
    if (core_tile >= TILEPRO64_NUM_TILES || home_tile >= TILEPRO64_NUM_TILES) {
        return 0;
    }
    return g_latency_model.matrix[core_tile][home_tile];
}
