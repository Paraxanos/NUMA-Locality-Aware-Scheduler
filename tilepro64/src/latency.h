#ifndef LATENCY_H
#define LATENCY_H

#include "tilepro64.h"
#include <stdint.h>

#define TILEPRO64_LOCAL_LATENCY_CYCLES       10
#define TILEPRO64_REMOTE_BASE_LATENCY_CYCLES 38
#define TILEPRO64_PER_HOP_LATENCY_CYCLES     2

typedef struct {
    uint32_t matrix[TILEPRO64_NUM_TILES][TILEPRO64_NUM_TILES];
    uint32_t local_cost;
    uint32_t remote_base;
    uint32_t per_hop;
} latency_model_t;

extern latency_model_t g_latency_model;

void latency_model_init(uint32_t local_cost, uint32_t remote_base, uint32_t per_hop);
uint32_t latency_get(uint16_t core_tile, uint16_t home_tile);

#endif // LATENCY_H
