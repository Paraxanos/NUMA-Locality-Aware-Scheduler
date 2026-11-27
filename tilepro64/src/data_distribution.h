#ifndef DATA_DISTRIBUTION_H
#define DATA_DISTRIBUTION_H

#include "home_cache.h"
#include <stdint.h>

typedef enum {
    DATA_DIST_FINE,
    DATA_DIST_COARSE,
    DATA_DIST_1D_STRIPED,
    DATA_DIST_2D_BLOCK
} data_dist_policy_t;

// Fine distribution: cache lines (e.g. 64B) striped round-robin across all 64 tiles
void distribute_fine(data_dist_t* dist, uint64_t total_bytes);

// Coarse distribution: entire buffer homed at designated tile
void distribute_coarse(data_dist_t* dist, uint16_t home_tile, uint64_t total_bytes);

// Multi-buffer coarse distribution: compound footprint from multiple coarse buffers
void distribute_multi_coarse(data_dist_t* dist, const uint16_t* home_tiles, const uint64_t* chunk_bytes, uint32_t num_chunks);

// H10a 2D stencil footprints: 1D-striped vs 2D-blocked neighbor-preserving placement
void distribute_2d_stencil_1d_striped(data_dist_t* dist, uint16_t tile_x, uint16_t tile_y,
                                      uint64_t block_bytes, uint64_t halo_bytes);
void distribute_2d_stencil_blocked(data_dist_t* dist, uint16_t tile_x, uint16_t tile_y,
                                   uint64_t block_bytes, uint64_t halo_bytes);

const char* data_dist_policy_name(data_dist_policy_t pol);

#endif // DATA_DISTRIBUTION_H
