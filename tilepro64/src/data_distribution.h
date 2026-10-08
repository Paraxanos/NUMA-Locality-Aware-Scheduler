#ifndef DATA_DISTRIBUTION_H
#define DATA_DISTRIBUTION_H

#include "home_cache.h"
#include <stdint.h>

typedef enum {
    DATA_DIST_FINE,
    DATA_DIST_COARSE
} data_dist_policy_t;

// Fine distribution: cache lines (e.g. 64B) striped round-robin across all 64 tiles
void distribute_fine(data_dist_t* dist, uint64_t total_bytes);

// Coarse distribution: entire buffer homed at designated tile
void distribute_coarse(data_dist_t* dist, uint16_t home_tile, uint64_t total_bytes);

// Multi-buffer coarse distribution: compound footprint from multiple coarse buffers
void distribute_multi_coarse(data_dist_t* dist, const uint16_t* home_tiles, const uint64_t* chunk_bytes, uint32_t num_chunks);

const char* data_dist_policy_name(data_dist_policy_t pol);

#endif // DATA_DISTRIBUTION_H
