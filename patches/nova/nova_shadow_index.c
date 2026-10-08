#include "nova_shadow_index.h"
#include <string.h>

void nova_shadow_init(struct shadow_index_t* idx) {
    memset(idx, 0, sizeof(*idx));
    atomic_init(&idx->head, 0);
}

void nova_shadow_append(struct shadow_index_t* idx, struct mir_task_t* task) {
    if (!idx || !task) return;
    uint32_t slot = atomic_fetch_add_explicit(&idx->head, 1, memory_order_relaxed) % NOVA_SHADOW_K;
    idx->entries[slot].task = task;
    idx->entries[slot].dist_read = task->dist_by_access_type[MIR_DATA_ACCESS_READ];
    idx->entries[slot].generation++;
}

uint32_t nova_shadow_peek(struct shadow_index_t* idx, struct shadow_entry_t* out_candidates, uint32_t max_count) {
    if (!idx || !out_candidates || max_count == 0) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < NOVA_SHADOW_K && count < max_count; i++) {
        struct shadow_entry_t entry = idx->entries[i];
        if (entry.task != NULL && !mir_task_is_taken(entry.task)) {
            out_candidates[count++] = entry;
        }
    }
    return count;
}