#ifndef TILEPRO64_H
#define TILEPRO64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define TILEPRO64_NUM_TILES   64
#define TILEPRO64_MESH_WIDTH  8
#define TILEPRO64_MESH_HEIGHT 8
#define TILEPRO64_MAX_HOPS    14 // (8-1) + (8-1) = 14
#define TILEPRO64_L2_BANK_KB  64 // 64 KB L2 bank per tile, 4 MB total shared
#define TILEPRO64_L1_SIZE_KB  16 // Private L1 cache per core

// Forward declaration
struct task_queue_t;

typedef struct {
    uint16_t id;                 // 0 .. 63
    uint8_t  x;                  // 0 .. 7
    uint8_t  y;                  // 0 .. 7
    uint16_t home_cache_id;      // Same as tile ID in standard configuration
    uint32_t l2_bank_size_bytes; // 64 * 1024 bytes
    uint32_t l1_size_bytes;      // 16 * 1024 bytes
    struct task_queue_t* queue;  // Task queue associated with this tile
} tile_t;

typedef struct {
    tile_t tiles[TILEPRO64_NUM_TILES];
    bool   initialized;
} tilepro64_system_t;

// Global system accessor
extern tilepro64_system_t g_tilepro64;

void tilepro64_init(void);
void tilepro64_cleanup(void);

static inline tile_t* tile_get(uint16_t id) {
    if (id >= TILEPRO64_NUM_TILES) return NULL;
    return &g_tilepro64.tiles[id];
}

static inline uint16_t tile_distance(uint16_t id1, uint16_t id2) {
    uint8_t x1 = id1 % TILEPRO64_MESH_WIDTH;
    uint8_t y1 = id1 / TILEPRO64_MESH_WIDTH;
    uint8_t x2 = id2 % TILEPRO64_MESH_WIDTH;
    uint8_t y2 = id2 / TILEPRO64_MESH_WIDTH;
    return (uint16_t)((x1 > x2 ? x1 - x2 : x2 - x1) + (y1 > y2 ? y1 - y2 : y2 - y1));
}

static inline bool tile_in_vicinity(uint16_t id1, uint16_t id2, uint16_t vicinity) {
    return tile_distance(id1, id2) <= vicinity;
}

#endif // TILEPRO64_H
