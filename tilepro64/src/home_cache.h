#ifndef HOME_CACHE_H
#define HOME_CACHE_H

#include "tilepro64.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint64_t bytes_per_tile[TILEPRO64_NUM_TILES]; // D[64]
    uint64_t total_bytes;
} data_dist_t;

void data_dist_init(data_dist_t* dist);
void data_dist_add(data_dist_t* dist, uint16_t tile_id, uint64_t bytes);
uint64_t data_dist_total(const data_dist_t* dist);
uint16_t data_dist_primary_tile(const data_dist_t* dist);

#endif // HOME_CACHE_H
