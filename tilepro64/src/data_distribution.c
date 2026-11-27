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

void distribute_2d_stencil_1d_striped(data_dist_t* dist, uint16_t tile_x, uint16_t tile_y,
                                      uint64_t block_bytes, uint64_t halo_bytes) {
    if (!dist) return;
    data_dist_init(dist);
    /* 1D linear cache-line striping destroys 2D row/column neighbor locality:
     * both the interior block and the 4 halo edges are interleaved across all 64 tiles,
     * forcing non-local fetches and center-bisection crossings even for adjacent cells. */
    uint64_t total = block_bytes + 4 * halo_bytes;
    uint64_t base = total / TILEPRO64_NUM_TILES;
    uint64_t rem = total % TILEPRO64_NUM_TILES;
    uint16_t phase = (uint16_t)((tile_y * TILEPRO64_MESH_WIDTH + tile_x) % TILEPRO64_NUM_TILES);
    for (uint16_t i = 0; i < TILEPRO64_NUM_TILES; i++) {
        uint16_t target = (uint16_t)((phase + i) % TILEPRO64_NUM_TILES);
        data_dist_add(dist, target, base + (i < rem ? 1 : 0));
    }
}

void distribute_2d_stencil_blocked(data_dist_t* dist, uint16_t tile_x, uint16_t tile_y,
                                   uint64_t block_bytes, uint64_t halo_bytes) {
    if (!dist) return;
    data_dist_init(dist);
    uint16_t center = (uint16_t)(tile_y * TILEPRO64_MESH_WIDTH + tile_x);
    data_dist_add(dist, center, block_bytes);

    /* Preserve 2D neighbor coordinates (1-hop N, S, E, W tiles; boundary halos stay local). */
    uint16_t north = (tile_y > 0) ? (uint16_t)((tile_y - 1) * TILEPRO64_MESH_WIDTH + tile_x) : center;
    uint16_t south = (tile_y + 1 < TILEPRO64_MESH_HEIGHT) ? (uint16_t)((tile_y + 1) * TILEPRO64_MESH_WIDTH + tile_x) : center;
    uint16_t west  = (tile_x > 0) ? (uint16_t)(tile_y * TILEPRO64_MESH_WIDTH + (tile_x - 1)) : center;
    uint16_t east  = (tile_x + 1 < TILEPRO64_MESH_WIDTH) ? (uint16_t)(tile_y * TILEPRO64_MESH_WIDTH + (tile_x + 1)) : center;

    data_dist_add(dist, north, halo_bytes);
    data_dist_add(dist, south, halo_bytes);
    data_dist_add(dist, west, halo_bytes);
    data_dist_add(dist, east, halo_bytes);
}

const char* data_dist_policy_name(data_dist_policy_t pol) {
    switch (pol) {
        case DATA_DIST_FINE: return "fine";
        case DATA_DIST_COARSE: return "coarse";
        case DATA_DIST_1D_STRIPED: return "1d_striped";
        case DATA_DIST_2D_BLOCK: return "2d_block";
        default: return "unknown";
    }
}
