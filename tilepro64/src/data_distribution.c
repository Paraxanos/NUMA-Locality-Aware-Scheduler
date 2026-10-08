#include "data_distribution.h"

void distribute_fine(data_dist_t* dist, uint64_t total_bytes) {
    if (!dist) return;
    data_dist_init(dist);

    uint64_t bytes_per_tile = total_bytes / TILEPRO64_NUM_TILES;
    uint64_t remainder = total_bytes % TILEPRO64_NUM_TILES;

    for (uint16_t i = 0; i < TILEPRO64_NUM_TILES; i++) {
        uint64_t b = bytes_per_tile + (i < remainder ? 1 : 0);
        data_dist_add(dist, i, b);
    }
}

void distribute_coarse(data_dist_t* dist, uint16_t home_tile, uint64_t total_bytes) {
    if (!dist) return;
    data_dist_init(dist);
    data_dist_add(dist, home_tile % TILEPRO64_NUM_TILES, total_bytes);
}

void distribute_multi_coarse(data_dist_t* dist, const uint16_t* home_tiles, const uint64_t* chunk_bytes, uint32_t num_chunks) {
    if (!dist || !home_tiles || !chunk_bytes) return;
    data_dist_init(dist);
    for (uint32_t c = 0; c < num_chunks; c++) {
        data_dist_add(dist, home_tiles[c] % TILEPRO64_NUM_TILES, chunk_bytes[c]);
    }
}

const char* data_dist_policy_name(data_dist_policy_t pol) {
    switch (pol) {
        case DATA_DIST_FINE: return "fine";
        case DATA_DIST_COARSE: return "coarse";
        default: return "unknown";
    }
}
