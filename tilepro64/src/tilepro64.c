#include "tilepro64.h"
#include "queue.h"
#include <stdlib.h>
#include <string.h>

tilepro64_system_t g_tilepro64 = { .initialized = false };

void tilepro64_init(void) {
    if (g_tilepro64.initialized) {
        tilepro64_cleanup();
    }

    for (uint16_t i = 0; i < TILEPRO64_NUM_TILES; i++) {
        g_tilepro64.tiles[i].id = i;
        g_tilepro64.tiles[i].x = (uint8_t)(i % TILEPRO64_MESH_WIDTH);
        g_tilepro64.tiles[i].y = (uint8_t)(i / TILEPRO64_MESH_WIDTH);
        g_tilepro64.tiles[i].home_cache_id = i;
        g_tilepro64.tiles[i].l2_bank_size_bytes = TILEPRO64_L2_BANK_KB * 1024;
        g_tilepro64.tiles[i].l1_size_bytes = TILEPRO64_L1_SIZE_KB * 1024;
        g_tilepro64.tiles[i].queue = queue_create(i);
    }

    g_tilepro64.initialized = true;
}

void tilepro64_cleanup(void) {
    if (!g_tilepro64.initialized) return;

    for (uint16_t i = 0; i < TILEPRO64_NUM_TILES; i++) {
        if (g_tilepro64.tiles[i].queue) {
            queue_destroy(g_tilepro64.tiles[i].queue);
            g_tilepro64.tiles[i].queue = NULL;
        }
    }

    g_tilepro64.initialized = false;
}
