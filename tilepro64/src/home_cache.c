#include "home_cache.h"
#include <string.h>

void data_dist_init(data_dist_t* dist) {
    if (!dist) return;
    memset(dist->bytes_per_tile, 0, sizeof(dist->bytes_per_tile));
    dist->total_bytes = 0;
}

void data_dist_add(data_dist_t* dist, uint16_t tile_id, uint64_t bytes) {
    if (!dist || tile_id >= TILEPRO64_NUM_TILES) return;
    dist->bytes_per_tile[tile_id] += bytes;
    dist->total_bytes += bytes;
}

uint64_t data_dist_total(const data_dist_t* dist) {
    return dist ? dist->total_bytes : 0;
}

uint16_t data_dist_primary_tile(const data_dist_t* dist) {
    if (!dist) return 0;
    uint64_t max_bytes = 0;
    uint16_t primary = 0;
    for (uint16_t i = 0; i < TILEPRO64_NUM_TILES; i++) {
        if (dist->bytes_per_tile[i] > max_bytes) {
            max_bytes = dist->bytes_per_tile[i];
            primary = i;
        }
    }
    return primary;
}
